#include "VTGUIFeedbackCommandlet.h"
#include "Audio/VTGUIFeedbackLibrary.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "VTGResponseTransitionFixture.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Misc/FileHelper.h"
#include "Interaction/VTGSmartTriggerComponent.h"
#include "K2Node_CustomEvent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

namespace VTGUIFeedback
{
void Compile(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
    check(Log.NumErrors == 0 && BP->Status != BS_Error);
}
void Backup(UBlueprint* BP)
{
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Target = FPaths::ProjectDir() / TEXT("Scripts/UIFeedback/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
    if (!IFileManager::Get().FileExists(*Target)) check(IFileManager::Get().Copy(*Target, *File) == COPY_OK);
}
void Save(UBlueprint* BP)
{
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
    check(UPackage::SavePackage(BP->GetOutermost(), BP, *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args));
}
template<class T> T* Add(UEdGraph* Graph)
{
    auto* Node = NewObject<T>(Graph); Graph->AddNode(Node, false, false); Node->CreateNewGuid(); return Node;
}
void Link(UEdGraphPin* A, UEdGraphPin* B) { check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B)); }

void Install(UBlueprint* BP, UEdGraphNode* Entry, bool Interaction, bool Verify)
{
    const FName Function = Interaction ? TEXT("PlayInteractionFeedback") : TEXT("PlayQuestFeedback");
    auto* Graph = Entry->GetGraph();
    UK2Node_CallFunction* Call = nullptr;
    int32 Count = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* C = Cast<UK2Node_CallFunction>(Node))
            if (C->FunctionReference.GetMemberName() == Function) { Call = C; ++Count; }
    check(Count <= 1);
    if (!Call)
    {
        check(!Verify);
        Call = Add<UK2Node_CallFunction>(Graph);
        Call->SetFromFunction(UVTGUIFeedbackLibrary::StaticClass()->FindFunctionByName(Function));
        Call->AllocateDefaultPins(); Call->NodePosX = Entry->NodePosX + 240; Call->NodePosY = Entry->NodePosY - 160;
        Call->NodeComment = TEXT("Independent interaction / quest UI feedback (not dialogue audio)");
        Call->bCommentBubbleVisible = true;
        auto* Out = Entry->FindPinChecked(TEXT("then"));
        const auto OldLinks = Out->LinkedTo; check(!OldLinks.IsEmpty()); Out->BreakAllPinLinks();
        Link(Out, Call->FindPinChecked(TEXT("execute")));
        for (auto* Pin : OldLinks) Link(Call->FindPinChecked(TEXT("then")), Pin);
        if (Interaction)
        {
            auto* Self = Add<UK2Node_Self>(Graph); Self->AllocateDefaultPins();
            Link(Self->FindPinChecked(TEXT("self")), Call->FindPinChecked(TEXT("WorldContextObject")));
            for (const auto& Pair : TArray<TPair<FName,FName>>{{TEXT("InteractionSoundEnabled"),TEXT("bEnabled")},{TEXT("InteractionSoundOverride"),TEXT("SoundOverride")}})
            {
                auto* Get = Add<UK2Node_VariableGet>(Graph); Get->VariableReference.SetSelfMember(Pair.Key); Get->AllocateDefaultPins();
                Link(Get->FindPinChecked(Pair.Key), Call->FindPinChecked(Pair.Value));
            }
        }
        else
            Link(Entry->FindPinChecked(TEXT("NarrativeComponent")), Call->FindPinChecked(TEXT("WorldContextObject")));
    }
    check(Entry->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Call->FindPinChecked(TEXT("execute"))));
    check(!Call->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    check(!Call->FindPinChecked(TEXT("WorldContextObject"))->LinkedTo.IsEmpty());
    if (Interaction)
    {
        check(!Call->FindPinChecked(TEXT("bEnabled"))->LinkedTo.IsEmpty());
        check(!Call->FindPinChecked(TEXT("SoundOverride"))->LinkedTo.IsEmpty());
    }
    Compile(BP);
    if (!Verify) Save(BP);
    UE_LOG(LogTemp, Display, TEXT("UI FEEDBACK PASS: %s trigger wired once; existing execution preserved"), *BP->GetName());
}
}

#include "VTGResponseFeedback.inl"
#include "VTGSmartTriggerInstall.inl"
#include "VTGSmartInteractionVisibility.inl"
#include "VTGCameraFootstepMaintenance.inl"
#include "VTGCombatSpineLimit.inl"
bool VTGTestSmartTriggers();
bool VTGRepairEggDialogue(bool Verify);
bool VTGRepairAptWindowEgg(bool Verify);

int32 UVTGUIFeedbackCommandlet::Main(const FString& Params)
{
    using namespace VTGUIFeedback;
    if(FParse::Param(*Params,TEXT("ApartmentWindowEgg")))return VTGRepairAptWindowEgg(FParse::Param(*Params,TEXT("VerifyOnly")))?0:1;
    if(FParse::Param(*Params,TEXT("EggDialogue")))return VTGRepairEggDialogue(FParse::Param(*Params,TEXT("VerifyOnly")))?0:1;
    if(FParse::Param(*Params,TEXT("HeadYawLimit")))
    {
        auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_Sa"));check(BP);
        auto* Defaults=CastChecked<UVTGCameraLookAnimInstance>(BP->GeneratedClass->GetDefaultObject());
        if(!FParse::Param(*Params,TEXT("VerifyOnly"))){Backup(BP);Defaults->MaxLookYaw=40.f;BP->MarkPackageDirty();Save(BP);}
        check(Defaults->MaxLookYaw==40.f);
        for(float Yaw:{-90.f,-40.f,-15.f,0.f,15.f,40.f,90.f})
        {
            const auto Look=UVTGCameraLookAnimInstance::ClampCameraLook(FQuat::Identity,FRotator(0,Yaw,0),Defaults->MaxLookYaw,Defaults->MaxLookPitch);
            check(FMath::IsNearlyEqual(Look.Yaw,double(FMath::Clamp(Yaw,-40.f,40.f)),.001));
        }
        UE_LOG(LogTemp,Display,TEXT("HEAD YAW LIMIT PASS: actual ABP defaults +/-40; neck/head combined; shared combat/non-combat"));return 0;
    }
    if(FParse::Param(*Params,TEXT("CombatSpineLimit"))){CombatSpineLimit(FParse::Param(*Params,TEXT("VerifyOnly")));return 0;}
    if(FParse::Param(*Params,TEXT("CameraAspectBatch"))){CameraAspectBatch(FParse::Param(*Params,TEXT("VerifyOnly")),FParse::Param(*Params,TEXT("Resume")));return 0;}
    if(FParse::Param(*Params,TEXT("LandingFootstep"))){InstallLanding(FParse::Param(*Params,TEXT("VerifyOnly")));return 0;}
    if(FParse::Param(*Params,TEXT("SmartVisibility"))){InstallSmartVisibility(FParse::Param(*Params,TEXT("VerifyOnly")));return 0;}
    if(FParse::Param(*Params,TEXT("SmartInteractionDetails")))
    {
        const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));
        auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BPA_SmartRestrict"));check(BP);
        if(!Verify)Backup(BP);
        for(FName Name:{FName(TEXT("InteractionDetectionDistance")),FName(TEXT("InteractionConfirmDistance"))})
        {
            if(FBlueprintEditorUtils::FindNewVariableIndex(BP,Name)==INDEX_NONE)
            {
                check(!Verify);FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Real;Type.PinSubCategory=UEdGraphSchema_K2::PC_Float;
                check(FBlueprintEditorUtils::AddMemberVariable(BP,Name,Type,TEXT("-1.0")));
                FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,Name,false);
                FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,Name,nullptr,FText::FromString(TEXT("Smart Trigger|Interaction")));
                FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ToolTip"),Name==TEXT("InteractionDetectionDistance")?TEXT("ISX detection distance in cm. -1 uses the normal ISX default."):TEXT("ISX interact/confirm distance in cm. -1 uses the normal ISX default."));
                FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ClampMin"),TEXT("-1"));
                FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("Units"),TEXT("cm"));
            }
        }
        Compile(BP);
        for(FName Name:{FName(TEXT("InteractionDetectionDistance")),FName(TEXT("InteractionConfirmDistance"))})
        {
            auto* Property=FindFProperty<FFloatProperty>(BP->GeneratedClass,Name);check(Property);
            check(Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
            check(Property->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject())==-1.f);
        }
        if(!Verify)Save(BP);
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
        auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        for(int32 Index=0;Index<2;++Index)
        {
            auto* Actor=World->SpawnActor<AActor>(BP->GeneratedClass);
            FindFProperty<FBoolProperty>(BP->GeneratedClass,TEXT("RequireInteraction"))->SetPropertyValue_InContainer(Actor,true);
            const float Detection=275.f+Index*285.f,Confirm=345.f+Index*285.f;
            FindFProperty<FFloatProperty>(BP->GeneratedClass,TEXT("InteractionDetectionDistance"))->SetPropertyValue_InContainer(Actor,Detection);
            FindFProperty<FFloatProperty>(BP->GeneratedClass,TEXT("InteractionConfirmDistance"))->SetPropertyValue_InContainer(Actor,Confirm);
            UVTGSmartTriggerComponent::InitializeTrigger(Actor);
            AActor* Proxy=nullptr;for(TActorIterator<AActor> It(World);It;++It)if(It->GetOwner()==Actor && It->ActorHasTag(TEXT("VTG_SmartTriggerInteractionProxy")))Proxy=*It;check(Proxy);
            auto* Property=FindFProperty<FObjectPropertyBase>(Proxy->GetClass(),TEXT("AC_ActorInteractionComponent"));auto* Component=Property->GetObjectPropertyValue_InContainer(Proxy);check(Component);
            for(const auto& Value:TArray<TPair<FName,float>>{{TEXT("OverrideDetectionDistance"),Detection},{TEXT("OverridePickUpDistance"),Confirm}})
            {auto* P=FindFProperty<FNumericProperty>(Component->GetClass(),Value.Key);check(P && FMath::IsNearlyEqual(P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(Component)),double(Value.Value)));}
        }
        World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
        UE_LOG(LogTemp,Display,TEXT("SMART DISTANCE PASS: editable defaults -1; two independent instances copied to real ISX components"));return 0;
    }
    if(FParse::Param(*Params,TEXT("SmartTriggerTests")))return VTGTestSmartTriggers()?0:1;
    if(FParse::Param(*Params,TEXT("SmartTriggers"))){InstallSmartTriggers(FParse::Param(*Params,TEXT("VerifyOnly")));return 0;}
    if(FParse::Param(*Params,TEXT("SmartInspect")))
    {
        for(const TCHAR* Name:{TEXT("BPA_SmartRestrict"),TEXT("BPA_0SmartTrigger"),TEXT("BP_0ISXInteractiveCollision"),TEXT("AC_ActorInteractionComponent"),TEXT("AC_PlayerInteractionComponent")})
        {
            const FString Root=FString(Name).StartsWith(TEXT("AC_"))?TEXT("/InventorySystemX/InventorySystemX/Components/"):TEXT("/Game/Blueprints/");
            auto* BP=LoadObject<UBlueprint>(nullptr,*(Root+Name));check(BP);
            FString Out=TEXT("PARENT ")+BP->ParentClass->GetPathName()+TEXT("\n");
            auto Props=[&](UObject* O){Out+=TEXT("OBJECT ")+O->GetPathName()+TEXT("\n");for(TFieldIterator<FProperty> P(O->GetClass());P;++P){FString V;P->ExportText_InContainer(0,V,O,O,O,PPF_None);if(V.Len()<3000)Out+=P->GetName()+TEXT("=")+V+TEXT("\n");}};
            Props(BP->GeneratedClass->GetDefaultObject());
            if(BP->SimpleConstructionScript)for(auto* N:BP->SimpleConstructionScript->GetAllNodes())Props(N->ComponentTemplate);
            TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* G:Graphs){Out+=TEXT("\nGRAPH ")+G->GetName()+TEXT("\n");for(UEdGraphNode* N:G->Nodes){Out+=N->GetName()+TEXT(" | ")+N->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"),TEXT(" "))+TEXT("\n");for(auto* P:N->Pins){Out+=TEXT("  ")+P->PinName.ToString()+(P->Direction==EGPD_Input?TEXT(" IN "):TEXT(" OUT "))+P->DefaultValue+TEXT(" ")+GetPathNameSafe(P->DefaultObject);for(auto* L:P->LinkedTo)Out+=TEXT(" -> ")+L->GetOwningNode()->GetName()+TEXT(".")+L->PinName.ToString();Out+=TEXT("\n");}}}
            FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectSavedDir()/FString(Name)+TEXT("_Inspect.txt")));
        }
        return 0;
    }
    if(FParse::Param(*Params,TEXT("OverlayPresentation")))
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EnvArt/ModernCity/Meshes/Props/SM_FireEscape_Landing2.SM_FireEscape_Landing2"));check(Mesh);
        for(const auto& Material:Mesh->GetStaticMaterials())UE_LOG(LogTemp,Display,TEXT("LANDING MATERIAL %s"),*GetPathNameSafe(Material.MaterialInterface));
        auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay"));check(BP);
        const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));
        if(!Verify)Backup(BP);
        UEdGraph* Graph=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for(auto* Candidate:Graphs)if(Candidate->GetName()==TEXT("OnNPCDialogueLineStarted"))Graph=Candidate;
        if(!Graph)for(auto* Candidate:Graphs)if(Candidate->GetName().Contains(TEXT("NPC")) && Candidate->GetName().Contains(TEXT("Started")))Graph=Candidate;
        check(Graph);
        auto Named=[&](const TCHAR* Name){for(UEdGraphNode* Node:Graph->Nodes)if(Node->GetName()==Name)return Node;return static_cast<UEdGraphNode*>(nullptr);};
        auto* Speaker=Named(TEXT("K2Node_BreakStruct_8"));check(Speaker);
        for(const TCHAR* Name:{TEXT("K2Node_CallFunction_12"),TEXT("K2Node_CallFunction_13")})
        {
            auto* Target=Named(Name);check(Target);auto* Input=Target->FindPinChecked(TEXT("InText"));
            const FString Marker=FString(TEXT("VTG hide Mail speaker label: "))+Name;
            UK2Node_CallFunction* Filter=nullptr;
            for(UEdGraphNode* Node:Graph->Nodes)if(Node->NodeComment==Marker)Filter=CastChecked<UK2Node_CallFunction>(Node);
            if(!Filter)
            {
                check(!Verify);Filter=Add<UK2Node_CallFunction>(Graph);Filter->SetFromFunction(UVTGUIFeedbackLibrary::StaticClass()->FindFunctionByName(TEXT("FilterSpeakerLabel")));Filter->AllocateDefaultPins();Filter->NodeComment=Marker;
                const auto Old=Input->LinkedTo;check(Old.Num()==1);Input->BreakAllPinLinks();Link(Old[0],Filter->FindPinChecked(TEXT("Label")));
                Link(Speaker->FindPinChecked(TEXT("SpeakerID")),Filter->FindPinChecked(TEXT("SpeakerID")));Link(Speaker->FindPinChecked(TEXT("SpeakerName")),Filter->FindPinChecked(TEXT("SpeakerName")));
                Link(Filter->FindPinChecked(TEXT("ReturnValue")),Input);
            }
            check(Input->LinkedTo.Contains(Filter->FindPinChecked(TEXT("ReturnValue"))));
        }
        Compile(BP);if(!Verify)Save(BP);
        UE_LOG(LogTemp,Display,TEXT("MAIL LABEL PASS: main and side labels filtered by ID or name"));return 0;
    }
    if(FParse::Param(*Params,TEXT("ResponseTransitionTests")))return VTGRunResponseTransitionChecks()?0:1;
    if(FParse::Param(*Params,TEXT("TransitionInspect")))
    {
        auto* Button=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/WBP_NarrativeButton_DialogueOption_ProfilePicture"));check(Button);
        auto* Owner=FindFProperty<FObjectPropertyBase>(Button->GeneratedClass,TEXT("OwningWidget"));check(Owner);
        auto* BP=CastChecked<UBlueprint>(Owner->PropertyClass->ClassGeneratedBy);
        if(FParse::Param(*Params,TEXT("Overlay"))) BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay"));
        check(BP);
        UE_LOG(LogTemp,Display,TEXT("TRANSITION OWNER %s"),*BP->GetPathName());
        for(TFieldIterator<FProperty> It(BP->GeneratedClass);It;++It) UE_LOG(LogTemp,Display,TEXT("TRANSITION PROP %s %s"),*It->GetName(),*It->GetCPPType());
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
        {
            UE_LOG(LogTemp,Display,TEXT("TRANSITION NODE %s %s %s"),*Graph->GetName(),*Node->GetName(),*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"),TEXT(" ")));
            for(auto* Pin:Node->Pins){FString Links;for(auto* Other:Pin->LinkedTo)Links+=Other->GetOwningNode()->GetName()+TEXT(".")+Other->PinName.ToString()+TEXT(" ");UE_LOG(LogTemp,Display,TEXT("TRANSITION PIN %s %s val=%s obj=%s links=%s"),*Node->GetName(),*Pin->PinName.ToString(),*Pin->DefaultValue,*GetPathNameSafe(Pin->DefaultObject),*Links);}
        }
        return 0;
    }
    if(FParse::Param(*Params,TEXT("ResponseAudio")))
    {
        InstallResponseAudio(FParse::Param(*Params,TEXT("VerifyOnly")));
        return 0;
    }
    if(FParse::Param(*Params,TEXT("ResponseInspect")))
    {
        for(const TCHAR* Path : {TEXT("/Game/NarrativeUI/Widgets/WBP_NarrativeButton_DialogueOption"), TEXT("/Game/Narrative/NarrativeUI/Widgets/WBP_NarrativeButton_DialogueOption_ProfilePicture"), TEXT("/Game/Narrative/NarrativeUI/WBP_NHUD"), TEXT("/NarrativeCommonUI/Widgets/WBP_NarrativeButton")})
        {
            auto* BP=LoadObject<UBlueprint>(nullptr,Path); if(!BP)continue;
            UE_LOG(LogTemp,Display,TEXT("RESPONSE BP %s parent=%s"),Path,*GetNameSafe(BP->ParentClass));
            TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
            for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
            {
                UE_LOG(LogTemp,Display,TEXT("RESPONSE NODE %s / %s / %s / %s"),*Graph->GetName(),*Node->GetName(),*Node->GetClass()->GetName(),*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"),TEXT(" ")));
                for(auto* Pin:Node->Pins)
                {
                    FString Links;for(auto* Other:Pin->LinkedTo)Links+=Other->GetOwningNode()->GetName()+TEXT(".")+Other->PinName.ToString()+TEXT(" ");
                    UE_LOG(LogTemp,Display,TEXT("RESPONSE PIN %s %s default=%s object=%s links=%s"),*Node->GetName(),*Pin->PinName.ToString(),*Pin->DefaultValue,*GetPathNameSafe(Pin->DefaultObject),*Links);
                }
            }
        }
        return 0;
    }
    const bool Verify = FParse::Param(*Params, TEXT("VerifyOnly"));
    const auto* Defaults = GetDefault<UVTGUIFeedbackLibrary>();
    check(Defaults->InteractionSounds.Num() == 5 && Defaults->QuestSounds.Num() == 3);
    auto* Override = Defaults->QuestSounds[0].Get();
    check(!UVTGUIFeedbackLibrary::ResolveInteractionSound(false, nullptr));
    check(!UVTGUIFeedbackLibrary::ResolveInteractionSound(false, Override));
    check(UVTGUIFeedbackLibrary::ResolveInteractionSound(true, Override) == Override);
    for (int32 I = 0; I < 100; ++I) check(Defaults->InteractionSounds.Contains(UVTGUIFeedbackLibrary::ResolveInteractionSound(true, nullptr)));
    UE_LOG(LogTemp, Display, TEXT("UI FEEDBACK PASS: five hover / three quest sources; disabled, override and default fallback"));

    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision")); check(BP);
    if (!Verify)
    {
        Backup(BP);
        for (const FName Name : {FName(TEXT("InteractionSoundEnabled")), FName(TEXT("InteractionSoundOverride"))})
        {
            if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE) continue;
            FEdGraphPinType Type;
            const bool Enabled = Name == TEXT("InteractionSoundEnabled");
            Type.PinCategory = Enabled ? UEdGraphSchema_K2::PC_Boolean : UEdGraphSchema_K2::PC_Object;
            if (!Enabled) Type.PinSubCategoryObject = USoundBase::StaticClass();
            check(FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type, Enabled ? TEXT("true") : TEXT("None")));
            FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, Name, false);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, Name, nullptr, FText::FromString(TEXT("Interaction Audio")));
            FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Name, nullptr, TEXT("ToolTip"), Enabled ? TEXT("Play audio when the player interacts. Enabled by default.") : TEXT("Optional interaction sound. None randomly uses Hover Magic 001-005."));
        }
        Compile(BP);
    }
    auto* Enabled = FindFProperty<FBoolProperty>(BP->GeneratedClass, TEXT("InteractionSoundEnabled"));
    auto* OverrideProperty = FindFProperty<FObjectPropertyBase>(BP->GeneratedClass, TEXT("InteractionSoundOverride"));
    check(Enabled && OverrideProperty && Enabled->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
    check(!OverrideProperty->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
    for (FProperty* Property : {static_cast<FProperty*>(Enabled), static_cast<FProperty*>(OverrideProperty)})
        check(Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
    UEdGraphNode* InteractionEntry = nullptr;
    for (UEdGraph* Graph : BP->UbergraphPages)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Event = Cast<UK2Node_ComponentBoundEvent>(Node))
                if (Event->DelegatePropertyName == TEXT("OnInteract_Server")) { check(!InteractionEntry); InteractionEntry = Event; }
    check(InteractionEntry); Install(BP, InteractionEntry, true, Verify);

    for (const TCHAR* Path : {TEXT("/Narrative/DefaultEvents/NE_BeginQuest"), TEXT("/Narrative/DefaultEvents/NE_CompleteNarrativeTask")})
    {
        auto* QuestBP = LoadObject<UBlueprint>(nullptr, Path); check(QuestBP);
        if (!Verify) Backup(QuestBP);
        UK2Node_FunctionEntry* Entry = nullptr;
        for (UEdGraph* Graph : QuestBP->FunctionGraphs)
            if (Graph->GetFName() == TEXT("ExecuteEvent"))
                for (UEdGraphNode* Node : Graph->Nodes) if (auto* E = Cast<UK2Node_FunctionEntry>(Node)) Entry = E;
        check(Entry); Install(QuestBP, Entry, false, Verify);
    }
    return 0;
}
