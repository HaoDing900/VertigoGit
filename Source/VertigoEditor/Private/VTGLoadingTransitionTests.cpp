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
    return Fail==0;
}
