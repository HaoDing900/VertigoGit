#include "Engine/Blueprint.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Components/Widget.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "NarrativeComponent.h"
#include "Editor.h"
#include "UObject/StructOnScope.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"

static void TestEggActionText()
{
    FEditorScriptExecutionGuard Guard;
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);
    auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
    check(W->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute)));
    auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Sa=W->SpawnActor<ACharacter>(LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C")),FTransform::Identity,Spawn);PC->Possess(Sa);
    auto* NC=Sa->FindComponentByClass<UNarrativeComponent>();check(NC);
    auto* UI=CreateWidget<UUserWidget>(PC,LoadClass<UUserWidget>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C")));check(UI);
    auto* Bind=UI->FindFunction(TEXT("Bind To Component"));check(Bind);FStructOnScope Args(Bind);
    FindFProperty<FObjectPropertyBase>(Bind,TEXT("Component"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),NC);UI->ProcessEvent(Bind,Args.GetStructMemory());
    auto* Line=CastChecked<URichTextBlock>(UI->GetWidgetFromName(TEXT("Line_Side")));
    auto* D=LoadClass<UDialogue>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Freemove.L3Sewer_Freemove_C"));check(D);
    FDialoguePlayParams Params;Params.StartFromID=TEXT("L3Sewer_Freemove_Comment");
    for(int Pass=0;Pass<2;++Pass)
    {
        Line->SetVisibility(ESlateVisibility::Collapsed);
        check(NC->BeginDialogue(D,Params));
        check(!Line->GetText().IsEmpty() && Line->GetVisibility()==ESlateVisibility::HitTestInvisible);
        check(UI->GetWidgetFromName(TEXT("DialogueBox_Side"))->GetVisibility()==ESlateVisibility::HitTestInvisible);
        check(UI->GetWidgetFromName(TEXT("DialogueBox"))->GetVisibility()==ESlateVisibility::Collapsed);
        NC->ExitDialogue();check(Line->GetVisibility()==ESlateVisibility::Collapsed);
    }
    if(auto* Unbind=UI->FindFunction(TEXT("Unbind from Component"))){FStructOnScope P(Unbind);UI->ProcessEvent(Unbind,P.GetStructMemory());}
    UI->RemoveFromParent();GI->RemoveLocalPlayer(Local);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    UE_LOG(LogTemp,Display,TEXT("EGG RUNTIME PASS: actual Freemove entry displays nonempty Action text without portrait on two successive dialogues; finish hides it"));
}

bool VTGRepairEggDialogue(bool Verify)
{
    auto* Overlay=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay"));
    auto* Manager=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"));
    auto* Still=LoadObject<UDialogueBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Still"));
    check(Overlay && Manager && Still);
    auto Graph=[](UBlueprint* BP,const TCHAR* Name){TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* G:Graphs)if(G->GetName()==Name)return G;return (UEdGraph*)nullptr;};
    auto Node=[](UEdGraph* G,const TCHAR* Name){check(G);for(UEdGraphNode* N:G->Nodes)if(N->GetName()==Name)return N;return (UEdGraphNode*)nullptr;};
    auto Link=[](UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));};
    auto* G=Graph(Overlay,TEXT("On NPC Dialogue Line Started"));
    auto* Text=Node(G,TEXT("K2Node_CallFunction_22"));check(Text);
    auto* Restore=Node(G,TEXT("VTG_RestoreActionLine"));
    auto* MG=Graph(Manager,TEXT("EventGraph"));
    auto* Entry=Node(MG,TEXT("K2Node_CustomEvent_22"));auto* Delay=Node(MG,TEXT("K2Node_CallFunction_48"));check(Entry && Delay);
    auto FindFinal=[](UDialogue* D){for(auto* N:D->GetNodes())if(N->GetID()==TEXT("L3Sewer_Still_DialogueNode_NPC_15"))return N;return (UDialogueNode*)nullptr;};
    auto* Final=FindFinal(Still->DialogueTemplate);check(Final);
    if(!Verify)
    {
        for(UBlueprint* BP:{Overlay,Manager,static_cast<UBlueprint*>(Still)})
        {
            const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
            const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/EggDialogue/Backup")/(BP->GetName()+TEXT(".uasset"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
            if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
        }
        if(!Restore)
        {
            auto* Call=NewObject<UK2Node_CallFunction>(G,TEXT("VTG_RestoreActionLine"));G->AddNode(Call,false,false);Call->CreateNewGuid();
            Call->SetFromFunction(UWidget::StaticClass()->FindFunctionByName(TEXT("SetVisibility")));Call->AllocateDefaultPins();
            Call->NodeComment=TEXT("Restore Action text even when the speaker has no portrait");Call->NodePosX=Text->NodePosX-250;Call->NodePosY=Text->NodePosY;
            Call->FindPinChecked(TEXT("InVisibility"))->DefaultValue=TEXT("HitTestInvisible");
            auto Inputs=Text->FindPinChecked(TEXT("execute"))->LinkedTo;Text->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
            for(auto* P:Inputs)Link(P,Call->FindPinChecked(TEXT("execute")));
            Link(Call->FindPinChecked(TEXT("then")),Text->FindPinChecked(TEXT("execute")));
            check(Text->FindPinChecked(TEXT("self"))->LinkedTo.Num()==1);Link(Text->FindPinChecked(TEXT("self"))->LinkedTo[0],Call->FindPinChecked(TEXT("self")));Restore=Call;
        }
        if(!Node(G,TEXT("VTG_RestoreActionBox")))
        {
            auto* Box=NewObject<UK2Node_VariableGet>(G,TEXT("VTG_ActionBoxRef"));G->AddNode(Box,false,false);Box->CreateNewGuid();Box->VariableReference.SetSelfMember(TEXT("DialogueBox_Side"));Box->AllocateDefaultPins();
            auto* Call=NewObject<UK2Node_CallFunction>(G,TEXT("VTG_RestoreActionBox"));G->AddNode(Call,false,false);Call->CreateNewGuid();Call->SetFromFunction(UWidget::StaticClass()->FindFunctionByName(TEXT("SetVisibility")));Call->AllocateDefaultPins();
            Call->NodeComment=TEXT("Show Action background independently of speaker portrait");Call->NodePosX=Restore->NodePosX-250;Call->NodePosY=Restore->NodePosY;Box->NodePosX=Call->NodePosX;Box->NodePosY=Call->NodePosY+160;
            Call->FindPinChecked(TEXT("InVisibility"))->DefaultValue=TEXT("HitTestInvisible");Link(Box->FindPinChecked(TEXT("DialogueBox_Side")),Call->FindPinChecked(TEXT("self")));
            auto Inputs=Restore->FindPinChecked(TEXT("execute"))->LinkedTo;Restore->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();for(auto* P:Inputs)Link(P,Call->FindPinChecked(TEXT("execute")));Link(Call->FindPinChecked(TEXT("then")),Restore->FindPinChecked(TEXT("execute")));
        }
        Final->bCallBoundEventOnFinish=true;
        Entry->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
        if(auto* Exit=Node(MG,TEXT("K2Node_CallFunction_69")))FBlueprintEditorUtils::RemoveNode(Manager,Exit,true);
        Link(Entry->FindPinChecked(TEXT("then")),Delay->FindPinChecked(TEXT("execute")));
        for(UBlueprint* BP:{Overlay,Manager,static_cast<UBlueprint*>(Still)})
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;
            FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors && BP->Status!=BS_Error);
            FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
            check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
        }
    }
    check(Restore && Restore->FindPinChecked(TEXT("InVisibility"))->DefaultValue==TEXT("HitTestInvisible"));
    check(Node(G,TEXT("VTG_RestoreActionBox")));
    check(Text->FindPinChecked(TEXT("execute"))->LinkedTo.Num()==1 && Text->FindPinChecked(TEXT("execute"))->LinkedTo[0]->GetOwningNode()==Restore);
    check(Restore->FindPinChecked(TEXT("self"))->LinkedTo==Text->FindPinChecked(TEXT("self"))->LinkedTo);
    check(Final->bCallBoundEventOnFinish && FindFinal(CastChecked<UDialogueBlueprintGeneratedClass>(Still->GeneratedClass)->GetDialogueTemplate())->bCallBoundEventOnFinish);
    check(!Node(MG,TEXT("K2Node_CallFunction_69")) && Entry->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Delay->FindPinChecked(TEXT("execute"))));
    check(FMath::IsNearlyEqual(FCString::Atof(*Delay->FindPinChecked(TEXT("Duration"))->DefaultValue),1.5f));
    UE_LOG(LogTemp,Display,TEXT("EGG DIALOGUE PASS: Action text restored independently of portrait; NPC15 finish callback; direct 1.5s handoff; compiled templates verified"));
    if(Verify)TestEggActionText();
    return true;
}


