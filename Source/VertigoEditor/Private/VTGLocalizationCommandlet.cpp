#include "VTGLocalizationCommandlet.h"
#include "VTGManualLocalization.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Parse.h"
#include "Engine/Blueprint.h"
#include "Dialogue.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "NarrativeEvent.h"
#include "UObject/UObjectHash.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
UVTGLocalizationCommandlet::UVTGLocalizationCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGLocalizationCommandlet::Main(const FString& Params)
{
    FString Error,Root;
    if(FParse::Value(*Params,TEXT("InspectDialogue="),Root,false))
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,*Root);check(BP && BP->GeneratedClass);
        auto* Runtime=NewObject<UDialogue>(GetTransientPackage(),BP->GeneratedClass);CastChecked<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass)->InitializeDialogue(Runtime);
        TArray<UObject*> Objects;GetObjectsWithPackage(BP->GetOutermost(),Objects,true);TArray<UObject*> RuntimeObjects;GetObjectsWithOuter(Runtime,RuntimeObjects,true);Objects.Append(RuntimeObjects);
        auto Dump=[&](const TCHAR* Stage){for(auto* Object:Objects)if(auto* Node=Cast<UDialogueNode>(Object)){const auto& Text=Node->Line.Text;UE_LOG(LogTemp,Display,TEXT("DIALOGUE_LOC %s path=%s ns=[%s] key=%s source=[%s] display=[%s]"),Stage,*Node->GetPathName(),*FTextInspector::GetNamespace(Text).Get(FString()),*FTextInspector::GetKey(Text).Get(FString()),FTextInspector::GetSourceString(Text)?**FTextInspector::GetSourceString(Text):TEXT(""),*Text.ToString());
            if(auto* Player=Cast<UDialogueNode_Player>(Node)){
                const auto& Hint=Player->GetRawHintText();
                UE_LOG(LogTemp,Display,TEXT("HINT_EFFECTIVE %s path=%s result=[%s] events=%d"),Stage,*Node->GetPathName(),*Player->GetHintText(Runtime).ToString(),Player->Events.Num());
                for(auto* Event:Player->Events)if(Event)UE_LOG(LogTemp,Display,TEXT("HINT_EVENT %s owner=%s class=%s hint=[%s]"),Stage,*Node->GetName(),*Event->GetClass()->GetPathName(),*Event->GetHintText().ToString());
                UE_LOG(LogTemp,Display,TEXT("HINT_LOC %s path=%s ns=[%s] key=%s source=[%s] display=[%s] empty=%d"),Stage,*Node->GetPathName(),*FTextInspector::GetNamespace(Hint).Get(FString()),*FTextInspector::GetKey(Hint).Get(FString()),FTextInspector::GetSourceString(Hint)?**FTextInspector::GetSourceString(Hint):TEXT(""),*Hint.ToString(),Hint.IsEmptyOrWhitespace());
            }
        }};
        Dump(TEXT("before"));auto& Manager=FTextLocalizationManager::Get();Manager.EnableGameLocalizationPreview(TEXT("en-US"));Manager.WaitForAsyncTasks();Dump(TEXT("preview-en"));Manager.RefreshResources();Manager.WaitForAsyncTasks();Dump(TEXT("after-refresh"));
        FTextLocalizationResource Resource;check(Resource.LoadFromFile(FPaths::ProjectContentDir()/TEXT("Localization/Vertigo/en-US/Vertigo.locres"),0));Manager.UpdateFromLocalizationResource(Resource);Dump(TEXT("direct-en"));
        return 0;
    }
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
