#include "Loading/VTGMediaLoadingPageSystem.h"
#include "VTGLoadingScreenCommandlet.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Widgets/SNullWidget.h"
#include "../../Vertigo/Private/Loading/SVTGLoadingFade.h"

int32 VTGRepairLoadingEntry(const FString& Params)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"));
    if(!BP)return 1;
    UEdGraph* G=BP->UbergraphPages[0];
    UK2Node_CallFunction* Play=nullptr;
    for(UEdGraphNode* N:G->Nodes)
        if(N->GetFName()==TEXT("K2Node_CallFunction_37"))Play=Cast<UK2Node_CallFunction>(N);
    if(!Play)return 2;
    if(!Params.Contains(TEXT("VerifyOnly")) && Play->FunctionReference.GetMemberName()!=TEXT("PlayEntrySequenceWhenReady"))
    {
        auto Sources=Play->FindPinChecked(TEXT("self"))->LinkedTo;
        Play->FindPinChecked(TEXT("self"))->BreakAllPinLinks();
        Play->SetFromFunction(UVTGMediaLoadingPageSystem::StaticClass()->FindFunctionByName(TEXT("PlayEntrySequenceWhenReady")));
        Play->ReconstructNode();
        for(auto* Source:Sources)
            if(!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Source,Play->FindPinChecked(TEXT("Player"))))return 3;
        Play->NodeComment=TEXT("Hold opening shot under loading cover; resume when loading starts revealing the world.");
        Play->bCommentBubbleVisible=true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    if(Play->FunctionReference.GetMemberName()!=TEXT("PlayEntrySequenceWhenReady") || Play->FindPinChecked(TEXT("Player"))->LinkedTo.Num()!=1)return 4;
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
    if(Log.NumErrors)return 5;
    if(!Params.Contains(TEXT("VerifyOnly")))
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        auto File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args))return 6;
    }
    UE_LOG(LogTemp,Display,TEXT("ENTRY HANDOFF PASS: street intro routes to loading reveal gate; Blueprint compiled"));
    return 0;
}

bool VTGTestLoadingEntryHandoff()
{
    auto* GI=NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    auto* W=GI->GetWorld();
    auto* System=GI->GetSubsystem<UVTGMediaLoadingPageSystem>();
    auto* Seq=NewObject<ULevelSequence>();Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,24000*6);
    ALevelSequenceActor* SequenceActor=nullptr;
    auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,FMovieSceneSequencePlaybackSettings(),SequenceActor);
    int Fail=0;
    auto Expect=[&](bool OK,const TCHAR* What){if(!OK)++Fail;UE_LOG(LogTemp,Display,TEXT("ENTRY HANDOFF %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),What);};
    UVTGMediaLoadingPageSystem::PlayEntrySequenceWhenReady(W,Player);
    Expect(Player->IsPlaying(),TEXT("without loading cover play immediately"));
    Player->Stop();
    System->LoadingFade=SNew(SVTGLoadingFade)[SNullWidget::NullWidget];
    UVTGMediaLoadingPageSystem::PlayEntrySequenceWhenReady(W,Player);
    const auto Start=Player->GetCurrentTime();
    Player->Update(10.f);
    Expect(Player->IsPaused() && Player->GetCurrentTime().Time==Start.Time,TEXT("intro remains at first shot through a ten-second loading hold"));
    UVTGMediaLoadingPageSystem::PlayEntrySequenceWhenReady(W,Player);
    Expect(System->PendingEntryPlayers.Num()==1,TEXT("duplicate requests do not restart or queue twice"));
    System->StartPendingEntrySequences();
    Expect(Player->IsPlaying() && Player->GetCurrentTime().Time==Start.Time,TEXT("world reveal resumes at first shot"));
    Player->Update(1.f);
    Expect(Player->GetCurrentTime().Time>Start.Time,TEXT("sequence advances after reveal"));
    Expect(System->PendingEntryPlayers.IsEmpty(),TEXT("reveal drains pending requests"));
    System->ClearLoadingTransition();Player->Stop();
    const uint32 Generation=System->PreloadGeneration;
    System->bPreloadStarted=true;System->bPreloadFinished=true;
    System->ClearLoadingTransition();
    Expect(System->PreloadGeneration!=Generation && !System->bPreloadStarted && !System->bPreloadFinished,TEXT("cleanup invalidates stale async preload completions"));
    {
        TGuardValue<uint64> Frame(GFrameCounter,2);
        System->LoadingFade=SNew(SVTGLoadingFade).InitialBlackHold(0).Duration(0)[SNullWidget::NullWidget];
        System->LoadingFade->Tick(FGeometry(),FPlatformTime::Seconds(),.016f);
        System->bTransitionActive=true;
        System->PendingLevel=TEXT("/Engine/Maps/Entry");
        System->Timing.OpaqueFrame=1;
        System->TickLoadingTransition(.016f);
        Expect(System->bPreloadStarted && !System->PendingLevel.IsNone(),TEXT("destination is preloaded before committing synchronous travel"));
        FlushAsyncLoading();
        Expect(System->bPreloadFinished && System->PreloadedDestination!=nullptr,TEXT("async map preload completes and remains retained for travel"));
        System->CancelLoadingTransition();System->ClearLoadingTransition();
        Expect(!System->PreloadedDestination && System->PendingLevel.IsNone(),TEXT("cancel releases preload and pending destination"));
    }
    GI->Shutdown(); W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return Fail==0;
}
