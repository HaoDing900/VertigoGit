// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FilmEmulatorColorProfiles.h"
#include "FilmStockPreset.h"
#include "FilmEmulatorSettings.generated.h"

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmEmulatorParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|General")
    bool bEnableFilmEmulation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Color")
    FName FilmProfileId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Print")
    FName FilmPrintProfileId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Print", meta = (DisplayName = "Use Film Print"))
    bool bEnableFilmPrint = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Color",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Strength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Color",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Saturation = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Color",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Contrast = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Color",
        meta = (ClampMin = "-5.0", ClampMax = "5.0"))
    float ExposureEV = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Print",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PrintStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FilmEmulator|Print",
        meta = (ClampMin = "-5.0", ClampMax = "5.0"))
    float PrintExposureEV = 0.0f;

    // Dithering and LUT inversion are intentionally omitted; LUTs are generated in positive space.
};

UCLASS(defaultconfig, config = Engine, meta = (DisplayName = "Film Emulator"))
class FILMEMULATOR_API UFilmEmulatorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UFilmEmulatorSettings();

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator")
    FFilmEmulatorParams DefaultParams;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Output", meta = (DisplayName = "Apply After Tonemapper"))
    bool bApplyAfterTonemapper = false;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Color")
    TArray<FFilmColorProfile> FilmProfiles;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Print")
    TArray<FFilmPrintProfile> FilmPrintProfiles;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Color")
    FName DefaultFilmProfileId;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Print")
    FName DefaultFilmPrintProfileId;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Presets")
    TSoftObjectPtr<UFilmStockPreset> DefaultPreset;

    UPROPERTY(config, EditAnywhere, Category = "Film Emulator|Presets", meta = (DisplayName = "Default Preset Id (JSON)"))
    FName DefaultPresetId;

    const FFilmEmulatorParams& GetDefaultParams() const { return DefaultParams; }
    void SaveSettingsConfig();
    void ReloadSettingsConfig();

    const FFilmColorProfile* FindFilmProfile(FName ProfileId) const;
    const FFilmColorProfile& GetDefaultFilmProfile() const;
    const FFilmPrintProfile* FindFilmPrintProfile(FName ProfileId) const;
    const FFilmPrintProfile& GetDefaultFilmPrintProfile() const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    virtual FName GetCategoryName() const override { return FName("Plugins"); }
    virtual FName GetSectionName() const override { return FName("Film Emulator"); }
};




