#include "Loading/VTGLevelEntrySequenceComponent.h"
#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_AddDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Widgets/SNullWidget.h"
#include "../../Vertigo/Private/Loading/SVTGLoadingFade.h"

int32 VTGInstallEntryComponent(const FString& Params)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"));
    if(!BP)return 1;
    const bool Verify=Params.Contains(TEXT("VerifyOnly"));
    USimpleConstructionScript* SCS=BP->SimpleConstructionScript;
    auto* Component=SCS->FindSCSNode(TEXT("LevelEntrySequence"));
    UEdGraph* G=BP->UbergraphPages[0];
    if(!Component && !Verify)
    {
        Component=SCS->CreateNode(UVTGLevelEntrySequenceComponent::StaticClass(),TEXT("LevelEntrySequence"));SCS->AddNode(Component);
        auto* Settings=CastChecked<UVTGLevelEntrySequenceComponent>(Component->ComponentTemplate);
        Settings->EntrySequence=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_IntroScene/LS_L2BarStreet_IntroScene"));
        Settings->bPlayOnBeginPlay=false; // Existing stage/checkpoint branches decide whether entry is appropriate.
        Settings->bSkipOnSaveResume=false; // Existing phase resume behavior remains authoritative for this manager.
        Settings->PlaybackSettings.bRestoreState=false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
        UK2Node_CallFunction* OldCreate=nullptr;UK2Node_CallFunction* OldPlay=nullptr;
        for(UEdGraphNode* N:G->Nodes)
        {
            if(N->GetFName()==TEXT("K2Node_CallFunction_38"))OldCreate=Cast<UK2Node_CallFunction>(N);
            if(N->GetFName()==TEXT("K2Node_CallFunction_37"))OldPlay=Cast<UK2Node_CallFunction>(N);
        }
        if(!OldCreate || !OldPlay)return 2;
        const auto* Schema=GetDefault<UEdGraphSchema_K2>();
        auto* Get=NewObject<UK2Node_VariableGet>(G);G->AddNode(Get,false,false);Get->CreateNewGuid();
        Get->VariableReference.SetSelfMember(TEXT("LevelEntrySequence"));Get->AllocateDefaultPins();Get->NodePosX=OldCreate->NodePosX-300;Get->NodePosY=OldCreate->NodePosY+250;
        auto Replace=[&](UK2Node_CallFunction* Old,const TCHAR* Function,const TArray<FName>& Pins)
        {
            auto* Call=NewObject<UK2Node_CallFunction>(G);G->AddNode(Call,false,false);Call->CreateNewGuid();
            Call->SetFromFunction(UVTGLevelEntrySequenceComponent::StaticClass()->FindFunctionByName(Function));Call->AllocateDefaultPins();
            Call->NodePosX=Old->NodePosX;Call->NodePosY=Old->NodePosY;
            for(FName Name:Pins)
            {
                auto* From=Old->FindPinChecked(Name);auto Links=From->LinkedTo;From->BreakAllPinLinks();
                const FName NewName=Name==TEXT("Player")?FName(TEXT("InPlayer")):Name;
                for(auto* Link:Links)check(Schema->TryCreateConnection(Call->FindPinChecked(NewName),Link));
            }
            check(Schema->TryCreateConnection(Get->FindPinChecked(TEXT("LevelEntrySequence")),Call->FindPinChecked(TEXT("self"))));
            FBlueprintEditorUtils::RemoveNode(BP,Old,true);
            return Call;
        };
        Replace(OldCreate,TEXT("CreateEntryPlayer"),{TEXT("execute"),TEXT("then"),TEXT("ReturnValue")});
        Replace(OldPlay,TEXT("PlayEntryPlayer"),{TEXT("execute"),TEXT("then"),TEXT("Player")});
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    if(!Component)return 3;
    auto* EntrySettings=CastChecked<UVTGLevelEntrySequenceComponent>(Component->ComponentTemplate);
    if(!Verify)
    {
        if(!EntrySettings->EntrySequence)EntrySettings->EntrySequence=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_IntroScene/LS_L2BarStreet_IntroScene"));
        EntrySettings->StartPolicy=EVTGEntryStartPolicy::WithLoadingReveal;
        EntrySettings->bPlayOnBeginPlay=false;
        EntrySettings->bSkipOnSaveResume=false;
        for(auto* Node:SCS->GetAllNodes())
            if(Node!=Component)
                if(auto* Extra=Cast<UVTGLevelEntrySequenceComponent>(Node->ComponentTemplate);Extra && !Extra->EntrySequence)
                    Extra->bPlayOnBeginPlay=false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    if(!EntrySettings->EntrySequence || EntrySettings->bPlayOnBeginPlay || EntrySettings->bSkipOnSaveResume)return 8;
    // Assign clears all listeners, including the component's completion bookkeeping. Preserve both listeners.
    UK2Node_AssignDelegate* OldBind=nullptr;
    for(UEdGraphNode* N:G->Nodes)if(N->GetFName()==TEXT("K2Node_AssignDelegate_1"))OldBind=Cast<UK2Node_AssignDelegate>(N);
    if(OldBind && !Verify)
    {
        auto* Bind=NewObject<UK2Node_AddDelegate>(G);G->AddNode(Bind,false,false);Bind->CreateNewGuid();
        Bind->DelegateReference=OldBind->DelegateReference;Bind->AllocateDefaultPins();
        Bind->NodePosX=OldBind->NodePosX;Bind->NodePosY=OldBind->NodePosY;
        for(auto* Pin:OldBind->Pins)if(auto* NewPin=Bind->FindPin(Pin->PinName))
        {
            auto Links=Pin->LinkedTo;Pin->BreakAllPinLinks();
            for(auto* Link:Links)check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(NewPin,Link));
        }
        FBlueprintEditorUtils::RemoveNode(BP,OldBind,true);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    else if(OldBind)return 7;
    int Creates=0,Plays=0;
    for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
    {
        Creates+=C->FunctionReference.GetMemberName()==TEXT("CreateEntryPlayer");
        Plays+=C->FunctionReference.GetMemberName()==TEXT("PlayEntryPlayer");
    }
    if(Creates!=1 || Plays!=1)return 4;
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);if(Log.NumErrors)return 5;
    if(!Verify)
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        const auto Path=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Path,Args))return 6;
    }
    UE_LOG(LogTemp,Display,TEXT("ENTRY COMPONENT PASS: street manager owns configurable component and uses component player creation/playback"));
    return 0;
}

bool VTGTestEntryComponent()
{
    auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
    auto* Loading=GI->GetSubsystem<UVTGMediaLoadingPageSystem>();
    auto* Owner=W->SpawnActor<AActor>();
    auto* C=NewObject<UVTGLevelEntrySequenceComponent>(Owner);Owner->AddInstanceComponent(C);C->RegisterComponent();
    auto* Seq=NewObject<ULevelSequence>();Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,24000*6);
    C->EntrySequence=Seq;C->bPlayOnBeginPlay=false;C->bSkipOnSaveResume=false;
    int Fail=0;auto Expect=[&](bool OK,const TCHAR* What){if(!OK)++Fail;UE_LOG(LogTemp,Display,TEXT("ENTRY COMPONENT %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),What);};
    Expect(C->PlayEntry() && C->Player->IsPlaying(),TEXT("no loading cover starts configured asset immediately"));
    Expect(!C->PlayEntry(),TEXT("duplicate active requests are ignored"));
    C->SkipEntry();Expect(!C->bRequested && !C->Player->IsPlaying(),TEXT("skip stops playback and releases request"));
    Loading->LoadingFade=SNew(SVTGLoadingFade)[SNullWidget::NullWidget];
    C->StartPolicy=EVTGEntryStartPolicy::AfterLoadingReveal;
    C->PlayEntry();auto* P=C->GetEntryPlayer();
    Loading->OnWorldRevealStarted.Broadcast();Expect(!P->IsPlaying(),TEXT("after-reveal policy waits through opening fade"));
    Loading->OnLoadingPageHidden.Broadcast();Expect(P->IsPlaying() && C->bStarted,TEXT("after-reveal policy starts when cover disappears"));
    C->SkipEntry();
    C->StartPolicy=EVTGEntryStartPolicy::WithLoadingReveal;C->PlayEntry();P=C->GetEntryPlayer();
    P->Update(10.f);Expect(!P->IsPlaying() && P->GetCurrentTime().Time==P->GetStartTime().Time,TEXT("waiting does not evaluate or advance the sequence"));
    Loading->OnWorldRevealStarted.Broadcast();Expect(P->IsPlaying(),TEXT("with-reveal policy starts on reveal signal"));
    P->Update(7.f);Expect(!C->bRequested,TEXT("natural finish releases component and permits replay"));
    C->bSkipOnSaveResume=true;C->bResumingSave=true;
    Expect(!C->PlayEntry(),TEXT("saved-game resume suppresses entry"));
    C->bResumingSave=false;C->bSkipOnSaveResume=false;
    Expect(C->PlayEntry(),TEXT("entry can be replayed after completion"));Loading->OnLoadingCancelled.Broadcast();
    Expect(!C->bRequested,TEXT("cancel clears pending request"));
    C->DestroyComponent();
    GI->Shutdown();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return Fail==0;
}
