// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FilmEmulatorColorProfiles.h"

#include "FilmStockPreset.generated.h"
class UFilmEmulatorLUT;
class UTexture2D;

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmGrainSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (DisplayName = "Enabled"))
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "16.0", ClampMax = "3200.0"))
    float ISO = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Intensity"))
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "4.0", ClampMax = "80.0", DisplayName = "Size (microns)"))
    float Size = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "2.0", DisplayName = "Radius Sigma (Factor)"))
    float SigmaR = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "1.0", AdvancedDisplay, DisplayName = "Radius Sigma Factor (Legacy)"))
    float Softness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "3.0", DisplayName = "Filter Sigma"))
    float FilterSigma = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Chromatic = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Response = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Grain", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Animation Amplitude"))
    float AnimationAmplitude = 1.0f;

    UPROPERTY(Transient)
    float FormatScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmHalationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float Radius = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (ClampMin = "1.0", ClampMax = "8.0", AdvancedDisplay, DisplayName = "Mid Radius Scale"))
    float RadiusMidScale = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (ClampMin = "1.0", ClampMax = "8.0", AdvancedDisplay, DisplayName = "Far Radius Scale"))
    float RadiusFarScale = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float Threshold = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (ClampMin = "0.0", ClampMax = "1.0", AdvancedDisplay, DisplayName = "Remjet Strength"))
    float Remjet = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (DisplayName = "Tint (Base)"))
    FLinearColor Tint = FLinearColor(1.0f, 0.4f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (AdvancedDisplay, DisplayName = "Tint Near (Override)"))
    FLinearColor TintNear = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (AdvancedDisplay, DisplayName = "Tint Mid (Override)"))
    FLinearColor TintMid = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Halation", meta = (AdvancedDisplay, DisplayName = "Tint Far (Override)"))
    FLinearColor TintFar = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
};


USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmGateWeaveSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Weave", meta = (DisplayName = "Enabled"))
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Weave", meta = (ClampMin = "0.0", ClampMax = "0.5", DisplayName = "Amplitude (mm)"))
    float Amplitude = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Weave", meta = (ClampMin = "0.0", ClampMax = "12.0", DisplayName = "Frequency (Hz)"))
    float Frequency = 0.6f;

    UPROPERTY(Transient)
    float FormatScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmFlickerSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure Flicker", meta = (DisplayName = "Enabled"))
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure Flicker", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Intensity (EV)"))
    float Intensity = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure Flicker", meta = (ClampMin = "0.0", ClampMax = "12.0", DisplayName = "Frequency (Hz)"))
    float Frequency = 0.3f;
};

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmGateScratchSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (DisplayName = "Enabled"))
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Intensity"))
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Density"))
    float Density = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.5", ClampMax = "20.0", DisplayName = "Width (microns)"))
    float Width = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Width Jitter"))
    float WidthJitter = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.1", ClampMax = "1.0", DisplayName = "Length"))
    float Length = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Length Jitter"))
    float LengthJitter = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Opacity Jitter"))
    float OpacityJitter = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "0.0", ClampMax = "12.0", DisplayName = "Frequency (Hz)"))
    float Frequency = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (DisplayName = "Auto Polarity"))
    bool bAutoPolarity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (ClampMin = "-1.0", ClampMax = "1.0", EditCondition = "!bAutoPolarity", DisplayName = "Polarity (Dark=-1, Light=+1)"))
    float Polarity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate Scratch", meta = (DisplayName = "Tint"))
    FLinearColor Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    UPROPERTY(Transient)
    float FormatScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmDirtSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (DisplayName = "Enabled"))
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Intensity"))
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Density"))
    float Density = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "2.0", ClampMax = "2000.0", DisplayName = "Size (microns)"))
    float Size = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Size Jitter"))
    float SizeJitter = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Opacity Jitter"))
    float OpacityJitter = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Softness"))
    float Softness = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "12.0", DisplayName = "Frequency (Hz)"))
    float Frequency = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (DisplayName = "Auto Polarity"))
    bool bAutoPolarity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "-1.0", ClampMax = "1.0", EditCondition = "!bAutoPolarity", DisplayName = "Polarity (Dark=-1, Light=+1)"))
    float Polarity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (DisplayName = "Tint"))
    FLinearColor Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (DisplayName = "Use Texture Based"))
    bool bUseTexture = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (DisplayName = "Invert Texture", EditCondition = "bUseTexture"))
    bool bInvertTexture = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (EditCondition = "bUseTexture", DisplayName = "Damage Texture"))
    TSoftObjectPtr<UTexture2D> DamageTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.1", ClampMax = "8.0", DisplayName = "Texture Tiling", EditCondition = "bUseTexture"))
    float TextureTiling = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.1", ClampMax = "8.0", DisplayName = "Texture Scale Min", EditCondition = "bUseTexture"))
    float TextureScaleMin = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.1", ClampMax = "8.0", DisplayName = "Texture Scale Max", EditCondition = "bUseTexture"))
    float TextureScaleMax = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.1", ClampMax = "12.0", DisplayName = "Noise Scale"))
    float NoiseScale = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Noise Strength"))
    float NoiseStrength = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Damage", meta = (ClampMin = "0.0", ClampMax = "5.0", DisplayName = "Noise Speed"))
    float NoiseSpeed = 0.25f;

    UPROPERTY(Transient)
    float FormatScale = 1.0f;
};
UENUM(BlueprintType)
enum class EFilmEmulatorFilmFormat : uint8
{
    Photo35 UMETA(DisplayName = "35mm Photo"),
    Cine35 UMETA(DisplayName = "Super 35"),
    Cine16 UMETA(DisplayName = "16mm"),
    Cine8 UMETA(DisplayName = "Super 8"),
    MediumFormat UMETA(DisplayName = "Medium Format"),
    LargeFormat UMETA(DisplayName = "Large Format / Paper"),
    Instant UMETA(DisplayName = "Instant"),
    Cine35Academy UMETA(DisplayName = "35mm (Academy)"),
    Cine35Techniscope UMETA(DisplayName = "Techniscope (2-perf)"),
    Cine16Super UMETA(DisplayName = "Super 16"),
    Cine8Standard UMETA(DisplayName = "8mm (Standard)"),
    Cine65 UMETA(DisplayName = "65mm"),
    Cine70 UMETA(DisplayName = "70mm"),
    Cinerama UMETA(DisplayName = "Cinerama (3-strip)"),
    Kinetoscope UMETA(DisplayName = "Kinetoscope")
};

inline const TCHAR* LexToString(EFilmEmulatorFilmFormat Format)
{
    switch (Format)
    {
    case EFilmEmulatorFilmFormat::Photo35:     return TEXT("35mm Photo");
    case EFilmEmulatorFilmFormat::Cine35:      return TEXT("Super 35");
    case EFilmEmulatorFilmFormat::Cine16:      return TEXT("16mm");
    case EFilmEmulatorFilmFormat::Cine8:       return TEXT("Super 8");
    case EFilmEmulatorFilmFormat::MediumFormat:return TEXT("Medium Format");
    case EFilmEmulatorFilmFormat::LargeFormat: return TEXT("Large Format / Paper");
    case EFilmEmulatorFilmFormat::Instant:     return TEXT("Instant");
    case EFilmEmulatorFilmFormat::Cine35Academy: return TEXT("35mm (Academy)");
    case EFilmEmulatorFilmFormat::Cine35Techniscope: return TEXT("Techniscope (2-perf)");
    case EFilmEmulatorFilmFormat::Cine16Super: return TEXT("Super 16");
    case EFilmEmulatorFilmFormat::Cine8Standard: return TEXT("8mm (Standard)");
    case EFilmEmulatorFilmFormat::Cine65: return TEXT("65mm");
    case EFilmEmulatorFilmFormat::Cine70: return TEXT("70mm");
    case EFilmEmulatorFilmFormat::Cinerama: return TEXT("Cinerama (3-strip)");
    case EFilmEmulatorFilmFormat::Kinetoscope: return TEXT("Kinetoscope");
    default: return TEXT("Unknown");
    }
}

UCLASS(BlueprintType)
class FILMEMULATOR_API UFilmStockPreset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (DisplayName = "Preset Id (JSON)", AdvancedDisplay))
    FName PresetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    EFilmEmulatorFilmType FilmType = EFilmEmulatorFilmType::ColorNegative;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (DisplayName = "Film Format"))
    EFilmEmulatorFilmFormat FilmFormat = EFilmEmulatorFilmFormat::Photo35;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (ClampMin = "0.25", ClampMax = "10.0", DisplayName = "Film Format Scale"))
    float FilmFormatScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (DisplayName = "Film LUT (3D)"))
    TSoftObjectPtr<UFilmEmulatorLUT> FilmLUTAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (DisplayName = "Film LUT (Legacy 2D)", AdvancedDisplay))
    TSoftObjectPtr<UTexture2D> FilmLUT;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (FilePathFilter = "png;bmp;exr;hdr;cube", AdvancedDisplay))
    FFilePath FilmLUTPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float SaturationBias = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ContrastBias = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    float ExposureBias = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FFilmGrainSettings Grain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock", meta = (AdvancedDisplay, DisplayName = "Grain Defaults (From JSON)"))
    FFilmGrainSettings GrainDefaults;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FFilmHalationSettings Halation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FFilmGateWeaveSettings GateWeave;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FFilmFlickerSettings Flicker;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FFilmGateScratchSettings GateScratch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Stock")
    FFilmDirtSettings Dirt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Print Strength"))
    float PrintStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print", meta = (DisplayName = "Print LUT (3D)"))
    TSoftObjectPtr<UFilmEmulatorLUT> FilmPrintLUTAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print", meta = (DisplayName = "Print LUT (Legacy 2D)", AdvancedDisplay))
    TSoftObjectPtr<UTexture2D> FilmPrintLUT;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print", meta = (FilePathFilter = "png;bmp;exr;hdr;cube", AdvancedDisplay))
    FFilePath FilmPrintLUTPath;
};







UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FILMEMULATOR_API UFilmStockPresetInline : public UFilmStockPreset
{
    GENERATED_BODY()
};


