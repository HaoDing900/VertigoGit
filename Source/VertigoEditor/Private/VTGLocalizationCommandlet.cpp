#include "VTGLocalizationCommandlet.h"
#include "VTGManualLocalization.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Parse.h"
UVTGLocalizationCommandlet::UVTGLocalizationCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGLocalizationCommandlet::Main(const FString& Params)
{
    FString Error,Root;
    if(FParse::Value(*Params,TEXT("GatherPath="),Root,false))
    {
        auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.SearchAllAssets(true);TArray<FAssetData> Assets;
        TArray<FString> Roots;Root.ParseIntoArray(Roots,TEXT(","),true);
        for(const auto& Path:Roots){TArray<FAssetData> Found;Registry.GetAssetsByPath(FName(Path),Found,true);Assets.Append(Found);}
        int32 Gathered=0;
        for(const auto& Asset:Assets)
        {
            const FString Class=Asset.AssetClassPath.GetAssetName().ToString();
            if(Class!=TEXT("Blueprint") && Class!=TEXT("WidgetBlueprint") && Class!=TEXT("DialogueBlueprint") && Class!=TEXT("QuestBlueprint") && Class!=TEXT("LevelSequence") && Class!=TEXT("DataTable") && Class!=TEXT("StringTable") && !Class.Contains(TEXT("DataAsset")) && !Class.StartsWith(TEXT("PDA_")))continue;
            if(!VTGManualLocalization::GatherAsset(Asset.GetAsset(),Error)){UE_LOG(LogTemp,Error,TEXT("%s"),*Error);return 1;}++Gathered;
        }
        UE_LOG(LogTemp,Display,TEXT("Localization: read-only gather of %d text-bearing assets under %s"),Gathered,*Root);
    }
    if(!VTGManualLocalization::Compile(Error)){UE_LOG(LogTemp,Error,TEXT("%s"),*Error);return 1;}
    UE_LOG(LogTemp,Display,TEXT("MANUAL LOCALIZATION COMPILE PASS (no translations generated)"));return 0;
}
