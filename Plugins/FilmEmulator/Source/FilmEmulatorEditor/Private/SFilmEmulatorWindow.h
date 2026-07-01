// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SCompoundWidget.h"

class UFilmStockPreset;
enum class ECheckBoxState : uint8;

class UFilmEmulatorSettings;
struct FFilmPrintProfile;

struct FFilmPresetOption
{
    enum class ESource : uint8
    {
        Asset,
        Json
    };

    ESource Source = ESource::Asset;
    TSharedPtr<FAssetData> AssetData;
    FName PresetId = NAME_None;
    bool bIsSeparator = false;
    FText SeparatorLabel;
};

class SFilmEmulatorWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFilmEmulatorWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    void RebuildPresetOptions();
    void RebuildPrintProfileOptions();
    UFilmStockPreset* GetSelectedPresetForDisplay() const;
    UFilmStockPreset* GetEditablePreset() const;
    bool IsPresetEditable() const;
    bool IsPresetControlsEnabled() const;
    UFilmStockPreset* GetOrCreateEditablePreset();
    FText GetPresetLabelForOption(TSharedPtr<FFilmPresetOption> Option) const;
    FText GetSelectedPresetText() const;
    FText GetSelectedPresetSourceText() const;
    TSharedRef<SWidget> MakePresetWidget(TSharedPtr<FFilmPresetOption> InItem) const;
    void OnPresetChanged(TSharedPtr<FFilmPresetOption> NewSelection, ESelectInfo::Type SelectInfo);

    const FFilmPrintProfile* GetSelectedPrintProfile() const;
    FText GetPrintProfileLabelForOption(TSharedPtr<FName> Option) const;
    FText GetSelectedPrintProfileText() const;
    FText GetSelectedPrintProfileLUTText() const;
    FText GetSelectedPrintProfileDescription() const;
    TSharedRef<SWidget> MakePrintProfileWidget(TSharedPtr<FName> InItem) const;
    void OnPrintProfileChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectInfo);

    TSharedRef<SWidget> BuildFilmStockTab();
    TSharedRef<SWidget> BuildFilmPrintTab();

    ECheckBoxState GetEnableState() const;
    void OnEnableChanged(ECheckBoxState NewState);

    ECheckBoxState GetFilmPrintEnabledState() const;
    void OnFilmPrintEnabledChanged(ECheckBoxState NewState);

    float GetStrength() const;
    void OnStrengthChanged(float NewValue);

    float GetExposureEV() const;
    void OnExposureEVChanged(float NewValue);

    float GetSaturation() const;
    void OnSaturationChanged(float NewValue);

    float GetContrast() const;
    void OnContrastChanged(float NewValue);

    float GetPrintStrength() const;
    void OnPrintStrengthChanged(float NewValue);

    float GetPrintExposureEV() const;
    void OnPrintExposureEVChanged(float NewValue);

    float GetPresetSaturationBias() const;
    void OnPresetSaturationBiasChanged(float NewValue);

    float GetPresetContrastBias() const;
    void OnPresetContrastBiasChanged(float NewValue);

    float GetPresetExposureBias() const;
    void OnPresetExposureBiasChanged(float NewValue);

    FText GetSelectedPresetTypeText() const;
    FText GetSelectedPresetFormatText() const;
    FText GetSelectedPresetLUTText() const;

    float GetFormatScale() const;
    void OnFormatScaleChanged(float NewValue);

    float GetGrainISO() const;
    void OnGrainISOChanged(float NewValue);
    float GetGrainIsoMin() const;
    float GetGrainIsoMax() const;

    ECheckBoxState GetGrainEnabledState() const;
    void OnGrainEnabledChanged(ECheckBoxState NewState);

    float GetGrainIntensity() const;
    void OnGrainIntensityChanged(float NewValue);

    float GetGrainSize() const;
    void OnGrainSizeChanged(float NewValue);

    float GetGrainSigmaR() const;
    void OnGrainSigmaRChanged(float NewValue);

    float GetGrainFilterSigma() const;
    void OnGrainFilterSigmaChanged(float NewValue);

    float GetGrainChromatic() const;
    void OnGrainChromaticChanged(float NewValue);

    float GetGrainResponse() const;
    void OnGrainResponseChanged(float NewValue);
    float GetGrainAnimationAmplitude() const;
    void OnGrainAnimationAmplitudeChanged(float NewValue);

    ECheckBoxState GetGateWeaveEnabledState() const;
    void OnGateWeaveEnabledChanged(ECheckBoxState NewState);
    float GetGateWeaveAmplitude() const;
    void OnGateWeaveAmplitudeChanged(float NewValue);
    float GetGateWeaveFrequency() const;
    void OnGateWeaveFrequencyChanged(float NewValue);

    ECheckBoxState GetFlickerEnabledState() const;
    void OnFlickerEnabledChanged(ECheckBoxState NewState);
    float GetFlickerIntensity() const;
    void OnFlickerIntensityChanged(float NewValue);
    float GetFlickerFrequency() const;
    void OnFlickerFrequencyChanged(float NewValue);

    ECheckBoxState GetGateScratchEnabledState() const;
    void OnGateScratchEnabledChanged(ECheckBoxState NewState);
    float GetGateScratchIntensity() const;
    void OnGateScratchIntensityChanged(float NewValue);
    float GetGateScratchDensity() const;
    void OnGateScratchDensityChanged(float NewValue);
    float GetGateScratchWidth() const;
    void OnGateScratchWidthChanged(float NewValue);
    float GetGateScratchWidthJitter() const;
    void OnGateScratchWidthJitterChanged(float NewValue);
    float GetGateScratchLength() const;
    void OnGateScratchLengthChanged(float NewValue);
    float GetGateScratchLengthJitter() const;
    void OnGateScratchLengthJitterChanged(float NewValue);
    float GetGateScratchOpacityJitter() const;
    void OnGateScratchOpacityJitterChanged(float NewValue);
    float GetGateScratchFrequency() const;
    void OnGateScratchFrequencyChanged(float NewValue);
    ECheckBoxState GetGateScratchAutoPolarityState() const;
    void OnGateScratchAutoPolarityChanged(ECheckBoxState NewState);
    bool IsGateScratchPolarityEditable() const;
    float GetGateScratchPolarity() const;
    void OnGateScratchPolarityChanged(float NewValue);
    float GetGateScratchTintR() const;
    void OnGateScratchTintRChanged(float NewValue);
    float GetGateScratchTintG() const;
    void OnGateScratchTintGChanged(float NewValue);
    float GetGateScratchTintB() const;
    void OnGateScratchTintBChanged(float NewValue);
    ECheckBoxState GetDirtEnabledState() const;
    void OnDirtEnabledChanged(ECheckBoxState NewState);
    float GetDirtIntensity() const;
    void OnDirtIntensityChanged(float NewValue);
    float GetDirtDensity() const;
    void OnDirtDensityChanged(float NewValue);
    float GetDirtSize() const;
    void OnDirtSizeChanged(float NewValue);
    float GetDirtSizeJitter() const;
    void OnDirtSizeJitterChanged(float NewValue);
    float GetDirtOpacityJitter() const;
    void OnDirtOpacityJitterChanged(float NewValue);
    float GetDirtSoftness() const;
    void OnDirtSoftnessChanged(float NewValue);
    float GetDirtFrequency() const;
    void OnDirtFrequencyChanged(float NewValue);
    ECheckBoxState GetDirtAutoPolarityState() const;
    void OnDirtAutoPolarityChanged(ECheckBoxState NewState);
    bool IsDirtPolarityEditable() const;
    float GetDirtPolarity() const;
    void OnDirtPolarityChanged(float NewValue);
    float GetDirtTintR() const;
    void OnDirtTintRChanged(float NewValue);
    float GetDirtTintG() const;
    void OnDirtTintGChanged(float NewValue);
    float GetDirtTintB() const;
    void OnDirtTintBChanged(float NewValue);
    ECheckBoxState GetDirtUseTextureState() const;
    void OnDirtUseTextureChanged(ECheckBoxState NewState);
    ECheckBoxState GetDirtInvertTextureState() const;
    void OnDirtInvertTextureChanged(ECheckBoxState NewState);
    bool IsDirtTextureEnabled() const;
    FString GetDirtTexturePath() const;
    void OnDirtTextureChanged(const FAssetData& AssetData);
    float GetDirtTextureTiling() const;
    void OnDirtTextureTilingChanged(float NewValue);
    float GetDirtTextureScaleMin() const;
    void OnDirtTextureScaleMinChanged(float NewValue);
    float GetDirtTextureScaleMax() const;
    void OnDirtTextureScaleMaxChanged(float NewValue);
    float GetDirtNoiseScale() const;
    void OnDirtNoiseScaleChanged(float NewValue);
    float GetDirtNoiseStrength() const;
    void OnDirtNoiseStrengthChanged(float NewValue);
    float GetDirtNoiseSpeed() const;
    void OnDirtNoiseSpeedChanged(float NewValue);

    ECheckBoxState GetHalationEnabledState() const;
    void OnHalationEnabledChanged(ECheckBoxState NewState);

    float GetHalationIntensity() const;
    void OnHalationIntensityChanged(float NewValue);

    float GetHalationRadius() const;
    void OnHalationRadiusChanged(float NewValue);

    float GetHalationThreshold() const;
    void OnHalationThresholdChanged(float NewValue);

    float GetHalationTintR() const;
    void OnHalationTintRChanged(float NewValue);

    float GetHalationTintG() const;
    void OnHalationTintGChanged(float NewValue);

    float GetHalationTintB() const;
    void OnHalationTintBChanged(float NewValue);

    float GetHalationRadiusMidScale() const;
    void OnHalationRadiusMidScaleChanged(float NewValue);

    float GetHalationRadiusFarScale() const;
    void OnHalationRadiusFarScaleChanged(float NewValue);

    float GetHalationTintNearR() const;
    void OnHalationTintNearRChanged(float NewValue);
    float GetHalationTintNearG() const;
    void OnHalationTintNearGChanged(float NewValue);
    float GetHalationTintNearB() const;
    void OnHalationTintNearBChanged(float NewValue);

    float GetHalationTintMidR() const;
    void OnHalationTintMidRChanged(float NewValue);
    float GetHalationTintMidG() const;
    void OnHalationTintMidGChanged(float NewValue);
    float GetHalationTintMidB() const;
    void OnHalationTintMidBChanged(float NewValue);

    float GetHalationTintFarR() const;
    void OnHalationTintFarRChanged(float NewValue);
    float GetHalationTintFarG() const;
    void OnHalationTintFarGChanged(float NewValue);
    float GetHalationTintFarB() const;
    void OnHalationTintFarBChanged(float NewValue);

    FReply OnSaveConfigClicked();
    FReply OnReloadConfigClicked();
    FReply OnImportPresetsClicked();
    FReply OnCheckPresetsClicked();
    FReply OnCheckLutsClicked();
    FReply OnResetPresetFromJsonClicked();
    bool CanResetPresetFromJson() const;

    UFilmEmulatorSettings* GetSettings() const;

    TArray<TSharedPtr<FFilmPresetOption>> PresetOptions;
    TSharedPtr<FFilmPresetOption> SelectedPreset;
    TSharedPtr<class SComboBox<TSharedPtr<FFilmPresetOption>>> PresetComboBox;

    TArray<TSharedPtr<FName>> PrintProfileOptions;
    TSharedPtr<FName> SelectedPrintProfile;
    TSharedPtr<class SComboBox<TSharedPtr<FName>>> PrintProfileComboBox;
};






