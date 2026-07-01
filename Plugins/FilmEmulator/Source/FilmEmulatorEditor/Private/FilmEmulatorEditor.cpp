// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorEditor.h"

#include "FilmEmulatorStyle.h"
#include "SFilmEmulatorWindow.h"
#include "FilmEmulatorLUT.h"
#include "UObject/Package.h"
#include "ObjectTools.h"
#include "FilmStockPreset.h"
#include "FilmEmulatorSettings.h"
#include "FilmEmulatorPresetLibrary.h"
#include "Editor.h"
#include "FilmEmulatorLUTAssetTypeActions.h"
#include "FilmEmulatorLUTDetails.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "AutomatedAssetImportData.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "EditorReimportHandler.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"

#define LOCTEXT_NAMESPACE "FFilmEmulatorEditorModule"

namespace FilmEmulatorEditorHelpers
{
UFilmEmulatorLUT* LoadLutAssetByPath(const FString& PackagePath, const FString& AssetName)
{
    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        return nullptr;
    }

    const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
    return Cast<UFilmEmulatorLUT>(StaticLoadObject(UFilmEmulatorLUT::StaticClass(), nullptr, *ObjectPath));
}

void ResolveLutAssetFromPath(const FFilePath& FilePath, TSoftObjectPtr<UFilmEmulatorLUT>& OutAsset)
{
    if (OutAsset.ToSoftObjectPath().IsValid())
    {
        return;
    }

    if (FilePath.FilePath.IsEmpty())
    {
        return;
    }

    const FString BaseName = FPaths::GetBaseFilename(FilePath.FilePath);
    const FString AssetName = ObjectTools::SanitizeObjectName(BaseName);
    if (AssetName.IsEmpty())
    {
        return;
    }

    if (UFilmEmulatorLUT* LutAsset = LoadLutAssetByPath(TEXT("/FilmEmulator/LUTs"), AssetName))
    {
        OutAsset = LutAsset;
        return;
    }

    if (UFilmEmulatorLUT* LutAsset = LoadLutAssetByPath(TEXT("/Game/FilmEmulator/LUTs"), AssetName))
    {
        OutAsset = LutAsset;
    }
}

void CopyPresetData(const UFilmStockPreset* Source, UFilmStockPreset* Dest)
{
    if (!Source || !Dest)
    {
        return;
    }

    Dest->DisplayName = Source->DisplayName;
    Dest->FilmType = Source->FilmType;
    Dest->FilmFormat = Source->FilmFormat;
    Dest->FilmFormatScale = Source->FilmFormatScale;
    Dest->Description = Source->Description;
    Dest->FilmLUTAsset = Source->FilmLUTAsset;
    Dest->FilmLUT = Source->FilmLUT;
    Dest->FilmLUTPath = Source->FilmLUTPath;
    Dest->PrintStrength = Source->PrintStrength;
    Dest->FilmPrintLUTAsset = Source->FilmPrintLUTAsset;
    Dest->FilmPrintLUT = Source->FilmPrintLUT;
    Dest->FilmPrintLUTPath = Source->FilmPrintLUTPath;
    ResolveLutAssetFromPath(Dest->FilmLUTPath, Dest->FilmLUTAsset);
    ResolveLutAssetFromPath(Dest->FilmPrintLUTPath, Dest->FilmPrintLUTAsset);
    Dest->SaturationBias = Source->SaturationBias;
    Dest->ContrastBias = Source->ContrastBias;
    Dest->ExposureBias = Source->ExposureBias;
    Dest->Grain = Source->Grain;
    Dest->GrainDefaults = Source->GrainDefaults;
    Dest->Halation = Source->Halation;
    Dest->GateWeave = Source->GateWeave;
    Dest->Flicker = Source->Flicker;
}


bool IsImportDataOutOfDate(const UAssetImportData* ImportData)
{
    if (!ImportData)
    {
        return false;
    }

    const FString SourceFilename = ImportData->GetFirstFilename();
    if (SourceFilename.IsEmpty() || !FPaths::FileExists(SourceFilename))
    {
        return false;
    }

    const FAssetImportInfo& SourceData = ImportData->GetSourceData();
    if (SourceData.SourceFiles.Num() == 0)
    {
        return false;
    }

    const FAssetImportInfo::FSourceFile& SourceFile = SourceData.SourceFiles[0];
    const FDateTime FileTimestamp = IFileManager::Get().GetTimeStamp(*SourceFilename);
    if (FileTimestamp == FDateTime::MinValue())
    {
        return false;
    }

    if (SourceFile.Timestamp.GetTicks() == 0)
    {
        return true;
    }

    return FileTimestamp > SourceFile.Timestamp;
}
}

void FFilmEmulatorEditorModule::StartupModule()
{
    FFilmEmulatorStyle::Initialize();
    RegisterTabSpawner();
    RegisterAssetTypeActions();
    RegisterDetailCustomizations();

}

void FFilmEmulatorEditorModule::ShutdownModule()
{
    UnregisterTabSpawner();
    UnregisterAssetTypeActions();
    UnregisterDetailCustomizations();
    if (PostEngineInitHandle.IsValid())
    {
        FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
        PostEngineInitHandle.Reset();
    }
    FFilmEmulatorStyle::Shutdown();
}

void FFilmEmulatorEditorModule::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TabName,
        FOnSpawnTab::CreateRaw(this, &FFilmEmulatorEditorModule::OnSpawnFilmEmulatorTab))
        .SetDisplayName(LOCTEXT("FilmEmulatorTabTitle", "Film Emulator"))
        .SetTooltipText(LOCTEXT("FilmEmulatorTabTooltip", "Open Film Emulator controls"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
        .SetIcon(FSlateIcon(FFilmEmulatorStyle::GetStyleSetName(), "FilmEmulator.Icon.Tab"));
}

void FFilmEmulatorEditorModule::UnregisterTabSpawner()
{
    if (FSlateApplication::IsInitialized())
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
    }
}

TSharedRef<SDockTab> FFilmEmulatorEditorModule::OnSpawnFilmEmulatorTab(const FSpawnTabArgs& SpawnTabArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SFilmEmulatorWindow)
        ];
}


void FFilmEmulatorEditorModule::RegisterAssetTypeActions()
{
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    TSharedPtr<IAssetTypeActions> Action = MakeShared<FFilmEmulatorLUTAssetTypeActions>();
    AssetToolsModule.Get().RegisterAssetTypeActions(Action.ToSharedRef());
    RegisteredAssetActions.Add(Action);
}

void FFilmEmulatorEditorModule::UnregisterAssetTypeActions()
{
    if (RegisteredAssetActions.Num() == 0)
    {
        return;
    }

    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
    {
        FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetActions)
        {
            if (Action.IsValid())
            {
                AssetToolsModule.Get().UnregisterAssetTypeActions(Action.ToSharedRef());
            }
        }
    }

    RegisteredAssetActions.Reset();
}

void FFilmEmulatorEditorModule::RegisterDetailCustomizations()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    PropertyModule.RegisterCustomClassLayout(
        UFilmEmulatorLUT::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FFilmEmulatorLUTDetails::MakeInstance));
    PropertyModule.NotifyCustomizationModuleChanged();
}

void FFilmEmulatorEditorModule::UnregisterDetailCustomizations()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        PropertyModule.UnregisterCustomClassLayout(UFilmEmulatorLUT::StaticClass()->GetFName());
        PropertyModule.NotifyCustomizationModuleChanged();
    }
}


void FFilmEmulatorEditorModule::AutoImportAssets()
{
    AutoImportPresetAssets();
}


void FFilmEmulatorEditorModule::AutoImportLUTAssets()
{
    if (!GEditor)
    {
        return;
    }

    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FilmEmulator"));
    if (!Plugin.IsValid())
    {
        return;
    }

    FString SourceDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/LUTs"));
    TArray<FString> Files;
    if (FPaths::DirectoryExists(SourceDir))
    {
        IFileManager::Get().FindFilesRecursive(Files, *SourceDir, TEXT("*.cube"), true, false);
    }

    if (Files.Num() == 0)
    {
        SourceDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/LUTs"));
        Files.Reset();
        if (FPaths::DirectoryExists(SourceDir))
        {
            IFileManager::Get().FindFilesRecursive(Files, *SourceDir, TEXT("*.cube"), true, false);
        }
    }

    if (Files.Num() == 0)
    {
        return;
    }

    const FString DestPath = TEXT("/FilmEmulator/LUTs");

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(*DestPath);
    Filter.ClassPaths.Add(UFilmEmulatorLUT::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;

    TArray<FAssetData> ExistingAssets;
    AssetRegistryModule.Get().GetAssets(Filter, ExistingAssets);

    TMap<FString, FAssetData> ExistingByName;
    for (const FAssetData& Asset : ExistingAssets)
    {
        ExistingByName.Add(Asset.AssetName.ToString(), Asset);
    }

    TArray<FString> ToImport;
    for (const FString& File : Files)
    {
        const FString AssetName = FPaths::GetBaseFilename(File);
        if (const FAssetData* Existing = ExistingByName.Find(AssetName))
        {
            UFilmEmulatorLUT* LutAsset = Cast<UFilmEmulatorLUT>(Existing->GetAsset());
            if (LutAsset && LutAsset->AssetImportData && FilmEmulatorEditorHelpers::IsImportDataOutOfDate(LutAsset->AssetImportData.Get()))
            {
                FReimportManager::Instance()->Reimport(LutAsset, false, true);
            }
            continue;
        }

        ToImport.Add(File);
    }

    if (ToImport.Num() > 0)
    {
        UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
        ImportData->bReplaceExisting = false;
        ImportData->bSkipReadOnly = true;
        ImportData->DestinationPath = DestPath;
        ImportData->Filenames = ToImport;

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
    }
}


void FFilmEmulatorEditorModule::AutoImportPresetAssets()
{
    if (!GEditor)
    {
        return;
    }

    FFilmEmulatorPresetLibrary& Library = FFilmEmulatorPresetLibrary::Get();
    Library.Reload();

    const TArray<FFilmPresetEntry>& Presets = Library.GetPresets();
    if (Presets.Num() == 0)
    {
        return;
    }

    constexpr const TCHAR* TargetPath = TEXT("/FilmEmulator/Presets");

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.ClassPaths.Add(UFilmStockPreset::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.PackagePaths.Add(FName("/FilmEmulator/Presets"));
    Filter.PackagePaths.Add(FName("/Game/FilmEmulator/Presets"));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> ExistingAssets;
    AssetRegistryModule.Get().GetAssets(Filter, ExistingAssets);

    TSet<FName> ExistingIds;
    ExistingIds.Reserve(ExistingAssets.Num());
    for (const FAssetData& Asset : ExistingAssets)
    {
        ExistingIds.Add(Asset.AssetName);
    }

    int32 CreatedCount = 0;

    for (const FFilmPresetEntry& Entry : Presets)
    {
        if (Entry.PresetId.IsNone() || !Entry.Preset.IsValid())
        {
            continue;
        }

        const FString AssetName = ObjectTools::SanitizeObjectName(Entry.PresetId.ToString());
        if (AssetName.IsEmpty())
        {
            continue;
        }

        const FName AssetId(*AssetName);
        if (ExistingIds.Contains(AssetId))
        {
            continue;
        }

        const FString PackageName = FString::Printf(TEXT("%s/%s"), TargetPath, *AssetName);
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            continue;
        }

        UFilmStockPreset* NewPreset = NewObject<UFilmStockPreset>(Package, *AssetName, RF_Public | RF_Standalone);
        if (!NewPreset)
        {
            continue;
        }

        FilmEmulatorEditorHelpers::CopyPresetData(Entry.Preset.Get(), NewPreset);
        NewPreset->PresetId = Entry.PresetId;
        NewPreset->MarkPackageDirty();

        AssetRegistryModule.Get().AssetCreated(NewPreset);
        CreatedCount++;
    }

    if (CreatedCount > 0)
    {
        if (UFilmEmulatorSettings* Settings = GetMutableDefault<UFilmEmulatorSettings>())
        {
            if (!Settings->DefaultPreset.IsValid() && !Settings->DefaultPreset.ToSoftObjectPath().IsValid())
            {
                if (const UFilmStockPreset* DefaultJsonPreset = Library.GetDefaultPreset())
                {
                    if (!DefaultJsonPreset->PresetId.IsNone())
                    {
                        const FString DefaultAssetName = ObjectTools::SanitizeObjectName(DefaultJsonPreset->PresetId.ToString());
                        if (!DefaultAssetName.IsEmpty())
                        {
                            const FString DefaultPackage = FString::Printf(TEXT("%s/%s"), TargetPath, *DefaultAssetName);
                            const FString DefaultObject = FString::Printf(TEXT("%s.%s"), *DefaultPackage, *DefaultAssetName);
                            if (UFilmStockPreset* DefaultAsset = Cast<UFilmStockPreset>(StaticLoadObject(UFilmStockPreset::StaticClass(), nullptr, *DefaultObject)))
                            {
                                Settings->DefaultPreset = DefaultAsset;
                                Settings->SaveSettingsConfig();
                            }
                        }
                    }
                }
            }
        }
    }
}

IMPLEMENT_MODULE(FFilmEmulatorEditorModule, FilmEmulatorEditor)

#undef LOCTEXT_NAMESPACE


