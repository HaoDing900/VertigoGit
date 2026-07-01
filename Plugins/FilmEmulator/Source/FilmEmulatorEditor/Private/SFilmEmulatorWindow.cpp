// Copyright 2026 TOXIC STOCK All rights reserved.

#include "SFilmEmulatorWindow.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "FilmEmulatorPresetLibrary.h"
#include "FilmEmulatorSettings.h"
#include "FilmEmulatorLUT.h"
#include "FilmStockPreset.h"
#include "FilmEmulatorStyle.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "FilmEmulatorWindow"

DEFINE_LOG_CATEGORY_STATIC(LogFilmEmulatorEditorWindow, Log, All);

namespace FilmEmulatorUI
{
    const float SectionPadding = 8.0f;
    const float LabelWidth = 160.0f;
}

namespace
{
TWeakPtr<IDetailsView> GFilmEmulatorSettingsDetails;
TWeakPtr<IDetailsView> GFilmEmulatorPresetDetails;

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
    Dest->GateScratch = Source->GateScratch;
    Dest->Dirt = Source->Dirt;
}

void ResolveHalationTints(const FFilmHalationSettings& Halation, FLinearColor& OutNear, FLinearColor& OutMid, FLinearColor& OutFar)
{
    auto ApplyFallback = [](const FLinearColor& Target, const FLinearColor& Fallback)
    {
        const float Sum = Target.R + Target.G + Target.B;
        if (Target.A <= 0.0f && Sum <= KINDA_SMALL_NUMBER)
        {
            return Fallback;
        }
        return Target;
    };

    const FLinearColor BaseTint = Halation.Tint;
    const FLinearColor NearDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.6f, 0.25f, 1.0f), 0.45f);
    const FLinearColor MidDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.35f, 0.12f, 1.0f), 0.45f);
    const FLinearColor FarDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.2f, 0.05f, 1.0f), 0.7f);

    OutNear = ApplyFallback(Halation.TintNear, NearDefault);
    OutMid = ApplyFallback(Halation.TintMid, MidDefault);
    OutFar = ApplyFallback(Halation.TintFar, FarDefault);
}

ECheckBoxState ToCheckBoxState(bool bValue)
{
    return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool IsChecked(ECheckBoxState State)
{
    return State == ECheckBoxState::Checked;
}
void AutoImportLUTAssets()
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
            if (LutAsset && LutAsset->AssetImportData && IsImportDataOutOfDate(LutAsset->AssetImportData.Get()))
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

void AutoImportPresetAssets()
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

        CopyPresetData(Entry.Preset.Get(), NewPreset);
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
} // namespace

void SFilmEmulatorWindow::Construct(const FArguments& InArgs)
{
    RebuildPresetOptions();
    RebuildPrintProfileOptions();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Background"))
        .Padding(4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("FilmEmulatorTitle", "FILM EMULATOR"))
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Title"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("FilmEmulatorSubtitle", "Color emulation before tonemapping"))
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
                        .Padding(12.0f, 8.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([this]()
                                {
                                    return GetEnableState() == ECheckBoxState::Checked
                                        ? LOCTEXT("FilmEmulatorOn", "FILM EMULATION  ON")
                                        : LOCTEXT("FilmEmulatorOff", "FILM EMULATION  OFF");
                                })
                                .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Bold"))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    return GetEnableState() == ECheckBoxState::Checked
                                        ? FLinearColor(0.2f, 0.85f, 0.2f, 1.0f)
                                        : FLinearColor(0.85f, 0.2f, 0.2f, 1.0f);
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SCheckBox)
                                .IsChecked(this, &SFilmEmulatorWindow::GetEnableState)
                                .OnCheckStateChanged(this, &SFilmEmulatorWindow::OnEnableChanged)
                            ]
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        BuildFilmStockTab()
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Top)
            .Padding(12.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(240.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
                    .Padding(FilmEmulatorUI::SectionPadding)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("FilmEmulatorToolsHeader", "TOOLS"))
                            .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                            .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
                            [
                                SNew(SButton)
                                .OnClicked(this, &SFilmEmulatorWindow::OnCheckLutsClicked)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("FilmEmulatorCheckLutsButton", "Check LUTs"))
                                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                                    .Justification(ETextJustify::Center)
                                ]
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SNew(SButton)
                                .OnClicked(this, &SFilmEmulatorWindow::OnCheckPresetsClicked)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("FilmEmulatorCheckPresetsButton", "Check Presets"))
                                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                                    .Justification(ETextJustify::Center)
                                ]
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
                            [
                                SNew(SButton)
                                .OnClicked(this, &SFilmEmulatorWindow::OnSaveConfigClicked)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("FilmEmulatorSaveConfigButton", "Save Config"))
                                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                                    .Justification(ETextJustify::Center)
                                ]
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SNew(SButton)
                                .OnClicked(this, &SFilmEmulatorWindow::OnReloadConfigClicked)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("FilmEmulatorReloadConfigButton", "Reload"))
                                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                                    .Justification(ETextJustify::Center)
                                ]
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            SNew(SButton)
                            .IsEnabled(this, &SFilmEmulatorWindow::CanResetPresetFromJson)
                            .OnClicked(this, &SFilmEmulatorWindow::OnResetPresetFromJsonClicked)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("FilmEmulatorResetToJsonButton", "Reset to JSON"))
                                .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                                .Justification(ETextJustify::Center)
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SFilmEmulatorWindow::BuildFilmStockTab()
{
    auto MakeFloatRow = [&](const FText& Label, TAttribute<float> Value, const SSpinBox<float>::FOnValueChanged& OnChanged, float Min, float Max, float Step, TAttribute<bool> Enabled)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            .IsEnabled(Enabled)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SSpinBox<float>)
                    .Value(Value)
                    .OnValueChanged(OnChanged)
                    .MinValue(Min)
                    .MaxValue(Max)
                    .MinSliderValue(Min)
                    .MaxSliderValue(Max)
                    .Delta(Step)
                    .MinDesiredWidth(180.0f)
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                ]
            ];
    };

    auto MakeBoolRow = [&](const FText& Label, TAttribute<ECheckBoxState> State, const FOnCheckStateChanged& OnChanged, TAttribute<bool> Enabled)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            .IsEnabled(Enabled)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked(State)
                    .OnCheckStateChanged(OnChanged)
                ]
            ];
    };

    auto MakeInfoRow = [&](const FText& Label, TAttribute<FText> Value)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(Value)
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                    .ColorAndOpacity(FLinearColor::White)
                ]
            ];
    };

    auto MakeTextureRow = [&](const FText& Label, TAttribute<FString> Path, const FOnSetObject& OnChanged, TAttribute<bool> Enabled)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            .IsEnabled(Enabled)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SObjectPropertyEntryBox)
                    .AllowedClass(UTexture2D::StaticClass())
                    .ObjectPath(Path)
                    .OnObjectChanged(OnChanged)
                ]
            ];
    };

    const TAttribute<bool> PresetEditable = TAttribute<bool>(this, &SFilmEmulatorWindow::IsPresetEditable);
    const TAttribute<bool> GateScratchPolarityEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
    {
        return IsPresetEditable() && IsGateScratchPolarityEditable();
    }));
    const TAttribute<bool> DirtPolarityEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
    {
        return IsPresetEditable() && IsDirtPolarityEditable();
    }));
    const TAttribute<bool> DirtTextureEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
    {
        return IsPresetEditable() && IsDirtTextureEnabled();
    }));

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmStockHeader", "FILM STOCK"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
                    .Padding(FMargin(8.0f, 6.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SBox)
                            .WidthOverride(FilmEmulatorUI::LabelWidth)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("FilmStockLabel", "Film Stock Preset"))
                                .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                                .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                            ]
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SAssignNew(PresetComboBox, SComboBox<TSharedPtr<FFilmPresetOption>>)
                            .OptionsSource(&PresetOptions)
                            .OnSelectionChanged(this, &SFilmEmulatorWindow::OnPresetChanged)
                            .OnGenerateWidget(this, &SFilmEmulatorWindow::MakePresetWidget)
                            .InitiallySelectedItem(SelectedPreset)
                            .Content()
                            [
                                SNew(STextBlock)
                                .Text(this, &SFilmEmulatorWindow::GetSelectedPresetText)
                                .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                            ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    MakeInfoRow(LOCTEXT("FilmPresetSourceLabel", "Source"), TAttribute<FText>(this, &SFilmEmulatorWindow::GetSelectedPresetSourceText))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeInfoRow(LOCTEXT("FilmPresetTypeLabel", "Film Type"), TAttribute<FText>(this, &SFilmEmulatorWindow::GetSelectedPresetTypeText))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeInfoRow(LOCTEXT("FilmPresetFormatLabel", "Film Format"), TAttribute<FText>(this, &SFilmEmulatorWindow::GetSelectedPresetFormatText))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeInfoRow(LOCTEXT("FilmPresetLutLabel", "Film LUT"), TAttribute<FText>(this, &SFilmEmulatorWindow::GetSelectedPresetLUTText))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
                    .Padding(FMargin(8.0f, 6.0f))
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]()
                        {
                            if (const UFilmStockPreset* Preset = GetSelectedPresetForDisplay())
                            {
                                return Preset->Description;
                            }
                            return FText::GetEmpty();
                        })
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                        .AutoWrapText(true)
                    ]
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            BuildFilmPrintTab()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmGlobalTuningHeader", "GLOBAL TUNING"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmStrengthLabel", "Strength"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetStrength),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnStrengthChanged),
                        0.0f, 1.0f, 0.01f, TAttribute<bool>(true))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmExposureEVLabel", "Exposure (EV)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetExposureEV),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnExposureEVChanged),
                        -5.0f, 5.0f, 0.05f, TAttribute<bool>(true))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmSaturationLabel", "Saturation"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetSaturation),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnSaturationChanged),
                        0.0f, 2.0f, 0.01f, TAttribute<bool>(true))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmContrastLabel", "Contrast"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetContrast),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnContrastChanged),
                        0.0f, 2.0f, 0.01f, TAttribute<bool>(true))
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmProfileTuningHeader", "PROFILE TUNING"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmFormatScaleLabel", "Film Format Scale"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetFormatScale),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnFormatScaleChanged),
                        0.25f, 10.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmExposureBiasLabel", "Exposure Bias"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetPresetExposureBias),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnPresetExposureBiasChanged),
                        -5.0f, 5.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmSaturationBiasLabel", "Saturation Bias"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetPresetSaturationBias),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnPresetSaturationBiasChanged),
                        0.0f, 2.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmContrastBiasLabel", "Contrast Bias"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetPresetContrastBias),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnPresetContrastBiasChanged),
                        0.0f, 2.0f, 0.01f, PresetEditable)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmHalationHeader", "HALATION"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmHalationEnabledLabel", "Enabled"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetHalationEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationEnabledChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationIntensityLabel", "Intensity"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationIntensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationIntensityChanged),
                        0.0f, 10.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationRadiusLabel", "Radius"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationRadius),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationRadiusChanged),
                        0.0f, 10.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationThresholdLabel", "Threshold"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationThreshold),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationThresholdChanged),
                        0.0f, 10.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintRLabel", "Tint R"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintRChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintGLabel", "Tint G"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintG),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintGChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintBLabel", "Tint B"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintB),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintBChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationRadiusMidLabel", "Mid Radius Scale"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationRadiusMidScale),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationRadiusMidScaleChanged),
                        1.0f, 8.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationRadiusFarLabel", "Far Radius Scale"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationRadiusFarScale),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationRadiusFarScaleChanged),
                        1.0f, 8.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintNearRLabel", "Tint Near R"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintNearR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintNearRChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintNearGLabel", "Tint Near G"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintNearG),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintNearGChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintNearBLabel", "Tint Near B"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintNearB),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintNearBChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintMidRLabel", "Tint Mid R"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintMidR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintMidRChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintMidGLabel", "Tint Mid G"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintMidG),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintMidGChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintMidBLabel", "Tint Mid B"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintMidB),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintMidBChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintFarRLabel", "Tint Far R"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintFarR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintFarRChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintFarGLabel", "Tint Far G"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintFarG),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintFarGChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmHalationTintFarBLabel", "Tint Far B"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetHalationTintFarB),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnHalationTintFarBChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmGrainHeader", "FILM GRAIN"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmGrainEnabledLabel", "Enabled"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetGrainEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainEnabledChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainIsoLabel", "ISO"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainISO),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainISOChanged),
                        GetGrainIsoMin(), GetGrainIsoMax(), 1.0f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainIntensityLabel", "Intensity"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainIntensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainIntensityChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainSizeLabel", "Size (microns)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainSize),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainSizeChanged),
                        4.0f, 80.0f, 0.1f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainSigmaLabel", "Radius Sigma"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainSigmaR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainSigmaRChanged),
                        0.0f, 2.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainFilterSigmaLabel", "Filter Sigma"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainFilterSigma),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainFilterSigmaChanged),
                        0.0f, 3.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainChromaticLabel", "Chromatic"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainChromatic),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainChromaticChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainResponseLabel", "Response"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainResponse),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainResponseChanged),
                        0.0f, 2.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGrainAnimLabel", "Animation Amplitude"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGrainAnimationAmplitude),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGrainAnimationAmplitudeChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmFlickerHeader", "EXPOSURE FLICKER"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmFlickerEnabledLabel", "Enabled"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetFlickerEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnFlickerEnabledChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmFlickerIntensityLabel", "Intensity (EV)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetFlickerIntensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnFlickerIntensityChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmFlickerFrequencyLabel", "Frequency (Hz)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetFlickerFrequency),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnFlickerFrequencyChanged),
                        0.0f, 12.0f, 0.05f, PresetEditable)
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmGateWeaveHeader", "GATE WEAVE"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmGateWeaveEnabledLabel", "Enabled"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetGateWeaveEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateWeaveEnabledChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateWeaveAmplitudeLabel", "Amplitude (mm)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateWeaveAmplitude),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateWeaveAmplitudeChanged),
                        0.0f, 0.5f, 0.001f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateWeaveFrequencyLabel", "Frequency (Hz)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateWeaveFrequency),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateWeaveFrequencyChanged),
                        0.0f, 12.0f, 0.05f, PresetEditable)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmGateScratchHeader", "GATE SCRATCH"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmGateScratchEnabledLabel", "Enabled"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetGateScratchEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchEnabledChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchIntensityLabel", "Intensity"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchIntensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchIntensityChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchDensityLabel", "Density"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchDensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchDensityChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchWidthLabel", "Width (microns)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchWidth),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchWidthChanged),
                        0.5f, 20.0f, 0.1f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchWidthJitterLabel", "Width Jitter"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchWidthJitter),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchWidthJitterChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchLengthLabel", "Length"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchLength),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchLengthChanged),
                        0.1f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchLengthJitterLabel", "Length Jitter"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchLengthJitter),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchLengthJitterChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchOpacityJitterLabel", "Opacity Jitter"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchOpacityJitter),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchOpacityJitterChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchFrequencyLabel", "Frequency (Hz)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchFrequency),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchFrequencyChanged),
                        0.0f, 12.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmGateScratchAutoPolarityLabel", "Auto Polarity"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetGateScratchAutoPolarityState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchAutoPolarityChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchPolarityLabel", "Polarity"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchPolarity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchPolarityChanged),
                        -1.0f, 1.0f, 0.01f, GateScratchPolarityEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchTintRLabel", "Tint R"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchTintR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchTintRChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchTintGLabel", "Tint G"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchTintG),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchTintGChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmGateScratchTintBLabel", "Tint B"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetGateScratchTintB),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnGateScratchTintBChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmDamageHeader", "FILM DAMAGE"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmDamageEnabledLabel", "Enabled"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetDirtEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtEnabledChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageIntensityLabel", "Intensity"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtIntensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtIntensityChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageDensityLabel", "Density"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtDensity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtDensityChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageSizeLabel", "Size (microns)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtSize),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtSizeChanged),
                        2.0f, 2000.0f, 1.0f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageSizeJitterLabel", "Size Jitter"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtSizeJitter),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtSizeJitterChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageOpacityJitterLabel", "Opacity Jitter"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtOpacityJitter),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtOpacityJitterChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageSoftnessLabel", "Softness"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtSoftness),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtSoftnessChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageFrequencyLabel", "Frequency (Hz)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtFrequency),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtFrequencyChanged),
                        0.0f, 12.0f, 0.05f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmDamageAutoPolarityLabel", "Auto Polarity"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetDirtAutoPolarityState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtAutoPolarityChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamagePolarityLabel", "Polarity"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtPolarity),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtPolarityChanged),
                        -1.0f, 1.0f, 0.01f, DirtPolarityEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageTintRLabel", "Tint R"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtTintR),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtTintRChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageTintGLabel", "Tint G"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtTintG),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtTintGChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageTintBLabel", "Tint B"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtTintB),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtTintBChanged),
                        0.0f, 1.0f, 0.01f, PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmDamageUseTextureLabel", "Use Texture Based"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetDirtUseTextureState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtUseTextureChanged),
                        PresetEditable)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmDamageInvertTextureLabel", "Invert Texture"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetDirtInvertTextureState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtInvertTextureChanged),
                        DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeTextureRow(LOCTEXT("FilmDamageTextureLabel", "Damage Texture"),
                        TAttribute<FString>(this, &SFilmEmulatorWindow::GetDirtTexturePath),
                        FOnSetObject::CreateSP(this, &SFilmEmulatorWindow::OnDirtTextureChanged),
                        DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageTextureTilingLabel", "Texture Tiling"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtTextureTiling),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtTextureTilingChanged),
                        0.1f, 8.0f, 0.05f, DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageTextureScaleMinLabel", "Texture Scale Min"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtTextureScaleMin),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtTextureScaleMinChanged),
                        0.1f, 8.0f, 0.05f, DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageTextureScaleMaxLabel", "Texture Scale Max"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtTextureScaleMax),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtTextureScaleMaxChanged),
                        0.1f, 8.0f, 0.05f, DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageNoiseScaleLabel", "Noise Scale"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtNoiseScale),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtNoiseScaleChanged),
                        0.1f, 12.0f, 0.05f, DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageNoiseStrengthLabel", "Noise Strength"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtNoiseStrength),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtNoiseStrengthChanged),
                        0.0f, 1.0f, 0.01f, DirtTextureEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmDamageNoiseSpeedLabel", "Noise Speed"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetDirtNoiseSpeed),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnDirtNoiseSpeedChanged),
                        0.0f, 5.0f, 0.05f, DirtTextureEnabled)
                ]
            ]
        ];
}

TSharedRef<SWidget> SFilmEmulatorWindow::BuildFilmPrintTab()
{
    auto MakeFloatRow = [&](const FText& Label, TAttribute<float> Value, const SSpinBox<float>::FOnValueChanged& OnChanged, float Min, float Max, float Step, TAttribute<bool> Enabled)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            .IsEnabled(Enabled)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SSpinBox<float>)
                    .Value(Value)
                    .OnValueChanged(OnChanged)
                    .MinValue(Min)
                    .MaxValue(Max)
                    .MinSliderValue(Min)
                    .MaxSliderValue(Max)
                    .Delta(Step)
                    .MinDesiredWidth(180.0f)
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                ]
            ];
    };

    auto MakeBoolRow = [&](const FText& Label, TAttribute<ECheckBoxState> State, const FOnCheckStateChanged& OnChanged, TAttribute<bool> Enabled)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            .IsEnabled(Enabled)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked(State)
                    .OnCheckStateChanged(OnChanged)
                ]
            ];
    };

    auto MakeInfoRow = [&](const FText& Label, TAttribute<FText> Value)
    {
        return SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FMargin(8.0f, 6.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(FilmEmulatorUI::LabelWidth)
                    [
                        SNew(STextBlock)
                        .Text(Label)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(Value)
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                    .ColorAndOpacity(FLinearColor::White)
                ]
            ];
    };

    const TAttribute<bool> PrintEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]()
    {
        return GetFilmPrintEnabledState() == ECheckBoxState::Checked;
    }));

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
            .Padding(FilmEmulatorUI::SectionPadding)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FilmPrintHeader", "FILM PRINT"))
                    .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Header"))
                    .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.Accent"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
                    .Padding(FMargin(8.0f, 6.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SBox)
                            .WidthOverride(FilmEmulatorUI::LabelWidth)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("FilmPrintProfileLabel", "Print Profile"))
                                .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                                .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                            ]
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SAssignNew(PrintProfileComboBox, SComboBox<TSharedPtr<FName>>)
                            .OptionsSource(&PrintProfileOptions)
                            .OnSelectionChanged(this, &SFilmEmulatorWindow::OnPrintProfileChanged)
                            .OnGenerateWidget(this, &SFilmEmulatorWindow::MakePrintProfileWidget)
                            .InitiallySelectedItem(SelectedPrintProfile)
                            .Content()
                            [
                                SNew(STextBlock)
                                .Text(this, &SFilmEmulatorWindow::GetSelectedPrintProfileText)
                                .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"))
                            ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    MakeInfoRow(LOCTEXT("FilmPrintLutLabel", "Print LUT"), TAttribute<FText>(this, &SFilmEmulatorWindow::GetSelectedPrintProfileLUTText))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FFilmEmulatorStyle::Get().GetBrush("FilmEmulator.Brush.Section"))
                    .Padding(FMargin(8.0f, 6.0f))
                    [
                        SNew(STextBlock)
                        .Text(this, &SFilmEmulatorWindow::GetSelectedPrintProfileDescription)
                        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Small"))
                        .ColorAndOpacity(FFilmEmulatorStyle::Get().GetColor("FilmEmulator.Color.TextDim"))
                        .AutoWrapText(true)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    MakeBoolRow(LOCTEXT("FilmPrintEnabledLabel", "Use Film Print"),
                        TAttribute<ECheckBoxState>(this, &SFilmEmulatorWindow::GetFilmPrintEnabledState),
                        FOnCheckStateChanged::CreateSP(this, &SFilmEmulatorWindow::OnFilmPrintEnabledChanged),
                        TAttribute<bool>(true))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmPrintStrengthLabel", "Print Strength"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetPrintStrength),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnPrintStrengthChanged),
                        0.0f, 1.0f, 0.01f, PrintEnabled)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    MakeFloatRow(LOCTEXT("FilmPrintExposureLabel", "Print Exposure (EV)"),
                        TAttribute<float>(this, &SFilmEmulatorWindow::GetPrintExposureEV),
                        SSpinBox<float>::FOnValueChanged::CreateSP(this, &SFilmEmulatorWindow::OnPrintExposureEVChanged),
                        -5.0f, 5.0f, 0.05f, PrintEnabled)
                ]
            ]
        ];
}

void SFilmEmulatorWindow::RebuildPresetOptions()
{
    PresetOptions.Reset();
    SelectedPreset.Reset();

    UFilmEmulatorSettings* Settings = GetSettings();
    const FSoftObjectPath SelectedAssetPath = Settings ? Settings->DefaultPreset.ToSoftObjectPath() : FSoftObjectPath();
    const FName SelectedPresetId = Settings ? Settings->DefaultPresetId : NAME_None;

    auto AddSeparator = [&](const FText& Label)
    {
        TSharedPtr<FFilmPresetOption> Separator = MakeShared<FFilmPresetOption>();
        Separator->bIsSeparator = true;
        Separator->SeparatorLabel = Label;
        PresetOptions.Add(Separator);
    };

    auto AddOption = [&](const TSharedPtr<FFilmPresetOption>& Option)
    {
        PresetOptions.Add(Option);

        if (SelectedPreset.IsValid())
        {
            return;
        }

        if (Option->Source == FFilmPresetOption::ESource::Asset)
        {
            if (SelectedAssetPath.IsValid() && Option->AssetData.IsValid() && Option->AssetData->ToSoftObjectPath() == SelectedAssetPath)
            {
                SelectedPreset = Option;
            }
        }
        else if (Option->Source == FFilmPresetOption::ESource::Json)
        {
            if (SelectedPresetId != NAME_None && Option->PresetId == SelectedPresetId)
            {
                SelectedPreset = Option;
            }
        }
    };

    auto GetFilmTypeLabel = [](EFilmEmulatorFilmType Type)
    {
        if (const UEnum* Enum = StaticEnum<EFilmEmulatorFilmType>())
        {
            return Enum->GetDisplayNameTextByValue(static_cast<int64>(Type));
        }
        return FText::FromString(LexToString(Type));
    };

    auto GetFilmFormatLabel = [](EFilmEmulatorFilmFormat Format)
    {
        if (const UEnum* Enum = StaticEnum<EFilmEmulatorFilmFormat>())
        {
            return Enum->GetDisplayNameTextByValue(static_cast<int64>(Format));
        }
        return FText::FromString(LexToString(Format));
    };

    auto AddGroupedOptions = [&](const TMap<EFilmEmulatorFilmType, TMap<EFilmEmulatorFilmFormat, TArray<TSharedPtr<FFilmPresetOption>>>>& Groups)
    {
        const EFilmEmulatorFilmType FilmTypeOrder[] =
        {
            EFilmEmulatorFilmType::ColorNegative,
            EFilmEmulatorFilmType::ColorSlide,
            EFilmEmulatorFilmType::BWNegative,
            EFilmEmulatorFilmType::BWPositive
        };

        for (EFilmEmulatorFilmType Type : FilmTypeOrder)
        {
            const TMap<EFilmEmulatorFilmFormat, TArray<TSharedPtr<FFilmPresetOption>>>* FormatGroups = Groups.Find(Type);
            if (!FormatGroups || FormatGroups->Num() == 0)
            {
                continue;
            }

            AddSeparator(GetFilmTypeLabel(Type));

            TArray<EFilmEmulatorFilmFormat> Formats;
            FormatGroups->GetKeys(Formats);
            Formats.Sort([](const EFilmEmulatorFilmFormat& A, const EFilmEmulatorFilmFormat& B)
            {
                return static_cast<uint8>(A) < static_cast<uint8>(B);
            });

            for (const EFilmEmulatorFilmFormat Format : Formats)
            {
                const TArray<TSharedPtr<FFilmPresetOption>>* Options = FormatGroups->Find(Format);
                if (!Options || Options->Num() == 0)
                {
                    continue;
                }

                const FText FormatLabel = GetFilmFormatLabel(Format);
                AddSeparator(FText::FromString(FString::Printf(TEXT("  %s"), *FormatLabel.ToString())));

                TArray<TSharedPtr<FFilmPresetOption>> Sorted = *Options;
                Sorted.Sort([this](const TSharedPtr<FFilmPresetOption>& A, const TSharedPtr<FFilmPresetOption>& B)
                {
                    return GetPresetLabelForOption(A).ToString() < GetPresetLabelForOption(B).ToString();
                });

                for (const TSharedPtr<FFilmPresetOption>& Option : Sorted)
                {
                    AddOption(Option);
                }
            }
        }
    };

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.ClassPaths.Add(UFilmStockPreset::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.PackagePaths.Add(FName("/FilmEmulator/Presets"));
    Filter.PackagePaths.Add(FName("/Game/FilmEmulator/Presets"));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetPresets;
    AssetRegistryModule.Get().GetAssets(Filter, AssetPresets);

    if (AssetPresets.Num() > 0)
    {
        TMap<EFilmEmulatorFilmType, TMap<EFilmEmulatorFilmFormat, TArray<TSharedPtr<FFilmPresetOption>>>> GroupedAssets;

        for (const FAssetData& Asset : AssetPresets)
        {
            EFilmEmulatorFilmType FilmType = EFilmEmulatorFilmType::ColorNegative;
            EFilmEmulatorFilmFormat FilmFormat = EFilmEmulatorFilmFormat::Photo35;
            if (UFilmStockPreset* Preset = Cast<UFilmStockPreset>(Asset.GetAsset()))
            {
                FilmType = Preset->FilmType;
                FilmFormat = Preset->FilmFormat;
            }

            TSharedPtr<FFilmPresetOption> Option = MakeShared<FFilmPresetOption>();
            Option->Source = FFilmPresetOption::ESource::Asset;
            Option->AssetData = MakeShared<FAssetData>(Asset);

            GroupedAssets.FindOrAdd(FilmType).FindOrAdd(FilmFormat).Add(Option);
        }

        AddSeparator(LOCTEXT("FilmPresetAssetSeparator", "Preset Assets"));
        AddGroupedOptions(GroupedAssets);
    }

    const TArray<FFilmPresetEntry>& JsonPresets = FFilmEmulatorPresetLibrary::Get().GetPresets();
    if (JsonPresets.Num() > 0)
    {
        TMap<EFilmEmulatorFilmType, TMap<EFilmEmulatorFilmFormat, TArray<TSharedPtr<FFilmPresetOption>>>> GroupedJson;

        for (const FFilmPresetEntry& Entry : JsonPresets)
        {
            EFilmEmulatorFilmType FilmType = EFilmEmulatorFilmType::ColorNegative;
            EFilmEmulatorFilmFormat FilmFormat = EFilmEmulatorFilmFormat::Photo35;
            if (Entry.Preset.IsValid())
            {
                FilmType = Entry.Preset->FilmType;
                FilmFormat = Entry.Preset->FilmFormat;
            }

            TSharedPtr<FFilmPresetOption> Option = MakeShared<FFilmPresetOption>();
            Option->Source = FFilmPresetOption::ESource::Json;
            Option->PresetId = Entry.PresetId;

            GroupedJson.FindOrAdd(FilmType).FindOrAdd(FilmFormat).Add(Option);
        }

        AddSeparator(LOCTEXT("FilmPresetJsonSeparator", "JSON Presets"));
        AddGroupedOptions(GroupedJson);
    }

    if (!SelectedPreset.IsValid())
    {
        for (const TSharedPtr<FFilmPresetOption>& Option : PresetOptions)
        {
            if (Option.IsValid() && !Option->bIsSeparator)
            {
                SelectedPreset = Option;
                break;
            }
        }
    }
}

void SFilmEmulatorWindow::RebuildPrintProfileOptions()
{
    PrintProfileOptions.Reset();
    SelectedPrintProfile.Reset();

    UFilmEmulatorSettings* Settings = GetSettings();
    if (!Settings)
    {
        return;
    }

    for (const FFilmPrintProfile& Profile : Settings->FilmPrintProfiles)
    {
        PrintProfileOptions.Add(MakeShared<FName>(Profile.ProfileId));
    }

    FName DesiredId = Settings->DefaultParams.FilmPrintProfileId;
    if (DesiredId.IsNone())
    {
        DesiredId = Settings->DefaultFilmPrintProfileId;
    }

    for (const TSharedPtr<FName>& Option : PrintProfileOptions)
    {
        if (Option.IsValid() && *Option == DesiredId)
        {
            SelectedPrintProfile = Option;
            break;
        }
    }

    if (!SelectedPrintProfile.IsValid() && PrintProfileOptions.Num() > 0)
    {
        SelectedPrintProfile = PrintProfileOptions[0];
    }
}

UFilmStockPreset* SFilmEmulatorWindow::GetSelectedPresetForDisplay() const
{
    if (SelectedPreset.IsValid() && !SelectedPreset->bIsSeparator)
    {
        if (SelectedPreset->Source == FFilmPresetOption::ESource::Asset)
        {
            if (SelectedPreset->AssetData.IsValid())
            {
                return Cast<UFilmStockPreset>(SelectedPreset->AssetData->GetAsset());
            }
        }
        else if (SelectedPreset->Source == FFilmPresetOption::ESource::Json)
        {
            return FFilmEmulatorPresetLibrary::Get().FindPresetById(SelectedPreset->PresetId);
        }
    }

    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        if (Settings->DefaultPreset.IsValid())
        {
            return Settings->DefaultPreset.Get();
        }
        if (Settings->DefaultPreset.ToSoftObjectPath().IsValid())
        {
            return Settings->DefaultPreset.LoadSynchronous();
        }
        if (Settings->DefaultPresetId != NAME_None)
        {
            return FFilmEmulatorPresetLibrary::Get().FindPresetById(Settings->DefaultPresetId);
        }
    }

    return nullptr;
}

UFilmStockPreset* SFilmEmulatorWindow::GetEditablePreset() const
{
    if (!SelectedPreset.IsValid() || SelectedPreset->bIsSeparator)
    {
        return nullptr;
    }

    if (SelectedPreset->Source == FFilmPresetOption::ESource::Asset)
    {
        if (SelectedPreset->AssetData.IsValid())
        {
            return Cast<UFilmStockPreset>(SelectedPreset->AssetData->GetAsset());
        }
    }
    else if (SelectedPreset->Source == FFilmPresetOption::ESource::Json)
    {
        return FFilmEmulatorPresetLibrary::Get().FindPresetById(SelectedPreset->PresetId);
    }

    return nullptr;
}

bool SFilmEmulatorWindow::IsPresetEditable() const
{
    return GetEditablePreset() != nullptr;
}

bool SFilmEmulatorWindow::IsPresetControlsEnabled() const
{
    return GetSelectedPresetForDisplay() != nullptr;
}

UFilmStockPreset* SFilmEmulatorWindow::GetOrCreateEditablePreset()
{
    return GetEditablePreset();
}

FText SFilmEmulatorWindow::GetPresetLabelForOption(TSharedPtr<FFilmPresetOption> Option) const
{
    if (!Option.IsValid())
    {
        return FText::GetEmpty();
    }

    if (Option->bIsSeparator)
    {
        return Option->SeparatorLabel;
    }

    if (Option->Source == FFilmPresetOption::ESource::Asset)
    {
        if (Option->AssetData.IsValid())
        {
            if (UFilmStockPreset* Preset = Cast<UFilmStockPreset>(Option->AssetData->GetAsset()))
            {
                if (!Preset->DisplayName.IsEmpty())
                {
                    return Preset->DisplayName;
                }
            }
            return FText::FromName(Option->AssetData->AssetName);
        }
    }
    else if (Option->Source == FFilmPresetOption::ESource::Json)
    {
        if (UFilmStockPreset* Preset = FFilmEmulatorPresetLibrary::Get().FindPresetById(Option->PresetId))
        {
            if (!Preset->DisplayName.IsEmpty())
            {
                return Preset->DisplayName;
            }
        }
        return FText::FromName(Option->PresetId);
    }

    return FText::GetEmpty();
}

FText SFilmEmulatorWindow::GetSelectedPresetText() const
{
    return GetPresetLabelForOption(SelectedPreset);
}

FText SFilmEmulatorWindow::GetSelectedPresetSourceText() const
{
    if (!SelectedPreset.IsValid() || SelectedPreset->bIsSeparator)
    {
        return FText::GetEmpty();
    }

    if (SelectedPreset->Source == FFilmPresetOption::ESource::Asset)
    {
        if (SelectedPreset->AssetData.IsValid())
        {
            return FText::FromString(SelectedPreset->AssetData->PackagePath.ToString());
        }
        return LOCTEXT("FilmPresetSourceAsset", "Asset");
    }

    const TArray<FFilmPresetEntry>& Presets = FFilmEmulatorPresetLibrary::Get().GetPresets();
    for (const FFilmPresetEntry& Entry : Presets)
    {
        if (Entry.PresetId == SelectedPreset->PresetId)
        {
            return FText::FromString(FPaths::GetCleanFilename(Entry.SourcePath));
        }
    }

    return LOCTEXT("FilmPresetSourceJson", "JSON");
}

TSharedRef<SWidget> SFilmEmulatorWindow::MakePresetWidget(TSharedPtr<FFilmPresetOption> InItem) const
{
    if (InItem.IsValid() && InItem->bIsSeparator)
    {
        return SNew(STextBlock)
            .Text(InItem->SeparatorLabel)
            .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Bold"))
            .ColorAndOpacity(FLinearColor(0.98f, 0.86f, 0.2f, 1.0f));
    }

    return SNew(STextBlock)
        .Text(GetPresetLabelForOption(InItem))
        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"));
}

void SFilmEmulatorWindow::OnPresetChanged(TSharedPtr<FFilmPresetOption> NewSelection, ESelectInfo::Type SelectInfo)
{
    (void)SelectInfo;
    if (!NewSelection.IsValid() || NewSelection->bIsSeparator)
    {
        return;
    }

    SelectedPreset = NewSelection;

    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        if (SelectedPreset->Source == FFilmPresetOption::ESource::Asset)
        {
            if (SelectedPreset->AssetData.IsValid())
            {
                if (UFilmStockPreset* PresetAsset = Cast<UFilmStockPreset>(SelectedPreset->AssetData->GetAsset()))
                {
                    Settings->DefaultPreset = PresetAsset;
                    Settings->DefaultPresetId = NAME_None;
                    Settings->SaveSettingsConfig();
                }
            }
        }
        else if (SelectedPreset->Source == FFilmPresetOption::ESource::Json)
        {
            Settings->DefaultPresetId = SelectedPreset->PresetId;
            Settings->DefaultPreset.Reset();
            Settings->SaveSettingsConfig();
        }
    }

    if (TSharedPtr<IDetailsView> Details = GFilmEmulatorPresetDetails.Pin())
    {
        Details->SetObject(GetSelectedPresetForDisplay());
    }
}

const FFilmPrintProfile* SFilmEmulatorWindow::GetSelectedPrintProfile() const
{
    UFilmEmulatorSettings* Settings = GetSettings();
    if (!Settings)
    {
        return nullptr;
    }

    if (SelectedPrintProfile.IsValid())
    {
        return Settings->FindFilmPrintProfile(*SelectedPrintProfile);
    }

    if (Settings->DefaultParams.FilmPrintProfileId != NAME_None)
    {
        if (const FFilmPrintProfile* Profile = Settings->FindFilmPrintProfile(Settings->DefaultParams.FilmPrintProfileId))
        {
            return Profile;
        }
    }

    if (!Settings->DefaultFilmPrintProfileId.IsNone())
    {
        if (const FFilmPrintProfile* Profile = Settings->FindFilmPrintProfile(Settings->DefaultFilmPrintProfileId))
        {
            return Profile;
        }
    }

    return nullptr;
}

FText SFilmEmulatorWindow::GetPrintProfileLabelForOption(TSharedPtr<FName> Option) const
{
    UFilmEmulatorSettings* Settings = GetSettings();
    if (!Settings || !Option.IsValid())
    {
        return FText::GetEmpty();
    }

    if (const FFilmPrintProfile* Profile = Settings->FindFilmPrintProfile(*Option))
    {
        if (!Profile->DisplayName.IsEmpty())
        {
            return Profile->DisplayName;
        }
        return FText::FromName(Profile->ProfileId);
    }

    return FText::FromName(*Option);
}

FText SFilmEmulatorWindow::GetSelectedPrintProfileText() const
{
    if (SelectedPrintProfile.IsValid())
    {
        return GetPrintProfileLabelForOption(SelectedPrintProfile);
    }

    return LOCTEXT("FilmPrintProfileNone", "None");
}

FText SFilmEmulatorWindow::GetSelectedPrintProfileLUTText() const
{
    if (const FFilmPrintProfile* Profile = GetSelectedPrintProfile())
    {
        if (Profile->PrintLUTAsset.ToSoftObjectPath().IsValid())
        {
            return FText::FromString(Profile->PrintLUTAsset.ToSoftObjectPath().GetAssetName());
        }
        if (Profile->PrintLUT.ToSoftObjectPath().IsValid())
        {
            return FText::FromString(Profile->PrintLUT.ToSoftObjectPath().GetAssetName());
        }
        if (!Profile->PrintLUTPath.FilePath.IsEmpty())
        {
            return FText::FromString(Profile->PrintLUTPath.FilePath);
        }
    }

    return FText::GetEmpty();
}

FText SFilmEmulatorWindow::GetSelectedPrintProfileDescription() const
{
    if (const FFilmPrintProfile* Profile = GetSelectedPrintProfile())
    {
        return Profile->Description;
    }

    return FText::GetEmpty();
}

TSharedRef<SWidget> SFilmEmulatorWindow::MakePrintProfileWidget(TSharedPtr<FName> InItem) const
{
    return SNew(STextBlock)
        .Text(GetPrintProfileLabelForOption(InItem))
        .Font(FFilmEmulatorStyle::Get().GetFontStyle("FilmEmulator.Font.Regular"));
}

void SFilmEmulatorWindow::OnPrintProfileChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectInfo)
{
    (void)SelectInfo;
    if (!NewSelection.IsValid())
    {
        return;
    }

    SelectedPrintProfile = NewSelection;

    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.FilmPrintProfileId = *NewSelection;
        Settings->SaveSettingsConfig();
    }
}
ECheckBoxState SFilmEmulatorWindow::GetEnableState() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return ToCheckBoxState(Settings->DefaultParams.bEnableFilmEmulation);
    }
    return ECheckBoxState::Unchecked;
}

void SFilmEmulatorWindow::OnEnableChanged(ECheckBoxState NewState)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.bEnableFilmEmulation = IsChecked(NewState);
        Settings->SaveSettingsConfig();
    }
}

ECheckBoxState SFilmEmulatorWindow::GetFilmPrintEnabledState() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return ToCheckBoxState(Settings->DefaultParams.bEnableFilmPrint);
    }
    return ECheckBoxState::Unchecked;
}

void SFilmEmulatorWindow::OnFilmPrintEnabledChanged(ECheckBoxState NewState)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.bEnableFilmPrint = IsChecked(NewState);
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetStrength() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return Settings->DefaultParams.Strength;
    }
    return 1.0f;
}

void SFilmEmulatorWindow::OnStrengthChanged(float NewValue)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.Strength = NewValue;
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetExposureEV() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return Settings->DefaultParams.ExposureEV;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnExposureEVChanged(float NewValue)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.ExposureEV = NewValue;
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetSaturation() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return Settings->DefaultParams.Saturation;
    }
    return 1.0f;
}

void SFilmEmulatorWindow::OnSaturationChanged(float NewValue)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.Saturation = NewValue;
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetContrast() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return Settings->DefaultParams.Contrast;
    }
    return 1.0f;
}

void SFilmEmulatorWindow::OnContrastChanged(float NewValue)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.Contrast = NewValue;
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetPrintStrength() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return Settings->DefaultParams.PrintStrength;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnPrintStrengthChanged(float NewValue)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.PrintStrength = NewValue;
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetPrintExposureEV() const
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        return Settings->DefaultParams.PrintExposureEV;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnPrintExposureEVChanged(float NewValue)
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->DefaultParams.PrintExposureEV = NewValue;
        Settings->SaveSettingsConfig();
    }
}

float SFilmEmulatorWindow::GetPresetSaturationBias() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return Preset->SaturationBias;
    }
    return 1.0f;
}

void SFilmEmulatorWindow::OnPresetSaturationBiasChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->SaturationBias = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetPresetContrastBias() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return Preset->ContrastBias;
    }
    return 1.0f;
}

void SFilmEmulatorWindow::OnPresetContrastBiasChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->ContrastBias = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetPresetExposureBias() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return Preset->ExposureBias;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnPresetExposureBiasChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->ExposureBias = NewValue;
        Preset->MarkPackageDirty();
    }
}

FText SFilmEmulatorWindow::GetSelectedPresetTypeText() const
{
    if (UFilmStockPreset* Preset = GetSelectedPresetForDisplay())
    {
        return FText::FromString(LexToString(Preset->FilmType));
    }
    return FText::GetEmpty();
}

FText SFilmEmulatorWindow::GetSelectedPresetFormatText() const
{
    if (UFilmStockPreset* Preset = GetSelectedPresetForDisplay())
    {
        return FText::FromString(LexToString(Preset->FilmFormat));
    }
    return FText::GetEmpty();
}

FText SFilmEmulatorWindow::GetSelectedPresetLUTText() const
{
    if (UFilmStockPreset* Preset = GetSelectedPresetForDisplay())
    {
        if (Preset->FilmLUTAsset.ToSoftObjectPath().IsValid())
        {
            return FText::FromString(Preset->FilmLUTAsset.ToSoftObjectPath().GetAssetName());
        }
        if (Preset->FilmLUT.ToSoftObjectPath().IsValid())
        {
            return FText::FromString(Preset->FilmLUT.ToSoftObjectPath().GetAssetName());
        }
        if (!Preset->FilmLUTPath.FilePath.IsEmpty())
        {
            return FText::FromString(Preset->FilmLUTPath.FilePath);
        }
    }

    return FText::GetEmpty();
}

float SFilmEmulatorWindow::GetFormatScale() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return Preset->FilmFormatScale;
    }
    return 1.0f;
}

void SFilmEmulatorWindow::OnFormatScaleChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->FilmFormatScale = NewValue;
        Preset->MarkPackageDirty();
    }
}
#define FE_PRESET_FLOAT_GET(FuncName, Expr, DefaultVal) \
float SFilmEmulatorWindow::FuncName() const \
{ \
    if (UFilmStockPreset* Preset = GetEditablePreset()) \
    { \
        return (Preset->Expr); \
    } \
    return DefaultVal; \
}

#define FE_PRESET_FLOAT_SET(FuncName, Expr) \
void SFilmEmulatorWindow::FuncName(float NewValue) \
{ \
    if (UFilmStockPreset* Preset = GetEditablePreset()) \
    { \
        Preset->Modify(); \
        Preset->Expr = NewValue; \
        Preset->MarkPackageDirty(); \
    } \
}

#define FE_PRESET_BOOL_GET(FuncName, Expr) \
ECheckBoxState SFilmEmulatorWindow::FuncName() const \
{ \
    if (UFilmStockPreset* Preset = GetEditablePreset()) \
    { \
        return ToCheckBoxState(Preset->Expr); \
    } \
    return ECheckBoxState::Unchecked; \
}

#define FE_PRESET_BOOL_SET(FuncName, Expr) \
void SFilmEmulatorWindow::FuncName(ECheckBoxState NewState) \
{ \
    if (UFilmStockPreset* Preset = GetEditablePreset()) \
    { \
        Preset->Modify(); \
        Preset->Expr = IsChecked(NewState); \
        Preset->MarkPackageDirty(); \
    } \
}

FE_PRESET_FLOAT_GET(GetGrainISO, Grain.ISO, 100.0f)
FE_PRESET_FLOAT_SET(OnGrainISOChanged, Grain.ISO)

float SFilmEmulatorWindow::GetGrainIsoMin() const
{
    return 16.0f;
}

float SFilmEmulatorWindow::GetGrainIsoMax() const
{
    return 3200.0f;
}

FE_PRESET_BOOL_GET(GetGrainEnabledState, Grain.bEnabled)
FE_PRESET_BOOL_SET(OnGrainEnabledChanged, Grain.bEnabled)
FE_PRESET_FLOAT_GET(GetGrainIntensity, Grain.Intensity, 0.0f)
FE_PRESET_FLOAT_SET(OnGrainIntensityChanged, Grain.Intensity)
FE_PRESET_FLOAT_GET(GetGrainSize, Grain.Size, 6.0f)
FE_PRESET_FLOAT_SET(OnGrainSizeChanged, Grain.Size)
FE_PRESET_FLOAT_GET(GetGrainSigmaR, Grain.SigmaR, 0.3f)
FE_PRESET_FLOAT_SET(OnGrainSigmaRChanged, Grain.SigmaR)
FE_PRESET_FLOAT_GET(GetGrainFilterSigma, Grain.FilterSigma, 0.8f)
FE_PRESET_FLOAT_SET(OnGrainFilterSigmaChanged, Grain.FilterSigma)
FE_PRESET_FLOAT_GET(GetGrainChromatic, Grain.Chromatic, 0.0f)
FE_PRESET_FLOAT_SET(OnGrainChromaticChanged, Grain.Chromatic)
FE_PRESET_FLOAT_GET(GetGrainResponse, Grain.Response, 1.0f)
FE_PRESET_FLOAT_SET(OnGrainResponseChanged, Grain.Response)
FE_PRESET_FLOAT_GET(GetGrainAnimationAmplitude, Grain.AnimationAmplitude, 1.0f)
FE_PRESET_FLOAT_SET(OnGrainAnimationAmplitudeChanged, Grain.AnimationAmplitude)

FE_PRESET_BOOL_GET(GetGateWeaveEnabledState, GateWeave.bEnabled)
FE_PRESET_BOOL_SET(OnGateWeaveEnabledChanged, GateWeave.bEnabled)
FE_PRESET_FLOAT_GET(GetGateWeaveAmplitude, GateWeave.Amplitude, 0.02f)
FE_PRESET_FLOAT_SET(OnGateWeaveAmplitudeChanged, GateWeave.Amplitude)
FE_PRESET_FLOAT_GET(GetGateWeaveFrequency, GateWeave.Frequency, 0.6f)
FE_PRESET_FLOAT_SET(OnGateWeaveFrequencyChanged, GateWeave.Frequency)

FE_PRESET_BOOL_GET(GetFlickerEnabledState, Flicker.bEnabled)
FE_PRESET_BOOL_SET(OnFlickerEnabledChanged, Flicker.bEnabled)
FE_PRESET_FLOAT_GET(GetFlickerIntensity, Flicker.Intensity, 0.02f)
FE_PRESET_FLOAT_SET(OnFlickerIntensityChanged, Flicker.Intensity)
FE_PRESET_FLOAT_GET(GetFlickerFrequency, Flicker.Frequency, 0.3f)
FE_PRESET_FLOAT_SET(OnFlickerFrequencyChanged, Flicker.Frequency)

FE_PRESET_BOOL_GET(GetGateScratchEnabledState, GateScratch.bEnabled)
FE_PRESET_BOOL_SET(OnGateScratchEnabledChanged, GateScratch.bEnabled)
FE_PRESET_FLOAT_GET(GetGateScratchIntensity, GateScratch.Intensity, 0.0f)
FE_PRESET_FLOAT_SET(OnGateScratchIntensityChanged, GateScratch.Intensity)
FE_PRESET_FLOAT_GET(GetGateScratchDensity, GateScratch.Density, 0.15f)
FE_PRESET_FLOAT_SET(OnGateScratchDensityChanged, GateScratch.Density)
FE_PRESET_FLOAT_GET(GetGateScratchWidth, GateScratch.Width, 3.0f)
FE_PRESET_FLOAT_SET(OnGateScratchWidthChanged, GateScratch.Width)
FE_PRESET_FLOAT_GET(GetGateScratchWidthJitter, GateScratch.WidthJitter, 0.3f)
FE_PRESET_FLOAT_SET(OnGateScratchWidthJitterChanged, GateScratch.WidthJitter)
FE_PRESET_FLOAT_GET(GetGateScratchLength, GateScratch.Length, 1.0f)
FE_PRESET_FLOAT_SET(OnGateScratchLengthChanged, GateScratch.Length)
FE_PRESET_FLOAT_GET(GetGateScratchLengthJitter, GateScratch.LengthJitter, 0.5f)
FE_PRESET_FLOAT_SET(OnGateScratchLengthJitterChanged, GateScratch.LengthJitter)
FE_PRESET_FLOAT_GET(GetGateScratchOpacityJitter, GateScratch.OpacityJitter, 0.5f)
FE_PRESET_FLOAT_SET(OnGateScratchOpacityJitterChanged, GateScratch.OpacityJitter)
FE_PRESET_FLOAT_GET(GetGateScratchFrequency, GateScratch.Frequency, 0.2f)
FE_PRESET_FLOAT_SET(OnGateScratchFrequencyChanged, GateScratch.Frequency)
FE_PRESET_BOOL_GET(GetGateScratchAutoPolarityState, GateScratch.bAutoPolarity)
FE_PRESET_BOOL_SET(OnGateScratchAutoPolarityChanged, GateScratch.bAutoPolarity)

bool SFilmEmulatorWindow::IsGateScratchPolarityEditable() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return !Preset->GateScratch.bAutoPolarity;
    }
    return false;
}

FE_PRESET_FLOAT_GET(GetGateScratchPolarity, GateScratch.Polarity, 1.0f)
FE_PRESET_FLOAT_SET(OnGateScratchPolarityChanged, GateScratch.Polarity)
FE_PRESET_FLOAT_GET(GetGateScratchTintR, GateScratch.Tint.R, 1.0f)
FE_PRESET_FLOAT_SET(OnGateScratchTintRChanged, GateScratch.Tint.R)
FE_PRESET_FLOAT_GET(GetGateScratchTintG, GateScratch.Tint.G, 1.0f)
FE_PRESET_FLOAT_SET(OnGateScratchTintGChanged, GateScratch.Tint.G)
FE_PRESET_FLOAT_GET(GetGateScratchTintB, GateScratch.Tint.B, 1.0f)
FE_PRESET_FLOAT_SET(OnGateScratchTintBChanged, GateScratch.Tint.B)

FE_PRESET_BOOL_GET(GetDirtEnabledState, Dirt.bEnabled)
FE_PRESET_BOOL_SET(OnDirtEnabledChanged, Dirt.bEnabled)
FE_PRESET_FLOAT_GET(GetDirtIntensity, Dirt.Intensity, 0.0f)
FE_PRESET_FLOAT_SET(OnDirtIntensityChanged, Dirt.Intensity)
FE_PRESET_FLOAT_GET(GetDirtDensity, Dirt.Density, 0.2f)
FE_PRESET_FLOAT_SET(OnDirtDensityChanged, Dirt.Density)
FE_PRESET_FLOAT_GET(GetDirtSize, Dirt.Size, 25.0f)
FE_PRESET_FLOAT_SET(OnDirtSizeChanged, Dirt.Size)
FE_PRESET_FLOAT_GET(GetDirtSizeJitter, Dirt.SizeJitter, 0.6f)
FE_PRESET_FLOAT_SET(OnDirtSizeJitterChanged, Dirt.SizeJitter)
FE_PRESET_FLOAT_GET(GetDirtOpacityJitter, Dirt.OpacityJitter, 0.6f)
FE_PRESET_FLOAT_SET(OnDirtOpacityJitterChanged, Dirt.OpacityJitter)
FE_PRESET_FLOAT_GET(GetDirtSoftness, Dirt.Softness, 0.6f)
FE_PRESET_FLOAT_SET(OnDirtSoftnessChanged, Dirt.Softness)
FE_PRESET_FLOAT_GET(GetDirtFrequency, Dirt.Frequency, 0.2f)
FE_PRESET_FLOAT_SET(OnDirtFrequencyChanged, Dirt.Frequency)
FE_PRESET_BOOL_GET(GetDirtAutoPolarityState, Dirt.bAutoPolarity)
FE_PRESET_BOOL_SET(OnDirtAutoPolarityChanged, Dirt.bAutoPolarity)

bool SFilmEmulatorWindow::IsDirtPolarityEditable() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return !Preset->Dirt.bAutoPolarity;
    }
    return false;
}

FE_PRESET_FLOAT_GET(GetDirtPolarity, Dirt.Polarity, 1.0f)
FE_PRESET_FLOAT_SET(OnDirtPolarityChanged, Dirt.Polarity)
FE_PRESET_FLOAT_GET(GetDirtTintR, Dirt.Tint.R, 1.0f)
FE_PRESET_FLOAT_SET(OnDirtTintRChanged, Dirt.Tint.R)
FE_PRESET_FLOAT_GET(GetDirtTintG, Dirt.Tint.G, 1.0f)
FE_PRESET_FLOAT_SET(OnDirtTintGChanged, Dirt.Tint.G)
FE_PRESET_FLOAT_GET(GetDirtTintB, Dirt.Tint.B, 1.0f)
FE_PRESET_FLOAT_SET(OnDirtTintBChanged, Dirt.Tint.B)
FE_PRESET_BOOL_GET(GetDirtUseTextureState, Dirt.bUseTexture)
FE_PRESET_BOOL_SET(OnDirtUseTextureChanged, Dirt.bUseTexture)
FE_PRESET_BOOL_GET(GetDirtInvertTextureState, Dirt.bInvertTexture)
FE_PRESET_BOOL_SET(OnDirtInvertTextureChanged, Dirt.bInvertTexture)

bool SFilmEmulatorWindow::IsDirtTextureEnabled() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        return Preset->Dirt.bUseTexture;
    }
    return false;
}

FString SFilmEmulatorWindow::GetDirtTexturePath() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        if (Preset->Dirt.DamageTexture.ToSoftObjectPath().IsValid())
        {
            return Preset->Dirt.DamageTexture.ToSoftObjectPath().ToString();
        }
    }
    return FString();
}

void SFilmEmulatorWindow::OnDirtTextureChanged(const FAssetData& AssetData)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Dirt.DamageTexture = TSoftObjectPtr<UTexture2D>(AssetData.ToSoftObjectPath());
        Preset->MarkPackageDirty();
    }
}

FE_PRESET_FLOAT_GET(GetDirtTextureTiling, Dirt.TextureTiling, 1.0f)
FE_PRESET_FLOAT_SET(OnDirtTextureTilingChanged, Dirt.TextureTiling)
FE_PRESET_FLOAT_GET(GetDirtTextureScaleMin, Dirt.TextureScaleMin, 0.5f)
FE_PRESET_FLOAT_SET(OnDirtTextureScaleMinChanged, Dirt.TextureScaleMin)
FE_PRESET_FLOAT_GET(GetDirtTextureScaleMax, Dirt.TextureScaleMax, 2.0f)
FE_PRESET_FLOAT_SET(OnDirtTextureScaleMaxChanged, Dirt.TextureScaleMax)
FE_PRESET_FLOAT_GET(GetDirtNoiseScale, Dirt.NoiseScale, 2.0f)
FE_PRESET_FLOAT_SET(OnDirtNoiseScaleChanged, Dirt.NoiseScale)
FE_PRESET_FLOAT_GET(GetDirtNoiseStrength, Dirt.NoiseStrength, 0.35f)
FE_PRESET_FLOAT_SET(OnDirtNoiseStrengthChanged, Dirt.NoiseStrength)
FE_PRESET_FLOAT_GET(GetDirtNoiseSpeed, Dirt.NoiseSpeed, 0.25f)
FE_PRESET_FLOAT_SET(OnDirtNoiseSpeedChanged, Dirt.NoiseSpeed)

FE_PRESET_BOOL_GET(GetHalationEnabledState, Halation.bEnabled)
FE_PRESET_BOOL_SET(OnHalationEnabledChanged, Halation.bEnabled)
FE_PRESET_FLOAT_GET(GetHalationIntensity, Halation.Intensity, 0.0f)
FE_PRESET_FLOAT_SET(OnHalationIntensityChanged, Halation.Intensity)
FE_PRESET_FLOAT_GET(GetHalationRadius, Halation.Radius, 1.0f)
FE_PRESET_FLOAT_SET(OnHalationRadiusChanged, Halation.Radius)
FE_PRESET_FLOAT_GET(GetHalationThreshold, Halation.Threshold, 1.0f)
FE_PRESET_FLOAT_SET(OnHalationThresholdChanged, Halation.Threshold)
FE_PRESET_FLOAT_GET(GetHalationTintR, Halation.Tint.R, 1.0f)
FE_PRESET_FLOAT_SET(OnHalationTintRChanged, Halation.Tint.R)
FE_PRESET_FLOAT_GET(GetHalationTintG, Halation.Tint.G, 0.4f)
FE_PRESET_FLOAT_SET(OnHalationTintGChanged, Halation.Tint.G)
FE_PRESET_FLOAT_GET(GetHalationTintB, Halation.Tint.B, 0.2f)
FE_PRESET_FLOAT_SET(OnHalationTintBChanged, Halation.Tint.B)
FE_PRESET_FLOAT_GET(GetHalationRadiusMidScale, Halation.RadiusMidScale, 2.0f)
FE_PRESET_FLOAT_SET(OnHalationRadiusMidScaleChanged, Halation.RadiusMidScale)
FE_PRESET_FLOAT_GET(GetHalationRadiusFarScale, Halation.RadiusFarScale, 4.0f)
FE_PRESET_FLOAT_SET(OnHalationRadiusFarScaleChanged, Halation.RadiusFarScale)

float SFilmEmulatorWindow::GetHalationTintNearR() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Near.R;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintNearRChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintNear.R = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintNearG() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Near.G;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintNearGChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintNear.G = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintNearB() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Near.B;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintNearBChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintNear.B = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintMidR() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Mid.R;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintMidRChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintMid.R = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintMidG() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Mid.G;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintMidGChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintMid.G = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintMidB() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Mid.B;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintMidBChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintMid.B = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintFarR() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Far.R;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintFarRChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintFar.R = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintFarG() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Far.G;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintFarGChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintFar.G = NewValue;
        Preset->MarkPackageDirty();
    }
}

float SFilmEmulatorWindow::GetHalationTintFarB() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        FLinearColor Near;
        FLinearColor Mid;
        FLinearColor Far;
        ResolveHalationTints(Preset->Halation, Near, Mid, Far);
        return Far.B;
    }
    return 0.0f;
}

void SFilmEmulatorWindow::OnHalationTintFarBChanged(float NewValue)
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        Preset->Modify();
        Preset->Halation.TintFar.B = NewValue;
        Preset->MarkPackageDirty();
    }
}
FReply SFilmEmulatorWindow::OnSaveConfigClicked()
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->SaveSettingsConfig();
    }
    return FReply::Handled();
}

FReply SFilmEmulatorWindow::OnReloadConfigClicked()
{
    if (UFilmEmulatorSettings* Settings = GetSettings())
    {
        Settings->ReloadSettingsConfig();
    }

    RebuildPresetOptions();
    RebuildPrintProfileOptions();

    if (PresetComboBox.IsValid())
    {
        PresetComboBox->RefreshOptions();
        PresetComboBox->SetSelectedItem(SelectedPreset);
    }

    if (PrintProfileComboBox.IsValid())
    {
        PrintProfileComboBox->RefreshOptions();
        PrintProfileComboBox->SetSelectedItem(SelectedPrintProfile);
    }

    if (TSharedPtr<IDetailsView> Details = GFilmEmulatorSettingsDetails.Pin())
    {
        Details->SetObject(GetSettings());
    }

    if (TSharedPtr<IDetailsView> Details = GFilmEmulatorPresetDetails.Pin())
    {
        Details->SetObject(GetSelectedPresetForDisplay());
    }

    return FReply::Handled();
}

FReply SFilmEmulatorWindow::OnImportPresetsClicked()
{
    AutoImportPresetAssets();
    RebuildPresetOptions();

    if (PresetComboBox.IsValid())
    {
        PresetComboBox->RefreshOptions();
        PresetComboBox->SetSelectedItem(SelectedPreset);
    }

    if (TSharedPtr<IDetailsView> Details = GFilmEmulatorPresetDetails.Pin())
    {
        Details->SetObject(GetSelectedPresetForDisplay());
    }

    return FReply::Handled();
}

FReply SFilmEmulatorWindow::OnCheckPresetsClicked()
{
    AutoImportPresetAssets();
    RebuildPresetOptions();

    if (PresetComboBox.IsValid())
    {
        PresetComboBox->RefreshOptions();
        PresetComboBox->SetSelectedItem(SelectedPreset);
    }

    if (TSharedPtr<IDetailsView> Details = GFilmEmulatorPresetDetails.Pin())
    {
        Details->SetObject(GetSelectedPresetForDisplay());
    }

    return FReply::Handled();
}

FReply SFilmEmulatorWindow::OnCheckLutsClicked()
{
    AutoImportLUTAssets();
    return FReply::Handled();
}

FReply SFilmEmulatorWindow::OnResetPresetFromJsonClicked()
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        if (!Preset->PresetId.IsNone())
        {
            if (UFilmStockPreset* JsonPreset = FFilmEmulatorPresetLibrary::Get().FindPresetById(Preset->PresetId))
            {
                Preset->Modify();
                CopyPresetData(JsonPreset, Preset);
                Preset->PresetId = JsonPreset->PresetId;
                Preset->MarkPackageDirty();

                if (TSharedPtr<IDetailsView> Details = GFilmEmulatorPresetDetails.Pin())
                {
                    Details->SetObject(Preset, true);
                }
            }
        }
    }

    return FReply::Handled();
}

bool SFilmEmulatorWindow::CanResetPresetFromJson() const
{
    if (UFilmStockPreset* Preset = GetEditablePreset())
    {
        if (!Preset->PresetId.IsNone())
        {
            return FFilmEmulatorPresetLibrary::Get().FindPresetById(Preset->PresetId) != nullptr;
        }
    }

    return false;
}

UFilmEmulatorSettings* SFilmEmulatorWindow::GetSettings() const
{
    return GetMutableDefault<UFilmEmulatorSettings>();
}

#undef FE_PRESET_FLOAT_GET
#undef FE_PRESET_FLOAT_SET
#undef FE_PRESET_BOOL_GET
#undef FE_PRESET_BOOL_SET

#undef LOCTEXT_NAMESPACE










