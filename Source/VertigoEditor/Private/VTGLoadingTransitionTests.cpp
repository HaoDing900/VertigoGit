#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

int32 VTGConnectLevelLoading(const FString& Params)
{
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    FARFilter Filter;
    Filter.PackagePaths={TEXT("/Game/LevelNSequence/In_Level"),TEXT("/Game/Blueprints/Actors/Bed"),TEXT("/Game/2DArt/ScreenTransition")};
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths=true; Filter.bRecursiveClasses=true;
    TArray<FAssetData> Assets; Registry.GetAssets(Filter,Assets);
    const bool Apply=FParse::Param(*Params,TEXT("Apply"));
    int32 ChangedAssets=0,ChangedNodes=0;
    for(const auto& Asset:Assets)
    {
        const auto Name=Asset.AssetName.ToString();
        if(!Name.StartsWith(TEXT("BPLM_")) && !Name.StartsWith(TEXT("BP_Bed_")) && !Name.StartsWith(TEXT("WBP_")))continue;
        auto* BP=Cast<UBlueprint>(Asset.GetAsset()); if(!BP)continue;
        int32 Count=0;
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
            if(auto* Call=Cast<UK2Node_CallFunction>(Node))
            {
                const auto Function=Call->FunctionReference.GetMemberName();
                if(Function==TEXT("OpenLevel") || Function==TEXT("OpenLevelBySoftObjectPtr"))
                {
                    UE_LOG(LogTemp,Display,TEXT("TRAVEL ENTRY: %s %s %s"),*BP->GetPathName(),*Graph->GetName(),*Function.ToString());
                    ++Count;
                    if(Apply)
                    {
                        const TCHAR* Replacement=Function==TEXT("OpenLevel")?TEXT("OpenLevelWithLoadingScreenByName"):TEXT("OpenLevelWithLoadingScreen");
                        Call->SetFromFunction(UVTGMediaLoadingPageSystem::StaticClass()->FindFunctionByName(Replacement));
                        Call->ReconstructNode();
                    }
                }
            }
        if(!Count || !Apply)continue;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
        if(Log.NumErrors)return 80;
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        const auto Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Args))return 81;
        ++ChangedAssets; ChangedNodes+=Count;
    }
    UE_LOG(LogTemp,Display,TEXT("TRAVEL ENTRY AUDIT: apply=%d changed assets=%d nodes=%d"),Apply,ChangedAssets,ChangedNodes);
    return 0;
}

bool VTGTestLoadingTransitionTiming()
{
    int32 Fail=0;
    auto Expect=[&](bool OK,const TCHAR* Message){Fail+=!OK; UE_LOG(LogTemp,Display,TEXT("LOADING TIMING %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),Message);};
    FVTGLoadingTransitionTiming Slow;
    Expect(!Slow.CanTravel(10,false),TEXT("travel cannot begin during fade-in"));
    Expect(!Slow.CanTravel(11,true) && !Slow.CanTravel(11,true) && Slow.CanTravel(12,true),TEXT("even zero-duration fade waits for an opaque rendered frame"));
    Slow.MarkVisible(0.0);
    Expect(!Slow.CanReveal(20.0,30,2.5,.5),TEXT("long loading cannot reveal before the map is ready"));
    Slow.MarkReady(20.0,30);
    Expect(!Slow.CanReveal(20.2,31,2.5,.5) && Slow.CanReveal(20.5,32,2.5,.5),TEXT("long load retains its screen for the post-load hold"));
    FVTGLoadingTransitionTiming Fast; Fast.MarkVisible(1.0); Fast.MarkReady(1.2,100);
    Expect(!Fast.CanReveal(1.8,101,2.5,.5) && Fast.CanReveal(3.5,102,2.5,.5),TEXT("instant loads respect the minimum display time"));
    FVTGLoadingTransitionTiming Zero; Zero.MarkVisible(5.0); Zero.MarkReady(5.0,200);
    Expect(!Zero.CanReveal(5.0,200,0,0) && Zero.CanReveal(5.0,201,0,0),TEXT("zero hold still waits for a frame in the destination"));
    FVTGLoadingTransitionTiming Unseen; Unseen.MarkReady(1.0,1);
    Expect(!Unseen.CanReveal(10.0,10,0,0),TEXT("screen never shown cannot immediately fade away"));
    FVTGLoadingBlackoutTiming Blackout;
    Expect(Blackout.CoverOpacity(0)==1 && Blackout.ImageOpacity(0)==0,TEXT("first frame is opaque black; old world and loading image are both covered"));
    Blackout.StartedAt=0;
    Expect(Blackout.ImageOpacity(1.49)==0 && Blackout.CoverOpacity(1.49)==1,TEXT("initial black holds for 1.5 seconds"));
    Expect(FMath::IsNearlyEqual(Blackout.ImageOpacity(2.0),.5f) && !Blackout.IsImageVisible(2.0),TEXT("image is revealed smoothly over the configured one second"));
    Expect(Blackout.IsImageVisible(2.5) && Blackout.ImageOpacity(2.5)==1,TEXT("loading image is fully shown before deferred travel"));
    FVTGLoadingTransitionTiming ImageMinimum;ImageMinimum.MarkVisible(Blackout.ImageVisibleAt());ImageMinimum.MarkReady(2.6,10);
    Expect(!ImageMinimum.CanReveal(4.99,11,2.5,0) && ImageMinimum.CanReveal(5,12,2.5,0),TEXT("initial black and its fade do not consume minimum image display time"));
    Blackout.ClosingAt=10;
    Expect(FMath::IsNearlyEqual(Blackout.ImageOpacity(10.75),.5f) && Blackout.CoverOpacity(10.75)==1,TEXT("completed loading image fades to black over 1.5 seconds without exposing world"));
    Expect(Blackout.ImageOpacity(11.5)==0 && !Blackout.CanReveal(12.99) && Blackout.CanReveal(13),TEXT("full black holds another 1.5 seconds before world reveal is eligible"));
    Expect(Blackout.CoverOpacity(99)==1 && !Blackout.IsFinished(99),TEXT("slow readiness keeps screen black indefinitely without a timed premature reveal"));
    Blackout.RevealingAt=15;
    Expect(FMath::IsNearlyEqual(Blackout.CoverOpacity(15.5),.5f) && !Blackout.IsFinished(15.5) && Blackout.IsFinished(16),TEXT("ready world is revealed in one second, then overlay can be removed"));
    FVTGLoadingReadinessTiming Stable;
    Expect(!Stable.Update(false,0,1) && !Stable.Update(true,1,2) && !Stable.Update(true,1.1,3),TEXT("map readiness must settle across real frames"));
    Expect(!Stable.Update(false,1.2,4) && !Stable.Update(true,2,5) && Stable.Update(true,2.25,6),TEXT("new streaming or save restore work resets the readiness stability gate"));
    FVTGLoadingBlackoutTiming ZeroBlack;ZeroBlack.InitialHold=ZeroBlack.OpeningFade=ZeroBlack.ClosingFade=ZeroBlack.BlackHold=ZeroBlack.RevealFade=0;ZeroBlack.StartedAt=0;ZeroBlack.ClosingAt=1;ZeroBlack.RevealingAt=2;
    Expect(ZeroBlack.IsFinished(2) && ZeroBlack.CoverOpacity(2)==0,TEXT("all zero-duration settings remain well-defined"));
    return Fail==0;
}
