#include "VTGBlackMarketQuestScanCommandlet.h"
#include "Quests/VTGEnsureQuestStateEvent.h"
#include "Dialogue.h"
#include "DialogueBlueprint.h"
#include "DialogueSM.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "NarrativeComponent.h"
#include "Quest.h"
#include "QuestSM.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PanelWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/AnimMontage.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

namespace QuestScan
{
const FName Allowed(TEXT("AllowIPHAScan"));
UEdGraphNode* Named(UEdGraph* G,const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetName()==Name)return N;checkNoEntry();return nullptr;}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;N->NodeComment=TEXT("VTG scan: cancelled requests must not reopen after the latent move");return N;}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(AP),B->FindPinChecked(BP)));}
bool Compile(UBlueprint* BP){FCompilerResultsLog R;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&R);UE_LOG(LogTemp,Display,TEXT("QUEST_SCAN compile %s errors=%d warnings=%d"),*BP->GetName(),R.NumErrors,R.NumWarnings);return !R.NumErrors && BP->Status!=BS_Error;}
void Save(UBlueprint* BP)
{
    const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/BlackMarketQuestScan/Backup")/(BP->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
    FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,A));
}
UDialogueNode* Node(UDialogue* D,const TCHAR* Name){for(auto* N:D->GetNodes())if(N->GetName()==Name)return N;checkNoEntry();return nullptr;}
void Apply(UBlueprint* BP)
{
    auto* Template=CastChecked<UDialogueBlueprint>(BP)->DialogueTemplate;auto* N=Node(Template,TEXT("DialogueNode_NPC_279"));
    check(N->Events.Num()==1 && N->Events[0]->GetClass()->GetName()==TEXT("NE_BeginQuest_C"));
    auto* Old=N->Events[0];auto* Event=NewObject<UVTGEnsureQuestStateEvent>(N,NAME_None,RF_Transactional);
    Event->Quest=LoadClass<UQuest>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/Q_L4BlackMarketInvestigation.Q_L4BlackMarketInvestigation_C"));check(Event->Quest);
    Event->StateID=TEXT("QuestState_1");Event->EventRuntime=Old->EventRuntime;Event->bRefireOnLoad=Old->bRefireOnLoad;Event->PartyEventPolicy=Old->PartyEventPolicy;N->Events[0]=Event;
    FEdGraphPinType Bool;Bool.PinCategory=UEdGraphSchema_K2::PC_Boolean;check(FBlueprintEditorUtils::AddMemberVariable(BP,Allowed,Bool,TEXT("false")));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);check(Compile(BP));
    UEdGraph* G=nullptr;for(UEdGraph* Graph:BP->UbergraphPages)if(Graph->GetName()==TEXT("EventGraph"))G=Graph;check(G);
    auto Insert=[&](UEdGraphNode* Before,const TCHAR* Out,UEdGraphNode* After,const TCHAR* In,bool Value,int X,int Y)
    {
        auto* Set=Add<UK2Node_VariableSet>(G,X,Y);Set->VariableReference.SetSelfMember(Allowed);Set->AllocateDefaultPins();GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Set->FindPinChecked(Allowed),Value?TEXT("true"):TEXT("false"));
        Before->FindPinChecked(Out)->BreakLinkTo(After->FindPinChecked(In));Link(Before,Out,Set,TEXT("execute"));Link(Set,TEXT("then"),After,In);
    };
    auto* Start=Named(G,TEXT("K2Node_CustomEvent_45"));auto* Find=Named(G,TEXT("K2Node_CallFunction_65"));Insert(Start,TEXT("then"),Find,TEXT("execute"),true,12500,8500);
    auto* Off=Named(G,TEXT("K2Node_CallFunction_55"));auto Inputs=Off->FindPinChecked(TEXT("execute"))->LinkedTo;
    // Every existing caller of the shared TurnOffScan chain cancels a pending request.
    for(int I=0;I<Inputs.Num();++I)Insert(Inputs[I]->GetOwningNode(),*Inputs[I]->PinName.ToString(),Off,TEXT("execute"),false,12500,8800+I*180);
    auto* Move=Named(G,TEXT("K2Node_CallFunction_53"));auto* Open=Named(G,TEXT("K2Node_CallFunction_54"));
    check(Move->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Open->FindPinChecked(TEXT("execute"))));
    auto* Gate=Add<UK2Node_IfThenElse>(G,12500,9300);Gate->AllocateDefaultPins();auto* Get=Add<UK2Node_VariableGet>(G,12200,9500);Get->VariableReference.SetSelfMember(Allowed);Get->AllocateDefaultPins();
    Move->FindPinChecked(TEXT("then"))->BreakAllPinLinks();Link(Move,TEXT("then"),Gate,TEXT("execute"));Link(Get,TEXT("AllowIPHAScan"),Gate,TEXT("Condition"));Link(Gate,TEXT("then"),Open,TEXT("execute"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}
void Obj(UObject* O,const TCHAR* Name,UObject* V){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),Name);check(P);P->SetObjectPropertyValue_InContainer(O,V);}
void Fire(UDialogue* D,int ID)
{
    const FString Name=FString::Printf(TEXT("OnDialogueNode Started/Finished Playing - L4_BlackMarketInvestigation_Still_MainInvestigation_DialogueNode_NPC_%d"),ID);auto* F=D->FindFunction(FName(*Name));check(F);FStructOnScope A(F);
    FindFProperty<FBoolProperty>(F,TEXT("bStarted"))->SetPropertyValue_InContainer(A.GetStructMemory(),true);D->ProcessEvent(F,A.GetStructMemory());
}
void Tests(UBlueprint* BP)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("QuestScanTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);check(W);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* PC=W->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);W->AddController(PC);auto* Comp=NewObject<UNarrativeComponent>(PC);Comp->RegisterComponent();
    auto* Template=CastChecked<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass)->GetDialogueTemplate();auto* Event=CastChecked<UVTGEnsureQuestStateEvent>(Node(Template,TEXT("DialogueNode_NPC_279"))->Events[0]);
    Event->ExecuteEvent(nullptr,PC,Comp);auto* Quest=Comp->GetQuestInstance(Event->Quest);check(Quest && Quest->GetCurrentState() && Quest->GetCurrentState()->GetID()==TEXT("QuestState_1"));
    check(Quest->GetCurrentState()->Branches.Num()==1 && Quest->GetCurrentState()->Branches[0]->GetID()==TEXT("QuestBranch_7"));
    auto* OverlayClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C"));check(OverlayClass);auto* Overlay=CreateWidget<UUserWidget>(PC,OverlayClass);check(Overlay);
    auto* OnState=Overlay->FindFunction(TEXT("On Quest New State"));check(OnState);FStructOnScope Args(OnState);FindFProperty<FObjectPropertyBase>(OnState,TEXT("Quest"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Quest);FindFProperty<FObjectPropertyBase>(OnState,TEXT("NewState"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Quest->GetCurrentState());Overlay->ProcessEvent(OnState,Args.GetStructMemory());
    auto* Box=Cast<UPanelWidget>(Overlay->WidgetTree->FindWidget(TEXT("TaskBox")));check(Box && Box->GetChildrenCount()>0);UE_LOG(LogTemp,Display,TEXT("QUEST_SCAN TEST PASS fresh quest exit state; Overlay task children=%d"),Box->GetChildrenCount());
    Comp->ForgetQuest(Event->Quest);Quest=Comp->BeginQuest(Event->Quest,TEXT("Investigation"));check(Quest);auto* Existing=Quest;Event->ExecuteEvent(nullptr,PC,Comp);check(Comp->GetQuestInstance(Event->Quest)==Existing && Existing->GetCurrentState()->GetID()==TEXT("QuestState_1"));
    struct FStateArgs{UQuestState* NewState;};FStateArgs StateArgs{Existing->GetState(TEXT("QuestState_2"))};Existing->ProcessEvent(Existing->FindFunctionChecked(TEXT("EnterState")),&StateArgs);Event->ExecuteEvent(nullptr,PC,Comp);check(Existing->GetCurrentState()->GetID()==TEXT("QuestState_2"));UE_LOG(LogTemp,Display,TEXT("QUEST_SCAN TEST PASS existing quest advances without restart; replay does not regress"));
    auto* ManagerClass=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket.BPLM_BlackMarket_C"));auto* DroneClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC.BP_Drone_IPHA_NPC_C"));auto* SaClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(ManagerClass && DroneClass && SaClass);
    auto* Manager=W->SpawnActor<AActor>(ManagerClass,FTransform::Identity,Spawn);auto* Drone=W->SpawnActor<ACharacter>(DroneClass,FTransform::Identity,Spawn);auto* Sa=W->SpawnActor<ACharacter>(SaClass,FTransform::Identity,Spawn);check(Manager && Drone && Sa);PC->Possess(Sa);Sa->GetMesh()->InitAnim(true);
    Obj(Manager,TEXT("IPHADrone"),Drone);Obj(Manager,TEXT("Player Pawn"),Sa);auto* Dialogue=NewObject<UDialogue>(Comp,BP->GeneratedClass);Obj(Dialogue,TEXT("OwningComp"),Comp);
    auto* BeamProperty=FindFProperty<FObjectPropertyBase>(Drone->GetClass(),TEXT("LightBeam"));check(BeamProperty);auto* Beam=Cast<USceneComponent>(BeamProperty->GetObjectPropertyValue_InContainer(Drone));check(Beam);
    auto Advance=[&](){for(int I=0;I<270;++I){W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Dialogue,1.f/60.f);}};
    Fire(Dialogue,177);Fire(Dialogue,185);check(!Beam->IsVisible());Advance();check(!Beam->IsVisible());UE_LOG(LogTemp,Display,TEXT("QUEST_SCAN TEST PASS rapid 177 -> 185: scan stays off after 4-second move completes"));
    Fire(Dialogue,177);Advance();check(Beam->IsVisible());Fire(Dialogue,185);check(!Beam->IsVisible());UE_LOG(LogTemp,Display,TEXT("QUEST_SCAN TEST PASS normal timing: scan opens then turns off at 185"));
    auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Characters/Sa/Anm/FistFight/ANM_Sa_FightFist03_Inplace1_Montage.ANM_Sa_FightFist03_Inplace1_Montage"));check(Montage);UE_LOG(LogTemp,Display,TEXT("THIRD_PUNCH montage length=%f rate=%f blendout=%f"),Montage->GetPlayLength(),Montage->RateScale,Montage->BlendOut.GetBlendTime());for(const auto& N:Montage->Notifies)UE_LOG(LogTemp,Display,TEXT("THIRD_PUNCH notify=%s start=%f end=%f"),*N.GetNotifyEventName().ToString(),N.GetTriggerTime(),N.GetEndTriggerTime());
    GEngine->DestroyWorldContext(W);W->DestroyWorld(false);
}
}
UVTGBlackMarketQuestScanCommandlet::UVTGBlackMarketQuestScanCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGBlackMarketQuestScanCommandlet::Main(const FString& Params)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation.L4_BlackMarketInvestigation_Still_MainInvestigation"));check(BP);
    if(!Params.Contains(TEXT("VerifyOnly")))QuestScan::Apply(BP);check(QuestScan::Compile(BP));QuestScan::Tests(BP);if(!Params.Contains(TEXT("VerifyOnly")))QuestScan::Save(BP);UE_LOG(LogTemp,Display,TEXT("QUEST_SCAN_SUCCESS"));return 0;
}





