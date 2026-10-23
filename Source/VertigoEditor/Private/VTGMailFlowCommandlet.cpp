#include "VTGMailFlowCommandlet.h"
#include "Engine/DataTable.h"
#include "Engine/Blueprint.h"
#include "Exporters/Exporter.h"
#include "UnrealExporter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "VTGLevelManagerBase.h"
#include "VTGMailReadingComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Message.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "Engine/World.h"
#include "Dialogue.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "LatentActions.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NarrativeUIInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Level.h"
#include "VTGNoteInputTestFixture.h"
#include "NarrativeComponent.h"
#include "DialogueBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/PlayerCameraManager.h"
#include "VTGPlayerAnimationLibrary.h"
#include "Animation/AnimMontage.h"

namespace
{
const UEdGraphSchema_K2* Schema = nullptr;
UEdGraph* Events(UBlueprint* BP)
{
    for (const auto& Graph : BP->UbergraphPages) if (Graph->GetFName() == TEXT("EventGraph")) return Graph.Get();
    return nullptr;
}
template<class T> T* Node(UEdGraph* Graph, const TCHAR* Name)
{
    for (const auto& Item : Graph->Nodes) if (Item->GetFName() == Name) return Cast<T>(Item.Get());
    return nullptr;
}
template<class T> T* Add(UEdGraph* Graph, const TCHAR* Name, int32 X, int32 Y)
{
    auto* Result = NewObject<T>(Graph, FName(Name));
    Graph->AddNode(Result, false, false);
    Result->CreateNewGuid();
    Result->NodePosX = X; Result->NodePosY = Y;
    return Result;
}
UK2Node_CallFunction* Call(UEdGraph* Graph, const TCHAR* Name, UClass* Class, const TCHAR* Function, int32 X, int32 Y)
{
    auto* Result = Add<UK2Node_CallFunction>(Graph, Name, X, Y);
    Result->SetFromFunction(Class->FindFunctionByName(Function));
    Result->AllocateDefaultPins();
    return Result;
}
bool Connect(UEdGraphPin* A, UEdGraphPin* B)
{
    return A && B && Schema->TryCreateConnection(A, B);
}
bool Insert(UEdGraphPin* From, UK2Node_CallFunction* NewCall)
{
    const auto Links = From->LinkedTo;
    From->BreakAllPinLinks();
    if (!Connect(From, NewCall->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
    for (auto* Link : Links) if (!Connect(NewCall->FindPin(UEdGraphSchema_K2::PN_Then), Link)) return false;
    return true;
}
bool InstallMailHold(UBlueprint* Player, UBlueprint* BPLM)
{
    auto* PG = Events(Player); auto* BG = Events(BPLM);
    if (!PG || !BG) return false;
    if (!Node<UK2Node_CustomEvent>(PG, TEXT("VTG_FinishMailReadingCleanup")))
    {
        auto* Original = Node<UK2Node_CustomEvent>(PG, TEXT("K2Node_CustomEvent_30"));
        if (!Original || Original->CustomFunctionName != TEXT("On Finish Reading Mail")) return false;
        auto* Cleanup = Add<UK2Node_CustomEvent>(PG, TEXT("VTG_FinishMailReadingCleanup"), Original->NodePosX, Original->NodePosY + 400);
        Cleanup->CustomFunctionName = TEXT("FinishMailReadingCleanup");
        Cleanup->AllocateDefaultPins();
        auto* Then = Original->FindPin(UEdGraphSchema_K2::PN_Then);
        const auto Links = Then->LinkedTo;
        Then->BreakAllPinLinks();
        for (auto* Link : Links) if (!Connect(Cleanup->FindPin(UEdGraphSchema_K2::PN_Then), Link)) return false;
        auto* Defer = Call(PG, TEXT("VTG_DeferMailFinish"), UVTGMailReadingComponent::StaticClass(), TEXT("DeferMailReadingFinish"), Original->NodePosX + 300, Original->NodePosY);
        Schema->TrySetDefaultValue(*Defer->FindPin(TEXT("CleanupEvent")), TEXT("FinishMailReadingCleanup"));
        auto* Branch = Add<UK2Node_IfThenElse>(PG, TEXT("VTG_MailFinishDeferred"), Original->NodePosX + 620, Original->NodePosY);
        Branch->AllocateDefaultPins();
        // The cleanup call resolves after structural compilation creates the new custom event.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Player);
        auto* Run = Call(PG, TEXT("VTG_RunMailCleanup"), Player->SkeletonGeneratedClass, TEXT("FinishMailReadingCleanup"), Original->NodePosX + 900, Original->NodePosY + 160);
        if (!Connect(Then, Defer->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
            !Connect(Defer->FindPin(UEdGraphSchema_K2::PN_Then), Branch->GetExecPin()) ||
            !Connect(Defer->GetReturnValuePin(), Branch->GetConditionPin()) ||
            !Connect(Branch->GetElsePin(), Run->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
        Defer->NodeComment = TEXT("Story Note owns the mail reading pose until it closes. Cleanup resumes once.");
        Defer->bCommentBubbleVisible = true;
    }
    if (!Node<UK2Node_CallFunction>(BG, TEXT("VTG_BeginStoryNoteMailReading")))
    {
        auto* Open = Node<UK2Node_CustomEvent>(BG, TEXT("K2Node_CustomEvent_900"));
        auto* Finished = Node<UK2Node_CallFunction>(BG, TEXT("K2Node_CallFunction_65"));
        if (!Open || !Finished) return false;
        auto* Begin = Call(BG, TEXT("VTG_BeginStoryNoteMailReading"), AVTGLevelManagerBase::StaticClass(), TEXT("BeginStoryNoteMailReading"), Open->NodePosX + 300, Open->NodePosY - 180);
        if (!Connect(Open->FindPin(TEXT("NoteAsset")), Begin->FindPin(TEXT("NoteAsset"))) || !Insert(Open->FindPin(UEdGraphSchema_K2::PN_Then), Begin)) return false;
        auto* End = Call(BG, TEXT("VTG_EndStoryNoteMailReading"), AVTGLevelManagerBase::StaticClass(), TEXT("EndStoryNoteMailReading"), Finished->NodePosX - 300, Finished->NodePosY - 180);
        const auto Sources = Finished->FindPin(UEdGraphSchema_K2::PN_Execute)->LinkedTo;
        for (auto* Source : Sources) if (!Insert(Source, End)) return false;
    }
    return true;
}

UK2Node_Event* IdleEvent(UBlueprint* BP, UClass* Interface)
{
    if (!BP->ImplementedInterfaces.ContainsByPredicate([Interface](const FBPInterfaceDescription& Item) { return Item.Interface == Interface; }))
        if (!FBlueprintEditorUtils::ImplementNewInterface(BP, FTopLevelAssetPath(Interface))) return nullptr;
    auto* Graph = Events(BP);
    auto* Event = Add<UK2Node_Event>(Graph, TEXT("VTG_SetBaseIdle"), 100, 12000);
    Event->EventReference.SetExternalMember(TEXT("SetBaseIdle"), Interface);
    Event->bOverrideFunction = true;
    Event->AllocateDefaultPins();
    return Event;
}
bool InstallIdle(UBlueprint* Player, UBlueprint* Anim, UClass* Interface)
{
    if (!Node<UK2Node_Event>(Events(Anim), TEXT("VTG_SetBaseIdle")))
    {
        TArray<UEdGraph*> Graphs; Anim->GetAllGraphs(Graphs);
        UAnimGraphNode_SequencePlayer* Idle = nullptr;
        for (auto* Graph : Graphs) if (Graph->GetName() == TEXT("Idle_1"))
            for (const auto& Item : Graph->Nodes) if (auto* Sequence = Cast<UAnimGraphNode_SequencePlayer>(Item.Get()))
                if (Sequence->GetName() == TEXT("AnimGraphNode_SequencePlayer_4")) Idle = Sequence;
        if (!Idle || !Idle->Node.GetSequence()) return false;
        FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Object; Type.PinSubCategoryObject = UAnimSequence::StaticClass();
        if (!FBlueprintEditorUtils::AddMemberVariable(Anim, TEXT("VTGBaseIdle"), Type, Idle->Node.GetSequence()->GetPathName())) return false;
        auto* Event = IdleEvent(Anim, Interface);
        if (!Event) return false;
        auto* Set = Add<UK2Node_VariableSet>(Events(Anim), TEXT("VTG_ApplyBaseIdle"), 450, 12000);
        Set->VariableReference.SetSelfMember(TEXT("VTGBaseIdle")); Set->AllocateDefaultPins();
        if (!Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then), Set->GetExecPin()) || !Connect(Event->FindPin(TEXT("NewIdle")), Set->FindPin(TEXT("VTGBaseIdle")))) return false;
        for (auto& Pin : Idle->ShowPinForProperties) if (Pin.PropertyName == TEXT("Sequence")) Pin.bShowPin = true;
        Idle->ReconstructNode();
        auto* Get = Add<UK2Node_VariableGet>(Idle->GetGraph(), TEXT("VTG_GetBaseIdle"), Idle->NodePosX - 220, Idle->NodePosY + 60);
        Get->VariableReference.SetSelfMember(TEXT("VTGBaseIdle")); Get->AllocateDefaultPins();
        if (!Connect(Get->FindPin(TEXT("VTGBaseIdle")), Idle->FindPin(TEXT("Sequence")))) return false;
    }
    if (!Node<UK2Node_Event>(Events(Player), TEXT("VTG_SetBaseIdle")))
    {
        auto* Event = IdleEvent(Player, Interface);
        if (!Event) return false;
        auto* Graph = Events(Player);
        auto* Mesh = Add<UK2Node_VariableGet>(Graph, TEXT("VTG_IdleMesh"), 100, 12200);
        Mesh->VariableReference.SetSelfMember(TEXT("Mesh")); Mesh->AllocateDefaultPins();
        auto* Instance = Call(Graph, TEXT("VTG_IdleAnimInstance"), USkeletalMeshComponent::StaticClass(), TEXT("GetAnimInstance"), 350, 12200);
        auto* Message = Add<UK2Node_Message>(Graph, TEXT("VTG_ForwardBaseIdle"), 650, 12000);
        Message->FunctionReference.SetExternalMember(TEXT("SetBaseIdle"), Interface); Message->AllocateDefaultPins();
        if (!Connect(Mesh->FindPin(TEXT("Mesh")), Instance->FindPin(UEdGraphSchema_K2::PN_Self)) ||
            !Connect(Instance->GetReturnValuePin(), Message->FindPin(UEdGraphSchema_K2::PN_Self)) ||
            !Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then), Message->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
            !Connect(Event->FindPin(TEXT("NewIdle")), Message->FindPin(TEXT("NewIdle")))) return false;
    }
    return true;
}
bool Backup(UBlueprint* BP)
{
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Copy = FPaths::ProjectSavedDir() / TEXT("Backups/MailStoryFlow") / (BP->GetName() + TEXT("-before-mail-flow.uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true);
    return IFileManager::Get().FileExists(*Copy) || IFileManager::Get().Copy(*Copy, *File) == COPY_OK;
}
bool Save(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors) return false;
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args);
}

bool Verify(UBlueprint* Player, UBlueprint* BPLM, UBlueprint* Sewer, UBlueprint* Anim, UClass* Interface, UDataTable* Table)
{
    bool Passed = true;
    auto Check = [&Passed](bool OK, const TCHAR* What)
    {
        UE_LOG(LogTemp, Display, TEXT("MAIL FLOW CHECK: %s: %s"), What, OK ? TEXT("PASS") : TEXT("FAIL"));
        Passed &= OK;
    };
    Check(Player->GeneratedClass->ImplementsInterface(Interface) && Anim->GeneratedClass->ImplementsInterface(Interface), TEXT("Player and animation class implement SetBaseIdle interface"));
    auto* Character = Cast<ACharacter>(Player->GeneratedClass->GetDefaultObject());
    Check(Character && Character->GetMesh() && Character->GetMesh()->GetAnimClass() == Anim->GeneratedClass, TEXT("Player's actual mesh uses the repaired ABP_Sa"));
    auto* Idle = FindFProperty<FObjectProperty>(Anim->GeneratedClass, TEXT("VTGBaseIdle"));
    Check(Idle && Idle->GetObjectPropertyValue_InContainer(Anim->GeneratedClass->GetDefaultObject()), TEXT("Ordinary standing idle default is preserved"));
    auto* Seat = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/LevelNSequence/In_Level/L3Apartment/ANM/am_LSANM_L3_Sa_SitChairLowerHead.am_LSANM_L3_Sa_SitChairLowerHead"));
    auto* Function = Anim->GeneratedClass->FindFunctionByName(TEXT("SetBaseIdle"));
    auto* NewIdle = Function ? FindFProperty<FObjectProperty>(Function, TEXT("NewIdle")) : nullptr;
    if (Idle && Seat && NewIdle)
    {
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
        auto* Mesh = NewObject<USkeletalMeshComponent>();
        auto* Instance = NewObject<UAnimInstance>(Mesh, Anim->GeneratedClass);
        FStructOnScope Args(Function);
        NewIdle->SetObjectPropertyValue_InContainer(Args.GetStructMemory(), Seat);
        Instance->ProcessEvent(Function, Args.GetStructMemory());
        Check(Idle->GetObjectPropertyValue_InContainer(Instance) == Seat, TEXT("Executing SetBaseIdle changes the animation instance to the stair pose"));
        NewIdle->SetObjectPropertyValue_InContainer(Args.GetStructMemory(), Idle->GetObjectPropertyValue_InContainer(Anim->GeneratedClass->GetDefaultObject()));
        Instance->ProcessEvent(Function, Args.GetStructMemory());
        Check(Idle->GetObjectPropertyValue_InContainer(Instance) != Seat, TEXT("SetBaseIdle can restore ordinary standing idle"));
    }
    else Check(false, TEXT("Idle runtime test prerequisites"));
    auto* Begin = Node<UK2Node_CallFunction>(Events(BPLM), TEXT("VTG_BeginStoryNoteMailReading"));
    auto* Open = Node<UK2Node_CustomEvent>(Events(BPLM), TEXT("K2Node_CustomEvent_900"));
    Check(Begin && Open && Open->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Begin->FindPin(UEdGraphSchema_K2::PN_Execute)), TEXT("Mail hold starts before story Note ends dialogue"));
    auto* End = Node<UK2Node_CallFunction>(Events(BPLM), TEXT("VTG_EndStoryNoteMailReading"));
    Check(End && End->FindPin(UEdGraphSchema_K2::PN_Execute)->LinkedTo.Num() == 1, TEXT("Story Note completion has a cleanup fallback"));
    auto* Defaults = Cast<AVTGLevelManagerBase>(BPLM->GeneratedClass->GetDefaultObject());
    Check(Defaults && Defaults->bKeepMailReadingDuringStoryNote, TEXT("Keep Mail Reading During Story Note setting defaults on"));

    auto* Overdue = Node<UK2Node_CallFunction>(Events(Sewer), TEXT("K2Node_CallFunction_76"));
    const FName Row = Overdue ? FName(*Overdue->FindPin(TEXT("MailID"))->DefaultValue) : NAME_None;
    Check(Row == TEXT("L3_Sewer_Overdue") && Table->GetRowMap().Contains(Row), TEXT("Tab-read overdue mail resolves an existing intended row"));
    const auto* RowData = Table->GetRowMap().Find(Row);
    UDialogueBlueprintGeneratedClass* Dialogue = nullptr; FName ID;
    if (RowData) for (TFieldIterator<FProperty> It(Table->GetRowStruct()); It; ++It)
    {
        if (auto* ClassProperty = CastField<FClassProperty>(*It); ClassProperty && It->GetName().StartsWith(TEXT("Dialogue_")))
            Dialogue = Cast<UDialogueBlueprintGeneratedClass>(ClassProperty->GetObjectPropertyValue_InContainer(*RowData));
        if (auto* NameProperty = CastField<FNameProperty>(*It); NameProperty && It->GetName().StartsWith(TEXT("DialogueID_")))
            ID = NameProperty->GetPropertyValue_InContainer(*RowData);
    }
    bool FoundNode = false;
    if (Dialogue && Dialogue->GetDialogueTemplate())
        for (auto* DialogueNode : Dialogue->GetDialogueTemplate()->GetNodes()) if (DialogueNode && DialogueNode->GetID() == ID) FoundNode = true;
    Check(FoundNode, TEXT("Overdue table row targets an existing Narrative node"));

    // Run the saved player's real finish entry point, replacing only the terminal/camera
    // continuation in a transient copy so this check cannot advance story or alter assets.
    auto* Probe = DuplicateObject<UBlueprint>(Player, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("MailFinishProbe")));
    auto* Graph = Events(Probe);
    auto* Cleanup = Node<UK2Node_CustomEvent>(Graph, TEXT("VTG_FinishMailReadingCleanup"));
    if (!Cleanup) return false;
    auto* Marker = Call(Graph, TEXT("VTG_TestMailCleanupMarker"), AActor::StaticClass(), TEXT("SetActorTickEnabled"), 100, 100);
    Schema->TrySetDefaultValue(*Marker->FindPin(TEXT("bEnabled")), TEXT("true"));
    Cleanup->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    if (!Connect(Cleanup->FindPin(UEdGraphSchema_K2::PN_Then), Marker->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Probe);
    FCompilerResultsLog Results; FKismetEditorUtilities::CompileBlueprint(Probe, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    FActorSpawnParameters Spawn; Spawn.bDeferConstruction = true; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Reader = World->SpawnActor<AActor>(Probe->GeneratedClass, FTransform::Identity, Spawn);
    if (Reader)
    {
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
        Reader->PrimaryActorTick.bCanEverTick = true;
        Reader->SetActorTickEnabled(false);
        auto* Mail = UVTGMailReadingComponent::GetOrCreate(Reader);
        auto* Source = NewObject<USceneComponent>(); auto* Note = NewObject<USceneComponent>();
        Mail->SetHold(Source, true);
        auto* Finish = Reader->FindFunction(TEXT("On Finish Reading Mail"));
        if (!Finish) return false;
        Reader->ProcessEvent(Finish, nullptr);
        Check(!Reader->IsActorTickEnabled(), TEXT("Saved player finish event defers terminal cleanup during Note opening"));
        Mail->NoteOpened(Note); Mail->NoteClosed(Note);
        Check(Reader->IsActorTickEnabled(), TEXT("Saved player finish continuation runs on logical Note close"));
        Reader->SetActorTickEnabled(false);
        Reader->ProcessEvent(Finish, nullptr);
        Check(Reader->IsActorTickEnabled(), TEXT("Saved player finishes ordinary mail immediately"));
    }
    else Check(false, TEXT("Player runtime probe spawned"));
    World->DestroyWorld(false);
    return Passed;
}

bool CanLeave(UBlueprint* Sewer, UBlueprint* DialogueBP, bool VerifyOnly)
{
    auto* SourceDialogue = Cast<UDialogueBlueprint>(DialogueBP);
    UDialogueNode* FinalNode = nullptr;
    if (SourceDialogue && SourceDialogue->DialogueTemplate)
        for (auto* Item : SourceDialogue->DialogueTemplate->GetNodes())
            if (Item && Item->GetID() == TEXT("L3_SaApartmentD1_DialogueNode_NPC_70")) FinalNode = Item;
    if (!FinalNode) return false;
    auto* Graph = Events(Sewer);
    auto* Delay = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_42"));
    auto* Restore = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_34"));
    auto* Release = Node<UK2Node_CallFunction>(Graph, TEXT("VTG_CanLeaveReleaseCamera"));
    auto* DG = Events(DialogueBP);
    auto* Event = Node<UK2Node_CustomEvent>(DG, TEXT("K2Node_CustomEvent_12"));
    auto* GetManager = Node<UK2Node_CallFunction>(DG, TEXT("K2Node_CallFunction_20"));
    auto* Finished = Node<UK2Node_IfThenElse>(DG, TEXT("VTG_CanLeaveOnNodeFinished"));
    if (!Delay || !Restore || !Event || !GetManager) return false;
    if (!VerifyOnly)
    {
        FinalNode->bCallBoundEventOnFinish = true;
        for (auto* BP : {Sewer, DialogueBP})
        {
            const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
            const FString Copy = FPaths::ProjectSavedDir() / TEXT("Backups/CanLeaveFinishCallback") / (BP->GetName() + TEXT("-before-finish-callback.uasset"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true);
            if (!IFileManager::Get().FileExists(*Copy) && IFileManager::Get().Copy(*Copy, *File) != COPY_OK) return false;
        }
        if (!Release)
        {
            Release = Call(Graph, TEXT("VTG_CanLeaveReleaseCamera"), Sewer->ParentClass, TEXT("ReleaseCameraLock"), Restore->NodePosX - 40, Restore->NodePosY - 240);
            Release->NodeComment = TEXT("Release the BPLM camera's input lock, cinematic flag and collision override before returning gameplay.");
            Release->bCommentBubbleVisible = true;
            if (!Insert(Delay->FindPin(UEdGraphSchema_K2::PN_Then), Release)) return false;
        }
        if (!Finished)
        {
            Finished = Add<UK2Node_IfThenElse>(DG, TEXT("VTG_CanLeaveOnNodeFinished"), Event->NodePosX + 260, Event->NodePosY);
            Finished->AllocateDefaultPins();
            Event->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
            if (!Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then), Finished->GetExecPin()) ||
                !Connect(Event->FindPin(TEXT("bStarted")), Finished->GetConditionPin()) ||
                !Connect(Finished->GetElsePin(), GetManager->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
            Finished->NodeComment = TEXT("Leave after the final line finishes, not while the node is starting.");
            Finished->bCommentBubbleVisible = true;
        }
        Event->NodeComment = TEXT("FINISH ONLY: NPC_70 explicitly enables Run Bound Event On Finish. All other bound dialogue events default to START. Release the stair camera after the final line ends.");
        Event->bCommentBubbleVisible = true;
        if (!Save(Sewer) || !Save(DialogueBP)) return false;
    }
    if (!FinalNode->bCallBoundEventOnFinish || !Release || !Finished ||
        !Delay->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Release->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
        !Release->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Restore->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
        Finished->GetThenPin()->LinkedTo.Num() ||
        !Finished->GetConditionPin()->LinkedTo.Contains(Event->FindPin(TEXT("bStarted"))) ||
        !Finished->GetElsePin()->LinkedTo.Contains(GetManager->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
    UE_LOG(LogTemp, Display, TEXT("CAN LEAVE: verified camera ownership release and final-line finish gate."));
    return true;
}

bool TestCanLeave(UBlueprint* Sewer)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto* PlayerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
    auto* ControllerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/PC_Game_VTG.PC_Game_VTG"));
    if (!PlayerBP || !ControllerBP) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Spawn; Spawn.bDeferConstruction = true; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<ACharacter>(PlayerBP->GeneratedClass, FTransform::Identity, Spawn);
    auto* Manager = World->SpawnActor<AActor>(Sewer->GeneratedClass, FTransform::Identity, Spawn);
    auto* Controller = World->SpawnActor<APlayerController>(ControllerBP->GeneratedClass);
    auto* PreviousCamera = World->SpawnActor<AActor>();
    if (!Player || !Manager || !Controller) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    auto* State = World->SpawnActor<AGameStateBase>();
    World->SetGameState(State);
    auto* PlayerState = World->SpawnActor<APlayerState>();
    PlayerState->SetOwner(Controller);
    Controller->PlayerState = PlayerState;
    State->AddPlayerState(PlayerState);
    Controller->Possess(Player);
    // This isolated world deliberately never runs InitializeActorsForPlay/BeginPlay.
    // Create the controller's real camera manager explicitly for view-target assertions.
    if (!Controller->PlayerCameraManager)
    {
        FActorSpawnParameters CameraSpawn; CameraSpawn.Owner = Controller;
        UClass* CameraClass = Controller->PlayerCameraManagerClass ? Controller->PlayerCameraManagerClass.Get() : APlayerCameraManager::StaticClass();
        Controller->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(CameraClass, CameraSpawn);
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->InitializeFor(Controller);
    }
    auto* ActiveProperty = FindFProperty<FObjectProperty>(Manager->GetClass(), TEXT("ActiveCameraPoint"));
    auto* CameraPoint = ActiveProperty ? World->SpawnActor<AActor>(ActiveProperty->PropertyClass, FTransform::Identity, Spawn) : nullptr;
    Controller->SetViewTarget(CameraPoint);
    auto SetObject = [](UObject* Object, const TCHAR* Name, UObject* Value)
    {
        auto* Property = FindFProperty<FObjectProperty>(Object->GetClass(), Name);
        if (Property) Property->SetObjectPropertyValue_InContainer(Object, Value);
        return Property != nullptr;
    };
    auto SetBool = [](UObject* Object, const TCHAR* Name, bool Value)
    {
        auto* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name);
        if (Property) Property->SetPropertyValue_InContainer(Object, Value);
        return Property != nullptr;
    };
    bool Passed = CameraPoint && SetObject(Manager, TEXT("ActiveCameraPoint"), CameraPoint) && SetObject(Manager, TEXT("Player Pawn"), Player) && SetObject(Manager, TEXT("PreviousViewTarget"), PreviousCamera) && SetObject(Manager, TEXT("Player Controller"), Controller);
    Passed &= SetBool(Controller, TEXT("Use Third Person Camera?"), false);
    auto* Cinematic = FindFProperty<FBoolProperty>(Player->GetClass(), TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"));
    auto* Lock = Manager->FindFunction(TEXT("LockPlayerInput"));
    auto* Leave = Manager->FindFunction(TEXT("L3_CanLeave"));
    if (!Cinematic || !Lock || !Leave) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    auto Check = [&Passed](bool OK, const TCHAR* What)
    {
        UE_LOG(LogTemp, Display, TEXT("CAN LEAVE TEST: %s: %s"), What, OK ? TEXT("PASS") : TEXT("FAIL"));
        Passed &= OK;
    };
    // Reproduce the camera-owned part of the stair sequence with its actual Blueprint lock.
    Manager->ProcessEvent(Lock, nullptr);
    SetBool(Manager, TEXT("bCameraLocked"), true);
    SetBool(Manager, TEXT("bHasCameraLockUsedInputLock"), true);
    Check(Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored() && Cinematic->GetPropertyValue_InContainer(Player) && !Player->GetActorEnableCollision(), TEXT("Stair camera lock reproduces blocked gameplay"));
    // Use an empty component so the saved TryExitDialogue is exercised without advancing story.
    // The saved exit node safely does nothing with a valid component and no current dialogue.
    auto* NarrativeProperty = FindFProperty<FObjectProperty>(Manager->GetClass(), TEXT("Narrative"));
    auto* Narrative = NarrativeProperty ? NewObject<UActorComponent>(Manager, NarrativeProperty->PropertyClass) : nullptr;
    if (!Narrative || !SetObject(Manager, TEXT("Narrative"), Narrative)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    Narrative->RegisterComponent();
    // Dispatch the actual saved NPC_70 callback through Narrative's real start/finish filter.
    // Previously calling L3_CanLeave directly skipped the broken caller entirely.
    auto* DialogueBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1.L3_SaApartmentD1"));
    auto* Probe = DuplicateObject<UBlueprint>(DialogueBP, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("CanLeaveDispatchProbe")));
    Probe->ParentClass = UVTGNoteInputTestDialogue::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(Probe);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Probe);
    FCompilerResultsLog CompileResults;
    FKismetEditorUtilities::CompileBlueprint(Probe, EBlueprintCompileOptions::None, &CompileResults);
    if (CompileResults.NumErrors) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    auto* Instance = NewObject<UVTGNoteInputTestDialogue>(Narrative, Probe->GeneratedClass);
    auto* TemplateClass = Cast<UDialogueBlueprintGeneratedClass>(DialogueBP->GeneratedClass);
    UDialogueNode* FinalNode = nullptr;
    for (auto* Item : TemplateClass->GetDialogueTemplate()->GetNodes())
        if (Item && Item->GetID() == TEXT("L3_SaApartmentD1_DialogueNode_NPC_70")) FinalNode = Item;
    if (!FinalNode) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    auto Advance = [World, Manager]()
    {
        for (int32 I = 0; I < 5; ++I)
        {
            World->GetLatentActionManager().BeginFrame();
            World->GetLatentActionManager().ProcessLatentActions(Manager, 0.25f);
        }
    };
    Instance->DispatchNodeForTest(Cast<UNarrativeComponent>(Narrative), FinalNode, true);
    Advance();
    Check(Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored(), TEXT("Real NPC_70 start callback keeps the camera locked during the final line"));
    {
        TGuardValue<bool> LegacyDispatch(FinalNode->bCallBoundEventOnFinish, false);
        Instance->DispatchNodeForTest(Cast<UNarrativeComponent>(Narrative), FinalNode, false);
        Advance();
        Check(Controller->GetViewTarget() == CameraPoint && Controller->IsMoveInputIgnored(), TEXT("Reproduced: start-only dispatcher cannot reach the finish-only Can Leave branch"));
    }
    Instance->DispatchNodeForTest(Cast<UNarrativeComponent>(Narrative), FinalNode, false);
    Advance();
    Check(Controller->GetViewTarget() == Player, TEXT("Actual PC_Game_VTG camera returns from the stair point to the player"));
    Check(!Controller->IsMoveInputIgnored() && !Controller->IsLookInputIgnored(), TEXT("Can Leave balances camera move/look locks"));
    Check(!Cinematic->GetPropertyValue_InContainer(Player) && Player->GetActorEnableCollision(), TEXT("Can Leave clears cinematic flag and restores collision"));
    Check(Player->InputEnabled() && Player->GetCharacterMovement()->MovementMode == MOVE_Walking, TEXT("Pawn input and Walking are restored"));
    Manager->ProcessEvent(Leave, nullptr);
    Advance();
    Check(!Controller->IsMoveInputIgnored() && !Controller->IsLookInputIgnored(), TEXT("Repeated Can Leave is harmless"));
    Manager->ProcessEvent(Lock, nullptr);
    SetBool(Manager, TEXT("bCameraLocked"), true);
    SetBool(Manager, TEXT("bHasCameraLockUsedInputLock"), true);
    SetObject(Manager, TEXT("PreviousViewTarget"), PreviousCamera);
    SetObject(Manager, TEXT("ActiveCameraPoint"), CameraPoint);
    auto* Input = UNarrativeUIInputComponent::GetOrCreate(Controller);
    auto* NoteOwner = NewObject<USceneComponent>();
    if (Input)
    {
        Input->Acquire(NoteOwner);
        Manager->ProcessEvent(Leave, nullptr);
        Advance();
        Check(Input->HasOwners() && Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored(), TEXT("Camera release preserves an independent Note/dialogue lock"));
        Input->Release(NoteOwner);
        Check(!Controller->IsMoveInputIgnored() && !Controller->IsLookInputIgnored(), TEXT("Final UI owner release leaves no stale input lock"));
    }
    else Check(false, TEXT("Independent UI input owner available"));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return Passed;
}

bool MailCamera(UBlueprint* Player, bool VerifyOnly)
{
    auto* Graph = Events(Player);
    auto* Restore = Node<UEdGraphNode>(Graph, TEXT("K2Node_MacroInstance_14"));
    auto* Cleanup = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_215"));
    auto* Guard = Node<UK2Node_IfThenElse>(Graph, TEXT("VTG_MailCameraStillOwned"));
    if (!Restore || !Cleanup) return false;
    if (!Guard && !VerifyOnly)
    {
        const FString File = FPackageName::LongPackageNameToFilename(Player->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Copy = FPaths::ProjectSavedDir() / TEXT("Backups/MailCameraOwnership/BP_Player_Sa-before-view-target-guard.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true);
        if (!IFileManager::Get().FileExists(*Copy) && IFileManager::Get().Copy(*Copy, *File) != COPY_OK) return false;
        auto* Owned = Call(Graph, TEXT("VTG_CanRestoreMailViewTarget"), UVTGMailReadingComponent::StaticClass(), TEXT("CanRestoreMailViewTarget"), Restore->NodePosX - 300, Restore->NodePosY + 140);
        Guard = Add<UK2Node_IfThenElse>(Graph, TEXT("VTG_MailCameraStillOwned"), Restore->NodePosX - 300, Restore->NodePosY);
        Guard->AllocateDefaultPins();
        auto* Input = Restore->FindPin(TEXT("exec"));
        if (!Input) return false;
        const auto Sources = Input->LinkedTo;
        Input->BreakAllPinLinks();
        for (auto* Source : Sources) if (!Connect(Source, Guard->GetExecPin())) return false;
        if (!Connect(Owned->GetReturnValuePin(), Guard->GetConditionPin()) ||
            !Connect(Guard->GetThenPin(), Input) ||
            !Connect(Guard->GetElsePin(), Cleanup->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
        Guard->NodeComment = TEXT("Terminal cleanup must not steal the view target from a later locked camera or dialogue. Always finish mail component cleanup.");
        Guard->bCommentBubbleVisible = true;
        if (!Save(Player)) return false;
    }
    return Guard && Guard->GetThenPin()->LinkedTo.Contains(Restore->FindPin(TEXT("exec"))) &&
        Guard->GetElsePin()->LinkedTo.Contains(Cleanup->FindPin(UEdGraphSchema_K2::PN_Execute));
}

bool StairIdle(UBlueprint* Sewer, bool VerifyOnly)
{
    auto* Graph = Events(Sewer);
    auto* Apply = Node<UK2Node_CallFunction>(Graph, TEXT("VTG_SetStairBaseIdle"));
    if (!Apply && !VerifyOnly)
    {
        auto* Old = Node<UK2Node_Message>(Graph, TEXT("K2Node_Message_2"));
        if (!Old || !Old->FindPin(TEXT("NewIdle"))->DefaultObject) return false;
        const FString File = FPackageName::LongPackageNameToFilename(Sewer->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Copy = FPaths::ProjectSavedDir() / TEXT("Backups/InstantStairIdle/BPLM_SewerUnderApt-before-instant-idle.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true);
        if (!IFileManager::Get().FileExists(*Copy) && IFileManager::Get().Copy(*Copy, *File) != COPY_OK) return false;
        Apply = Call(Graph, TEXT("VTG_SetStairBaseIdle"), UVTGPlayerAnimationLibrary::StaticClass(), TEXT("SetPlayerBaseIdle"), Old->NodePosX, Old->NodePosY);
        for (const auto& Pair : {TPair<FName,FName>(UEdGraphSchema_K2::PN_Execute, UEdGraphSchema_K2::PN_Execute), TPair<FName,FName>(UEdGraphSchema_K2::PN_Then, UEdGraphSchema_K2::PN_Then), TPair<FName,FName>(UEdGraphSchema_K2::PN_Self, TEXT("Player")), TPair<FName,FName>(TEXT("NewIdle"), TEXT("NewIdle"))})
        {
            auto* From = Old->FindPin(Pair.Key); auto* To = Apply->FindPin(Pair.Value);
            if (!From || !To) return false;
            To->DefaultValue = From->DefaultValue; To->DefaultObject = From->DefaultObject; To->DefaultTextValue = From->DefaultTextValue;
            const auto Links = From->LinkedTo; From->BreakAllPinLinks();
            for (auto* Link : Links) if (!Connect(Link, To)) return false;
        }
        Schema->TrySetDefaultValue(*Apply->FindPin(TEXT("Instant")), TEXT("true"));
        Apply->NodeComment = TEXT("Instant is opt-in: interrupt the previous terminal montage and evaluate the new idle immediately. False preserves ordinary montage/blend behavior.");
        Apply->bCommentBubbleVisible = true;
        FBlueprintEditorUtils::RemoveNode(Sewer, Old, true);
        if (!Save(Sewer)) return false;
    }
    return Apply && Apply->FunctionReference.GetMemberName() == TEXT("SetPlayerBaseIdle") && Apply->FindPin(TEXT("Instant"))->DefaultValue == TEXT("true") && Apply->FindPin(TEXT("Player"))->LinkedTo.Num() == 1;
}

bool TestStairIdle(UBlueprint* Sewer)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
    auto* Anim = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Characters/Sa/ABP_Sa.ABP_Sa"));
    auto* Apply = Node<UK2Node_CallFunction>(Events(Sewer), TEXT("VTG_SetStairBaseIdle"));
    auto* Seat = Apply ? Cast<UAnimSequence>(Apply->FindPin(TEXT("NewIdle"))->DefaultObject) : nullptr;
    if (!BP || !Anim || !Seat) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform::Identity, Spawn);
    if (!Player) { World->DestroyWorld(false); return false; }
    auto* Mesh = Player->GetMesh();
    Mesh->SetAnimInstanceClass(Anim->GeneratedClass);
    Mesh->InitAnim(true);
    auto* Instance = Mesh->GetAnimInstance();
    auto* CloseProperty = FindFProperty<FObjectProperty>(Player->GetClass(), TEXT("Mont_CloseTerminal"));
    auto* Close = CloseProperty ? Cast<UAnimMontage>(CloseProperty->GetObjectPropertyValue_InContainer(Player)) : nullptr;
    auto* Idle = Instance ? FindFProperty<FObjectProperty>(Instance->GetClass(), TEXT("VTGBaseIdle")) : nullptr;
    bool Passed = Instance && Close && Idle;
    auto Check = [&Passed](bool OK, const TCHAR* What)
    {
        UE_LOG(LogTemp, Display, TEXT("STAIR IDLE TEST: %s: %s"), What, OK ? TEXT("PASS") : TEXT("FAIL"));
        Passed &= OK;
    };
    if (Passed)
    {
        UE_LOG(LogTemp, Display, TEXT("STAIR IDLE: close montage=%s length=%.3f blendout=%.3f; seat=%s"), *Close->GetPathName(), Close->GetPlayLength(), Close->BlendOut.GetBlendTime(), *Seat->GetPathName());
        Instance->Montage_Play(Close);
        Check(UVTGPlayerAnimationLibrary::SetPlayerBaseIdle(Player, Seat, false) && Instance->Montage_IsPlaying(Close), TEXT("Default switch preserves the playing terminal montage"));
        Check(Idle->GetObjectPropertyValue_InContainer(Instance) == Seat, TEXT("Base idle already changes even while the montage covers it"));
        Mesh->TickAnimation(.1f, false);
        Mesh->RefreshBoneTransforms();
        const TArray<FTransform> BeforePose = Mesh->GetComponentSpaceTransforms();
        Check(UVTGPlayerAnimationLibrary::SetPlayerBaseIdle(Player, Seat, true), TEXT("Instant switch succeeds on actual Sa"));
        Check(!Instance->IsAnyMontagePlaying() && Idle->GetObjectPropertyValue_InContainer(Instance) == Seat, TEXT("Instant switch removes the montage without waiting for its blend-out"));
        Check(Mesh->GetAnimClass() == Anim->GeneratedClass, TEXT("Instant switch preserves the locomotion animation class"));
        const auto& AfterPose = Mesh->GetComponentSpaceTransforms();
        bool PoseChangedNow = false;
        for (int32 Index = 0; Index < FMath::Min(BeforePose.Num(), AfterPose.Num()); ++Index)
            PoseChangedNow |= !BeforePose[Index].Equals(AfterPose[Index], .001f);
        Check(PoseChangedNow, TEXT("Evaluated bone pose changes inside the instant call without waiting another frame"));
    }
    World->DestroyWorld(false);
    return Passed;
}

bool TestMailCamera(UBlueprint* Saved)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto* Probe = DuplicateObject<UBlueprint>(Saved, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("MailCameraProbe")));
    auto* Graph = Events(Probe);
    auto* Guard = Node<UK2Node_IfThenElse>(Graph, TEXT("VTG_MailCameraStillOwned"));
    auto* OldRestore = Node<UEdGraphNode>(Graph, TEXT("K2Node_MacroInstance_14"));
    if (!Guard || !OldRestore) return false;
    for (const bool Legacy : {true, false})
    {
        auto* Event = Add<UK2Node_CustomEvent>(Graph, Legacy ? TEXT("VTG_LegacyCameraReturnTest") : TEXT("VTG_GuardedCameraReturnTest"), 100, Legacy ? 100 : 400);
        Event->CustomFunctionName = Legacy ? TEXT("LegacyCameraReturnForTest") : TEXT("GuardedCameraReturnForTest");
        Event->AllocateDefaultPins();
        Event->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Legacy ? OldRestore->FindPin(TEXT("exec")) : Guard->GetExecPin());
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Probe);
    FCompilerResultsLog Results; FKismetEditorUtilities::CompileBlueprint(Probe, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Reader = World->SpawnActor<ACharacter>(Probe->GeneratedClass, FTransform::Identity, Spawn);
    auto* ControllerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/PC_Game_VTG.PC_Game_VTG"));
    auto* LockBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_0CameraLockPoint.BP_0CameraLockPoint"));
    auto* Controller = ControllerBP ? World->SpawnActor<APlayerController>(ControllerBP->GeneratedClass) : nullptr;
    auto* Lock = LockBP ? World->SpawnActor<AActor>(LockBP->GeneratedClass, FTransform(FVector(-900, 0, 200)), Spawn) : nullptr;
    if (!Reader || !Controller || !Lock) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
    auto* State = World->SpawnActor<AGameStateBase>(); World->SetGameState(State);
    auto* PlayerState = World->SpawnActor<APlayerState>(); PlayerState->SetOwner(Controller); Controller->PlayerState = PlayerState; State->AddPlayerState(PlayerState);
    Controller->Possess(Reader);
    if (!Controller->PlayerCameraManager)
    {
        FActorSpawnParameters CameraSpawn; CameraSpawn.Owner = Controller;
        Controller->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(APlayerCameraManager::StaticClass(), CameraSpawn);
        Controller->PlayerCameraManager->InitializeFor(Controller);
    }
    auto* FoundCamera = FindFProperty<FObjectProperty>(Reader->GetClass(), TEXT("FoundActiveCamera"));
    auto* MailCameraIn = FindFProperty<FBoolProperty>(Reader->GetClass(), TEXT("MailCamAlreadyIn?"));
    auto* Legacy = Reader->FindFunction(TEXT("LegacyCameraReturnForTest"));
    auto* Guarded = Reader->FindFunction(TEXT("GuardedCameraReturnForTest"));
    bool Passed = FoundCamera && MailCameraIn && Legacy && Guarded;
    auto Check = [&Passed](bool OK, const TCHAR* What)
    {
        UE_LOG(LogTemp, Display, TEXT("MAIL CAMERA TEST: %s: %s"), What, OK ? TEXT("PASS") : TEXT("FAIL"));
        Passed &= OK;
    };
    if (Passed)
    {
        FoundCamera->SetObjectPropertyValue_InContainer(Reader, nullptr);
        const FTransform FixedTransform = Lock->GetActorTransform();
        Controller->SetViewTarget(Lock);
        Reader->ProcessEvent(Legacy, nullptr);
        Check(Controller->GetViewTarget() == Reader && Lock->GetActorTransform().Equals(FixedTransform), TEXT("Reproduced: old delayed mail return steals the view while Lock Cam itself never moves"));
        Controller->SetViewTarget(Lock);
        MailCameraIn->SetPropertyValue_InContainer(Reader, true);
        Reader->ProcessEvent(Guarded, nullptr);
        Check(Controller->GetViewTarget() == Lock && Lock->GetActorTransform().Equals(FixedTransform), TEXT("Guarded cleanup preserves the later stair camera"));
        Check(!MailCameraIn->GetPropertyValue_InContainer(Reader), TEXT("Mail camera state still cleans up after skipping the stale view-target change"));
        auto* OtherCamera = World->SpawnActor<AActor>(FoundCamera->PropertyClass, FTransform::Identity, Spawn);
        FoundCamera->SetObjectPropertyValue_InContainer(Reader, OtherCamera);
        Controller->SetViewTarget(Lock);
        Reader->ProcessEvent(Guarded, nullptr);
        Check(Controller->GetViewTarget() == Lock, TEXT("An older activated fixed camera also cannot steal the stair view"));
        Controller->SetViewTarget(Reader);
        Reader->ProcessEvent(Guarded, nullptr);
        Check(OtherCamera && Controller->GetViewTarget() == OtherCamera, TEXT("Ordinary mail still restores its activated fixed camera"));
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return Passed;
}
}

UVTGMailFlowCommandlet::UVTGMailFlowCommandlet()
{
    IsEditor = true;
    IsClient = IsServer = false;
    LogToConsole = true;
}

namespace
{
bool NoteMailCameraSessions(bool VerifyOnly)
{
    auto* Player = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
    auto* Apartment = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment.BPLM_SaApartment"));
    if (!Player || !Apartment) return false;
    if (!VerifyOnly)
        for (auto* BP : {Player, Apartment})
        {
            const FString Backup = FPaths::ProjectSavedDir()/TEXT("Backups/NoteMailCameraSession")/(BP->GetName()+TEXT("-before-session-guard.uasset"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
            if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup,
                *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension())) != COPY_OK) return false;
        }
    auto* PG = Events(Player); auto* AG = Events(Apartment);
    auto InstallStart = [&](UEdGraph* Graph, UEdGraphPin* From, UEdGraphPin* Reader, const TCHAR* Name)
    {
        auto* Start = Node<UK2Node_CallFunction>(Graph, Name);
        if (!Start && !VerifyOnly)
        {
            Start = Call(Graph, Name, UVTGMailReadingComponent::StaticClass(), TEXT("NotifyMailReadingStarted"), From->GetOwningNode()->NodePosX + 200, From->GetOwningNode()->NodePosY - 180);
            if (Reader && !Connect(Reader, Start->FindPin(TEXT("Reader")))) return false;
            if (!Insert(From, Start)) return false;
        }
        return Start && From->LinkedTo.Contains(Start->FindPin(UEdGraphSchema_K2::PN_Execute));
    };
    auto* Read = Node<UK2Node_CustomEvent>(PG, TEXT("K2Node_CustomEvent_29"));
    auto* NextDialogueDelay = Node<UK2Node_CallFunction>(AG, TEXT("K2Node_CallFunction_36"));
    auto* Pawn = Node<UK2Node_VariableGet>(AG, TEXT("K2Node_VariableGet_1"));
    if (!Read || Read->CustomFunctionName != TEXT("ReadMailSafe") || !NextDialogueDelay || !Pawn) return false;
    if (!InstallStart(PG, Read->FindPin(UEdGraphSchema_K2::PN_Then), nullptr, TEXT("VTG_BeginMailCameraSession"))) return false;
    auto* Begin = Node<UK2Node_CallFunction>(AG, TEXT("K2Node_CallFunction_74"));
    auto* Resume = Node<UK2Node_CallFunction>(AG, TEXT("VTG_ResumeNoteMailCameraSession"));
    auto* Success = Node<UK2Node_IfThenElse>(AG, TEXT("VTG_NoteMailDialogueAccepted"));
    if (!Begin || Begin->FunctionReference.GetMemberName()!=TEXT("BeginDialogue")) return false;
    if (!Success && !VerifyOnly)
    {
        if (!Resume) Resume=Call(AG,TEXT("VTG_ResumeNoteMailCameraSession"),UVTGMailReadingComponent::StaticClass(),TEXT("NotifyNoteMailDialogueStarted"),Begin->NodePosX+550,Begin->NodePosY);
        // Upgrade the initial experimental pre-call hook; rejection must not cancel valid old cleanup.
        if (NextDialogueDelay->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Resume->FindPin(UEdGraphSchema_K2::PN_Execute)))
        {
            NextDialogueDelay->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
            Resume->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
            if (!Connect(NextDialogueDelay->FindPin(UEdGraphSchema_K2::PN_Then),Begin->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
        }
        if (Begin->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num()) return false;
        if (!Connect(Pawn->FindPin(TEXT("Player Pawn")),Resume->FindPin(TEXT("Reader")))) return false;
        Success=Add<UK2Node_IfThenElse>(AG,TEXT("VTG_NoteMailDialogueAccepted"),Begin->NodePosX+280,Begin->NodePosY);Success->AllocateDefaultPins();
        if (!Connect(Begin->FindPin(UEdGraphSchema_K2::PN_Then),Success->GetExecPin()) ||
            !Connect(Begin->GetReturnValuePin(),Success->GetConditionPin()) ||
            !Connect(Success->GetThenPin(),Resume->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
    }
    if (Resume && !VerifyOnly)
        Resume->SetFromFunction(UVTGMailReadingComponent::StaticClass()->FindFunctionByName(TEXT("NotifyNoteMailDialogueStarted")));
    if (!Resume || Resume->FunctionReference.GetMemberName()!=TEXT("NotifyNoteMailDialogueStarted") || !Success || !Success->GetConditionPin()->LinkedTo.Contains(Begin->GetReturnValuePin()) ||
        !Success->GetThenPin()->LinkedTo.Contains(Resume->FindPin(UEdGraphSchema_K2::PN_Execute)) || Success->GetElsePin()->LinkedTo.Num()) return false;
    const TCHAR* Delays[] = {TEXT("K2Node_CallFunction_282"), TEXT("K2Node_CallFunction_280"), TEXT("K2Node_CallFunction_276")};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Delay = Node<UK2Node_CallFunction>(PG, Delays[Index]);
        if (!Delay || Delay->FunctionReference.GetMemberName() != TEXT("Delay")) return false;
        const FString Prefix = FString::Printf(TEXT("VTG_NoteMailStep%d"), Index);
        const FName EventName(*(Prefix + TEXT("Continue")));
        auto* Schedule = Node<UK2Node_CallFunction>(PG, *(Prefix + TEXT("Schedule")));
        if (!Schedule && !VerifyOnly)
        {
            Schedule = Call(PG, *(Prefix + TEXT("Schedule")), UVTGMailReadingComponent::StaticClass(), TEXT("ScheduleNoteMailCleanupStep"), Delay->NodePosX - 300, Delay->NodePosY - 180);
            Schema->TrySetDefaultValue(*Schedule->FindPin(TEXT("Delay")), Delay->FindPin(TEXT("Duration"))->DefaultValue);
            Schema->TrySetDefaultValue(*Schedule->FindPin(TEXT("Continuation")), EventName.ToString());
            Schema->TrySetDefaultValue(*Schedule->FindPin(TEXT("bFinalStep")), Index == 2 ? TEXT("true") : TEXT("false"));
            auto* Branch = Add<UK2Node_IfThenElse>(PG, *(Prefix + TEXT("Protected")), Delay->NodePosX, Delay->NodePosY - 180); Branch->AllocateDefaultPins();
            auto* Event = Add<UK2Node_CustomEvent>(PG, *EventName.ToString(), Delay->NodePosX + 200, Delay->NodePosY - 360);
            Event->CustomFunctionName = EventName; Event->AllocateDefaultPins();
            for (auto* Link : Delay->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo)
                if (!Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then), Link)) return false;
            auto* Exec = Delay->FindPin(UEdGraphSchema_K2::PN_Execute);
            const auto Sources = Exec->LinkedTo; Exec->BreakAllPinLinks();
            for (auto* Source : Sources) if (!Connect(Source, Schedule->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
            if (!Connect(Schedule->FindPin(UEdGraphSchema_K2::PN_Then), Branch->GetExecPin()) ||
                !Connect(Schedule->GetReturnValuePin(), Branch->GetConditionPin()) || !Connect(Branch->GetElsePin(), Exec)) return false;
            Schedule->NodeComment = TEXT("Only deferred mail -> Note handoffs use a session-checked continuation. Ordinary mail keeps its original Delay."); Schedule->bCommentBubbleVisible = true;
        }
        auto* Event = Node<UK2Node_CustomEvent>(PG, *EventName.ToString());
        auto* Branch = Node<UK2Node_IfThenElse>(PG, *(Prefix + TEXT("Protected")));
        if (!Schedule || !Event || !Branch || Schedule->FindPin(TEXT("Continuation"))->DefaultValue != EventName.ToString() ||
            !Branch->GetElsePin()->LinkedTo.Contains(Delay->FindPin(UEdGraphSchema_K2::PN_Execute)) || Branch->GetThenPin()->LinkedTo.Num()) return false;
        if (!Delay->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num()) return false;
        for (auto* Link : Delay->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo)
            if (!Event->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Link)) return false;
    }
    auto* Owned = Node<UK2Node_IfThenElse>(PG, TEXT("VTG_MailCameraStillOwned"));
    auto* Blend = Node<UK2Node_CustomEvent>(PG, TEXT("K2Node_CustomEvent_1"));
    auto* Capture = Node<UK2Node_CallFunction>(PG, TEXT("VTG_CaptureMailCameraReturn"));
    if (!Blend || Blend->CustomFunctionName != TEXT("BlendToMailCam")) return false;
    if (!Capture && !VerifyOnly)
    {
        Capture = Call(PG,TEXT("VTG_CaptureMailCameraReturn"),UVTGMailReadingComponent::StaticClass(),TEXT("CaptureMailCameraReturn"),Blend->NodePosX+200,Blend->NodePosY-180);
        if (!Insert(Blend->FindPin(UEdGraphSchema_K2::PN_Then),Capture)) return false;
    }
    if (!Capture) return false;
    auto* Cleanup = Node<UK2Node_CallFunction>(PG, TEXT("K2Node_CallFunction_215"));
    auto* Restore = Node<UK2Node_CallFunction>(PG, TEXT("VTG_RestoreNoteMailCamera"));
    auto* Handled = Node<UK2Node_IfThenElse>(PG, TEXT("VTG_NoteMailCameraRestored"));
    if (!Owned || !Cleanup) return false;
    if (!Restore && !VerifyOnly)
    {
        Restore = Call(PG,TEXT("VTG_RestoreNoteMailCamera"),UVTGMailReadingComponent::StaticClass(),TEXT("RestoreNoteMailCamera"),Owned->NodePosX-500,Owned->NodePosY-350);
        const auto Sources = Owned->GetExecPin()->LinkedTo;
        Owned->GetExecPin()->BreakAllPinLinks();
        for (auto* Source : Sources) if (!Connect(Source,Restore->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
        Handled = Add<UK2Node_IfThenElse>(PG,TEXT("VTG_NoteMailCameraRestored"),Owned->NodePosX-200,Owned->NodePosY-350); Handled->AllocateDefaultPins();
        if (!Connect(Restore->FindPin(UEdGraphSchema_K2::PN_Then),Handled->GetExecPin()) ||
            !Connect(Restore->GetReturnValuePin(),Handled->GetConditionPin()) ||
            !Connect(Handled->GetThenPin(),Cleanup->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
            !Connect(Handled->GetElsePin(),Owned->GetExecPin())) return false;
        Restore->NodeComment = TEXT("Only Note-linked mail returns to its original view; later external scene cameras retain ownership."); Restore->bCommentBubbleVisible = true;
    }
    if (!Restore || !Handled || !Handled->GetElsePin()->LinkedTo.Contains(Owned->GetExecPin())) return false;
    if (!VerifyOnly && (!Save(Player) || !Save(Apartment))) return false;
    UE_LOG(LogTemp, Display, TEXT("NOTE MAIL CAMERA: verified scoped continuation wiring; ordinary Delay and fixed-camera selection preserved."));
    return true;
}
}

int32 UVTGMailFlowCommandlet::Main(const FString& Params)
{
    Schema = GetDefault<UEdGraphSchema_K2>();
    if (FParse::Param(*Params, TEXT("FixNoteMailCamera")) || FParse::Param(*Params, TEXT("VerifyNoteMailCamera")))
        return NoteMailCameraSessions(FParse::Param(*Params, TEXT("VerifyNoteMailCamera"))) ? 0 : 70;
    if (FParse::Param(*Params, TEXT("FixApartmentInput")) || FParse::Param(*Params, TEXT("VerifyApartmentInput")))
    {
        const bool Verify = FParse::Param(*Params, TEXT("VerifyApartmentInput"));
        auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment.BPLM_SaApartment"));
        if (!BP) return 60;
        auto* Graph = Events(BP);
        auto* Restore = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_39"));
        auto* Move = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_45"));
        auto* Collision = Node<UK2Node_CallFunction>(Graph, TEXT("VTG_RestoreApartmentCollision"));
        if (!Restore || !Move) return 61;
        if (!Verify && !Collision)
        {
            const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/ApartmentInput/BPLM_SaApartment-before-collision-restore.uasset");
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
            if (IFileManager::Get().Copy(*Backup, *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), false) != COPY_OK) return 62;
            Collision = Call(Graph, TEXT("VTG_RestoreApartmentCollision"), AActor::StaticClass(), TEXT("SetActorEnableCollision"), Move->NodePosX, Move->NodePosY - 180);
            Schema->TrySetDefaultValue(*Collision->FindPin(TEXT("bNewActorEnableCollision")), TEXT("true"));
            for (auto* Target : Restore->FindPin(TEXT("self"))->LinkedTo)
                if (!Connect(Target, Collision->FindPin(TEXT("self")))) return 63;
            Collision->NodeComment = TEXT("Intro disables the real Sa actor's collision. Restore it before Walking; EnableInput alone does not restore collision.");
            Collision->bCommentBubbleVisible = true;
            if (!Insert(Restore->FindPin(UEdGraphSchema_K2::PN_Then), Collision) || !Save(BP)) return 64;
        }
        if (!Collision || Collision->FindPin(TEXT("bNewActorEnableCollision"))->DefaultValue != TEXT("true") ||
            !Restore->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Collision->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
            !Collision->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Move->FindPin(UEdGraphSchema_K2::PN_Execute))) return 65;
        // Evaluate the real initialization tail without unrelated scene lighting/camera references.
        auto* Probe = DuplicateObject<UBlueprint>(BP, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("ApartmentInputProbe")));
        auto* PG = Events(Probe);
        auto* Start = Node<UK2Node_CustomEvent>(PG, TEXT("K2Node_CustomEvent_2"));
        auto* Visible = Node<UK2Node_CallFunction>(PG, TEXT("K2Node_CallFunction_41"));
        auto* End = Node<UK2Node_CallFunction>(PG, TEXT("K2Node_CallFunction_45"));
        Start->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
        if (!Connect(Start->FindPin(UEdGraphSchema_K2::PN_Then), Visible->FindPin(UEdGraphSchema_K2::PN_Execute))) return 66;
        End->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Probe);
        FKismetEditorUtilities::CompileBlueprint(Probe);
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        FActorSpawnParameters Spawn; Spawn.bDeferConstruction = true; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* PCClass = LoadClass<APlayerController>(nullptr, TEXT("/Game/ThirdPerson/PC_Game_VTG.PC_Game_VTG_C"));
        auto* SaClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));
        auto* PC = World->SpawnActor<APlayerController>(PCClass);
        auto* Sa = World->SpawnActor<ACharacter>(SaClass, FTransform::Identity, Spawn);
        auto* Manager = World->SpawnActor<AActor>(Probe->GeneratedClass, FTransform::Identity, Spawn);
        auto* GS = World->SpawnActor<AGameStateBase>(); World->SetGameState(GS);
        auto* PS = World->SpawnActor<APlayerState>(); PS->SetOwner(PC); PC->PlayerState = PS; GS->AddPlayerState(PS); PC->Possess(Sa);
        FindFProperty<FObjectProperty>(Manager->GetClass(), TEXT("Player Pawn"))->SetObjectPropertyValue_InContainer(Manager, Sa);
        Sa->ProcessEvent(Sa->FindFunction(TEXT("Sa_LockInput")), nullptr);
        Sa->GetCharacterMovement()->DisableMovement(); Sa->SetActorEnableCollision(false);
        // Baseline: the old init tail enabled input and Walking but left collision off.
        Sa->ProcessEvent(Sa->FindFunction(TEXT("Sa_RestoreInput")), nullptr);
        Sa->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        bool Passed = !Sa->GetActorEnableCollision() && Sa->InputEnabled() && !PC->IsMoveInputIgnored();
        UE_LOG(LogTemp, Display, TEXT("APARTMENT INPUT baseline: input=%d moveIgnored=%d collision=%d mode=%d"), Sa->InputEnabled(), PC->IsMoveInputIgnored(), Sa->GetActorEnableCollision(), int32(Sa->GetCharacterMovement()->MovementMode));
        Sa->ProcessEvent(Sa->FindFunction(TEXT("Sa_LockInput")), nullptr);
        Sa->GetCharacterMovement()->DisableMovement();
        Manager->ProcessEvent(Manager->FindFunction(TEXT("Initialize L3Apt_Day1Morning")), nullptr);
        Passed &= Sa->InputEnabled() && Sa->GetActorEnableCollision() && Sa->GetCharacterMovement()->MovementMode == MOVE_Walking && !PC->IsMoveInputIgnored();
        UE_LOG(LogTemp, Display, TEXT("APARTMENT INPUT saved init tail: input=%d moveIgnored=%d collision=%d mode=%d result=%s"), Sa->InputEnabled(), PC->IsMoveInputIgnored(), Sa->GetActorEnableCollision(), int32(Sa->GetCharacterMovement()->MovementMode), Passed ? TEXT("PASS") : TEXT("FAIL"));
        World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        return Passed ? 0 : 67;
    }
    if (FParse::Param(*Params, TEXT("FixStairIdle")) || FParse::Param(*Params, TEXT("VerifyStairIdle")))
    {
        auto* Sewer = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt"));
        if (!Sewer || !StairIdle(Sewer, FParse::Param(*Params, TEXT("VerifyStairIdle")))) return 50;
        return TestStairIdle(Sewer) ? 0 : 51;
    }
    if (FParse::Param(*Params, TEXT("FixMailCamera")) || FParse::Param(*Params, TEXT("VerifyMailCamera")))
    {
        auto* Player = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
        if (!Player || !MailCamera(Player, FParse::Param(*Params, TEXT("VerifyMailCamera")))) return 40;
        return TestMailCamera(Player) ? 0 : 41;
    }
    if (FParse::Param(*Params, TEXT("AuditBoundEventTiming")))
    {
        auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.SearchAllAssets(true);
        FARFilter Filter;
        Filter.ClassPaths.Add(UDialogueBlueprint::StaticClass()->GetClassPathName());
        Filter.PackagePaths.Add(TEXT("/Game"));
        Filter.bRecursiveClasses = Filter.bRecursivePaths = true;
        TArray<FAssetData> Assets;
        Registry.GetAssets(Filter, Assets);
        int32 Dialogues = 0, Nodes = 0, Bound = 0, Finish = 0, PhaseBranches = 0;
        bool Passed = true;
        FString Details;
        for (const auto& Data : Assets)
        {
            auto* BP = Cast<UDialogueBlueprint>(Data.GetAsset());
            if (!BP || !BP->DialogueTemplate) { Passed = false; continue; }
            ++Dialogues;
            for (auto* Item : BP->DialogueTemplate->GetNodes())
            {
                if (!Item) continue;
                ++Nodes;
                if (Item->OnPlayNodeFuncName.IsNone()) continue;
                ++Bound;
                if (Item->bCallBoundEventOnFinish)
                {
                    ++Finish;
                    Details += FString::Printf(TEXT("FINISH ONLY: %s | %s | %s\n"), *BP->GetPathName(), *Item->GetID().ToString(), *Item->OnPlayNodeFuncName.ToString());
                    Passed &= BP->GetName() == TEXT("L3_SaApartmentD1") && Item->GetID() == TEXT("L3_SaApartmentD1_DialogueNode_NPC_70");
                }
                if (auto* Graph = Events(BP)) for (const auto& GraphNode : Graph->Nodes)
                    if (auto* Event = Cast<UK2Node_CustomEvent>(GraphNode.Get()); Event && Event->CustomFunctionName == Item->OnPlayNodeFuncName)
                        if (auto* Phase = Event->FindPin(TEXT("bStarted")); Phase && Phase->LinkedTo.Num())
                        {
                            ++PhaseBranches;
                            Details += FString::Printf(TEXT("bStarted consumer: %s | %s | timing=%s\n"), *BP->GetPathName(), *Item->GetID().ToString(), Item->bCallBoundEventOnFinish ? TEXT("FINISH") : TEXT("START"));
                        }
            }
        }
        Passed &= Dialogues > 0 && Finish == 1;
        const FString Summary = FString::Printf(TEXT("Dialogue assets=%d\nNodes=%d\nBound events=%d\nSTART only=%d\nFINISH only=%d\nBound bStarted consumers=%d\nAudit=%s\nDefault is START. Only NPC_70 is allowed to opt in to FINISH. No asset was modified by this audit.\n\n"), Dialogues, Nodes, Bound, Bound - Finish, Finish, PhaseBranches, Passed ? TEXT("PASS") : TEXT("FAIL"));
        FFileHelper::SaveStringToFile(Summary + Details, *(FPaths::ProjectDir() / TEXT("Scripts/MailDialogueDiagnosis/bound-event-timing-audit.txt")));
        UE_LOG(LogTemp, Display, TEXT("BOUND TIMING AUDIT: assets=%d nodes=%d bound=%d start=%d finish=%d: %s"), Dialogues, Nodes, Bound, Bound - Finish, Finish, Passed ? TEXT("PASS") : TEXT("FAIL"));
        return Passed ? 0 : 31;
    }
    if (FParse::Param(*Params, TEXT("InspectCanLeave")))
    {
        auto* Map = LoadObject<UWorld>(nullptr, TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment.L_SewerUnderApartment"));
        if (!Map || !Map->PersistentLevel) return 30;
        FString Report;
        for (const auto& Item : Map->PersistentLevel->Actors)
        {
            auto* Actor = Item.Get();
            if (!Actor || !(Actor->GetClass()->GetName().Contains(TEXT("BPLM")) || Actor->GetClass()->GetName().Contains(TEXT("CameraLock")))) continue;
            Report += FString::Printf(TEXT("\n%s %s %s at %s\n"), *Actor->GetName(), *Actor->GetActorLabel(), *Actor->GetClass()->GetPathName(), *Actor->GetActorLocation().ToString());
            for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
            {
                const FString Name = It->GetName();
                if (!(Name.Contains(TEXT("Camera")) || Name.Contains(TEXT("Input")) || Name.Contains(TEXT("Lock")) || Name.Contains(TEXT("Release")) || Name.Contains(TEXT("ID")))) continue;
                FString Value; It->ExportText_InContainer(0, Value, Actor, nullptr, Actor, PPF_None);
                Report += Name + TEXT("=") + Value + TEXT("\n");
            }
        }
        FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectDir() / TEXT("Scripts/MailDialogueDiagnosis/sewer-camera-state.txt")));
        return 0;
    }
    if (FParse::Param(*Params, TEXT("FixCanLeave")) || FParse::Param(*Params, TEXT("VerifyCanLeave")))
    {
        auto* Sewer = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt"));
        auto* DialogueBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1.L3_SaApartmentD1"));
        if (!Sewer || !DialogueBP || !CanLeave(Sewer, DialogueBP, FParse::Param(*Params, TEXT("VerifyCanLeave")))) return 20;
        return TestCanLeave(Sewer) ? 0 : 21;
    }
    auto* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Narrative/Mail/DT_Mail_ID_FillHere.DT_Mail_ID_FillHere"));
    if (!Table) return 1;
    if (FParse::Param(*Params, TEXT("Apply")) || FParse::Param(*Params, TEXT("VerifyOnly")))
    {
        auto* Player = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
        auto* BPLM = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/BPLM.BPLM"));
        auto* Sewer = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt"));
        auto* Anim = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Characters/Sa/ABP_Sa.ABP_Sa"));
        auto* Interface = LoadClass<UInterface>(nullptr, TEXT("/Game/Characters/BPI_CineAnmInterface.BPI_CineAnmInterface_C"));
        if (!Player || !BPLM || !Sewer || !Anim || !Interface) return 10;
        if (FParse::Param(*Params, TEXT("VerifyOnly"))) return Verify(Player, BPLM, Sewer, Anim, Interface, Table) ? 0 : 15;
        for (auto* BP : {Player, BPLM, Sewer, Anim}) if (!Backup(BP)) return 11;
        if (!InstallIdle(Player, Anim, Interface) || !InstallMailHold(Player, BPLM)) return 12;
        auto* Overdue = Node<UK2Node_CallFunction>(Events(Sewer), TEXT("K2Node_CallFunction_76"));
        if (!Overdue || !Table->GetRowMap().Contains(TEXT("L3_Sewer_Overdue"))) return 13;
        Schema->TrySetDefaultValue(*Overdue->FindPin(TEXT("MailID")), TEXT("L3_Sewer_Overdue"));
        for (auto* BP : {Anim, Player, BPLM, Sewer}) if (!Save(BP)) return 14;
        UE_LOG(LogTemp, Display, TEXT("MAIL FLOW: saved player cleanup handoff, idle interface, and overdue row correction."));
        return 0;
    }
    FFileHelper::SaveStringToFile(Table->GetTableAsJSON(), *(FPaths::ProjectDir() / TEXT("Scripts/MailDialogueDiagnosis/mail_table.json")));
    for (const TCHAR* Path : {TEXT("/Game/Characters/Sa/ABP_Sa.ABP_Sa"), TEXT("/Game/Characters/BP_StillNPC.BP_StillNPC"), TEXT("/Game/Characters/BPI_CineAnmInterface.BPI_CineAnmInterface")})
    {
        auto* BP = LoadObject<UBlueprint>(nullptr, Path);
        if (!BP) return 2;
        FExportObjectInnerContext Context;
        FStringOutputDevice Output;
        UExporter::ExportToOutputDevice(&Context, BP, nullptr, Output, TEXT("copy"), 0, PPF_Copy, false);
        FFileHelper::SaveStringToFile(Output, *(FPaths::ProjectDir() / TEXT("Scripts/MailDialogueDiagnosis") / (BP->GetName() + TEXT(".copy"))));
    }
    return 0;
}
