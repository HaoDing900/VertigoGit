#include "VTGLoadingPauseLocalizationCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

UVTGLoadingPauseLocalizationCommandlet::UVTGLoadingPauseLocalizationCommandlet()
{IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}

int32 UVTGLoadingPauseLocalizationCommandlet::Main(const FString& Params)
{
    const bool Apply=FParse::Param(*Params,TEXT("Apply"));
    const FString Backup=FPaths::ProjectSavedDir()/TEXT("Backups/LoadingPauseLocalization")/FGuid::NewGuid().ToString();
    for(const TCHAR* Path:{TEXT("/Game/Widget/Loading/WBP_VTGLoadingScreen"),TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu")})
    {
        auto* BP=LoadObject<UWidgetBlueprint>(nullptr,Path);
        if(!BP || !BP->WidgetTree)return 1;
        TArray<UTextBlock*> Labels;
        if(BP->GetFName()==TEXT("WBP_VTGLoadingScreen"))Labels.Add(Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("LoadingLabel"))));
        else for(const FName ButtonName:{FName(TEXT("B_Continue")),FName(TEXT("B_Language")),FName(TEXT("B_Exit"))})
        {
            auto* Button=Cast<UButton>(BP->WidgetTree->FindWidget(ButtonName));
            Labels.Add(Button?Cast<UTextBlock>(Button->GetContent()):nullptr);
        }
        bool Changed=false;
        for(auto* Label:Labels)
        {
            if(!Label){UE_LOG(LogTemp,Error,TEXT("Missing requested label in %s"),Path);return 1;}
            const FText Text=Label->GetText();
            const FString Key=FTextInspector::GetKey(Text).Get(FString());
            const auto* Source=FTextInspector::GetSourceString(Text);
            UE_LOG(LogTemp,Display,TEXT("LOADING PAUSE LABEL %s source=%s invariant=%d key=%s"),*Label->GetPathName(),Source?**Source:*Text.ToString(),Text.IsCultureInvariant(),*Key);
            if(!Apply || (!Text.IsCultureInvariant() && !Key.IsEmpty()))continue;
            const FString Origin=Source?*Source:Text.ToString();
            if(Origin.IsEmpty()){UE_LOG(LogTemp,Error,TEXT("Empty label requires an authored Origin"));return 1;}
            Label->Modify();
            Label->SetText(FText::ChangeKey(TEXT("VTG.WidgetLabels"),BP->GetName()+TEXT(".")+Label->GetName(),FText::FromString(Origin)));
            Changed=true;
        }
        if(!Changed)continue;
        const FString File=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*Backup,true);
        if(IFileManager::Get().Copy(*(Backup/FPaths::GetCleanFilename(File)),*File)!=COPY_OK)return 1;
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
        FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
        if(Log.NumErrors)return 1;
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args))return 1;
        UE_LOG(LogTemp,Display,TEXT("LOADING PAUSE LOCALIZATION saved %s; backup %s"),Path,*Backup);
    }
    UE_LOG(LogTemp,Display,TEXT("LOADING PAUSE LOCALIZATION: PASS (%s)"),Apply?TEXT("apply"):TEXT("audit"));
    return 0;
}
