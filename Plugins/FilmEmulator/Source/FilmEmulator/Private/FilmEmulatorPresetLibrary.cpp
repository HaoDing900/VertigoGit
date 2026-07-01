// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorPresetLibrary.h"

#include "FilmEmulatorColorProfiles.h"
#include "FilmStockPreset.h"
#include "FilmEmulatorLUT.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogFilmEmulatorPresets, Log, All);

namespace
{
bool TryGetNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    double Value = 0.0;
    if (!Object->TryGetNumberField(Field, Value))
    {
        return false;
    }

    OutValue = static_cast<float>(Value);
    return true;
}

float GetNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float DefaultValue)
{
    float Value = DefaultValue;
    return TryGetNumberField(Object, Field, Value) ? Value : DefaultValue;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool DefaultValue)
{
    if (!Object.IsValid())
    {
        return DefaultValue;
    }

    bool Value = false;
    if (!Object->TryGetBoolField(Field, Value))
    {
        return DefaultValue;
    }

    return Value;
}

bool TryParseFilmType(const FString& InString, EFilmEmulatorFilmType& OutType)
{
    if (InString.IsEmpty())
    {
        return false;
    }

    const UEnum* Enum = StaticEnum<EFilmEmulatorFilmType>();
    if (Enum)
    {
        int64 Value = Enum->GetValueByNameString(InString);
        if (Value != INDEX_NONE)
        {
            OutType = static_cast<EFilmEmulatorFilmType>(Value);
            return true;
        }

        FString Compact = InString;
        Compact.ReplaceInline(TEXT(" "), TEXT(""));
        Compact.ReplaceInline(TEXT("_"), TEXT(""));
        Compact.ReplaceInline(TEXT("-"), TEXT(""));
        Compact.ReplaceInline(TEXT("&"), TEXT("And"));
        Value = Enum->GetValueByNameString(Compact);
        if (Value != INDEX_NONE)
        {
            OutType = static_cast<EFilmEmulatorFilmType>(Value);
            return true;
        }
    }

    FString Normalized = InString.ToLower();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT("&"), TEXT("and"));

    if (Normalized == TEXT("colornegative") || Normalized == TEXT("negative") || Normalized == TEXT("cn"))
    {
        OutType = EFilmEmulatorFilmType::ColorNegative;
        return true;
    }

    if (Normalized == TEXT("colorslide") || Normalized == TEXT("colorpositive") || Normalized == TEXT("positive") || Normalized == TEXT("slide") || Normalized == TEXT("cp"))
    {
        OutType = EFilmEmulatorFilmType::ColorSlide;
        return true;
    }

    if (Normalized == TEXT("bwnegative") || Normalized == TEXT("blackwhitenegative") || Normalized == TEXT("blackandwhitenegative") || Normalized == TEXT("bwn"))
    {
        OutType = EFilmEmulatorFilmType::BWNegative;
        return true;
    }

    if (Normalized == TEXT("bwpositive") || Normalized == TEXT("blackwhiteprint") || Normalized == TEXT("blackandwhiteprint") || Normalized == TEXT("bwprint") || Normalized == TEXT("bwp"))
    {
        OutType = EFilmEmulatorFilmType::BWPositive;
        return true;
    }

    return false;
}

bool TryParseFilmFormat(const FString& InString, EFilmEmulatorFilmFormat& OutFormat)
{
    if (InString.IsEmpty())
    {
        return false;
    }

    FString Normalized = InString.ToLower();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));

    if (Normalized == TEXT("35mm") || Normalized == TEXT("photo35") || Normalized == TEXT("fullframe") || Normalized == TEXT("ff"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Photo35;
        return true;
    }

    if (Normalized == TEXT("cine35") || Normalized == TEXT("s35") || Normalized == TEXT("super35") || Normalized == TEXT("35mmcine"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine35;
        return true;
    }

    if (Normalized == TEXT("academy35") || Normalized == TEXT("35mmacademy") || Normalized == TEXT("academy") || Normalized == TEXT("cine35academy"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine35Academy;
        return true;
    }

    if (Normalized == TEXT("techniscope") || Normalized == TEXT("2perf") || Normalized == TEXT("2perf35") || Normalized == TEXT("35mm2perf") || Normalized == TEXT("2perfcine"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine35Techniscope;
        return true;
    }

    if (Normalized == TEXT("super16") || Normalized == TEXT("s16"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine16Super;
        return true;
    }

    if (Normalized == TEXT("16mm") || Normalized == TEXT("cine16"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine16;
        return true;
    }

    if (Normalized == TEXT("super8") || Normalized == TEXT("s8") || Normalized == TEXT("cine8"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine8;
        return true;
    }

    if (Normalized == TEXT("8mm") || Normalized == TEXT("standard8") || Normalized == TEXT("regular8") || Normalized == TEXT("double8") || Normalized == TEXT("std8"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine8Standard;
        return true;
    }

    if (Normalized == TEXT("65mm"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine65;
        return true;
    }

    if (Normalized == TEXT("70mm"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cine70;
        return true;
    }

    if (Normalized == TEXT("cinerama"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Cinerama;
        return true;
    }

    if (Normalized == TEXT("kinetoscope"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Kinetoscope;
        return true;
    }

    if (Normalized == TEXT("medium") || Normalized == TEXT("mediumformat") || Normalized == TEXT("120") || Normalized == TEXT("6x6"))
    {
        OutFormat = EFilmEmulatorFilmFormat::MediumFormat;
        return true;
    }

    if (Normalized == TEXT("large") || Normalized == TEXT("largeformat") || Normalized == TEXT("paper") || Normalized == TEXT("print"))
    {
        OutFormat = EFilmEmulatorFilmFormat::LargeFormat;
        return true;
    }

    if (Normalized == TEXT("instant") || Normalized == TEXT("instax") || Normalized == TEXT("fp100c"))
    {
        OutFormat = EFilmEmulatorFilmFormat::Instant;
        return true;
    }

    return false;
}

float GetFilmFormatScale(EFilmEmulatorFilmFormat Format)
{
    switch (Format)
    {
    case EFilmEmulatorFilmFormat::Cine35:
        return 1.4f;
    case EFilmEmulatorFilmFormat::Cine35Academy:
        return 1.57f;
    case EFilmEmulatorFilmFormat::Cine35Techniscope:
        return 2.04f;
    case EFilmEmulatorFilmFormat::Cine16:
        return 2.8f;
    case EFilmEmulatorFilmFormat::Cine16Super:
        return 3.05f;
    case EFilmEmulatorFilmFormat::Cine8:
        return 5.6f;
    case EFilmEmulatorFilmFormat::Cine8Standard:
        return 7.17f;
    case EFilmEmulatorFilmFormat::Cine65:
        return 0.84f;
    case EFilmEmulatorFilmFormat::Cine70:
        return 0.90f;
    case EFilmEmulatorFilmFormat::Cinerama:
        return 0.63f;
    case EFilmEmulatorFilmFormat::Kinetoscope:
        return 1.34f;
    case EFilmEmulatorFilmFormat::MediumFormat:
        return 0.7f;
    case EFilmEmulatorFilmFormat::LargeFormat:
        return 0.35f;
    case EFilmEmulatorFilmFormat::Instant:
        return 0.6f;
    case EFilmEmulatorFilmFormat::Photo35:
    default:
        return 1.0f;
    }
}

bool TryParseLinearColor(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FLinearColor& OutColor)
{
    if (!Object.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Object->TryGetArrayField(Field, Array) && Array)
    {
        if (Array->Num() >= 3)
        {
            const float R = static_cast<float>((*Array)[0]->AsNumber());
            const float G = static_cast<float>((*Array)[1]->AsNumber());
            const float B = static_cast<float>((*Array)[2]->AsNumber());
            const float A = Array->Num() >= 4 ? static_cast<float>((*Array)[3]->AsNumber()) : 1.0f;
            OutColor = FLinearColor(R, G, B, A);
            return true;
        }
    }

    const TSharedPtr<FJsonObject>* ColorObject = nullptr;
    if (Object->TryGetObjectField(Field, ColorObject) && ColorObject && ColorObject->IsValid())
    {
        const float R = GetNumberField(*ColorObject, TEXT("r"), OutColor.R);
        const float G = GetNumberField(*ColorObject, TEXT("g"), OutColor.G);
        const float B = GetNumberField(*ColorObject, TEXT("b"), OutColor.B);
        const float A = GetNumberField(*ColorObject, TEXT("a"), OutColor.A);
        OutColor = FLinearColor(R, G, B, A);
        return true;
    }

    return false;
}

bool IsLikelyFilePath(const FString& Path)
{
    const FString Extension = FPaths::GetExtension(Path).ToLower();
    return Extension == TEXT("cube") || Extension == TEXT("png") || Extension == TEXT("bmp") || Extension == TEXT("exr") || Extension == TEXT("hdr");
}

bool ParsePresetObject(const TSharedPtr<FJsonObject>& PresetObject, const FString& SourcePath, FFilmPresetEntry& OutEntry)
{
    if (!PresetObject.IsValid())
    {
        return false;
    }

    FString PresetIdString;
    if (!PresetObject->TryGetStringField(TEXT("id"), PresetIdString))
    {
        PresetObject->TryGetStringField(TEXT("presetId"), PresetIdString);
    }

    FString DisplayName;
    PresetObject->TryGetStringField(TEXT("displayName"), DisplayName);

    FString Description;
    PresetObject->TryGetStringField(TEXT("description"), Description);

    FString FilmTypeString;
    PresetObject->TryGetStringField(TEXT("filmType"), FilmTypeString);

    FName PresetId = NAME_None;
    if (!PresetIdString.IsEmpty())
    {
        PresetId = FName(*PresetIdString);
    }
    else
    {
        PresetId = FName(*FPaths::GetBaseFilename(SourcePath));
    }

    UFilmStockPreset* Preset = NewObject<UFilmStockPreset>(GetTransientPackage(), NAME_None, RF_Transient);
    if (!Preset)
    {
        return false;
    }

    Preset->PresetId = PresetId;
    Preset->DisplayName = !DisplayName.IsEmpty() ? FText::FromString(DisplayName) : FText::FromName(PresetId);
    Preset->Description = !Description.IsEmpty() ? FText::FromString(Description) : FText::GetEmpty();

    if (!FilmTypeString.IsEmpty())
    {
        EFilmEmulatorFilmType ParsedType = Preset->FilmType;
        if (TryParseFilmType(FilmTypeString, ParsedType))
        {
            Preset->FilmType = ParsedType;
        }
        else
        {
            UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Unknown film type '%s' in %s"), *FilmTypeString, *SourcePath);
        }
    }

    FString FormatString;
    if (PresetObject->TryGetStringField(TEXT("format"), FormatString) || PresetObject->TryGetStringField(TEXT("filmFormat"), FormatString))
    {
        EFilmEmulatorFilmFormat ParsedFormat = Preset->FilmFormat;
        if (TryParseFilmFormat(FormatString, ParsedFormat))
        {
            Preset->FilmFormat = ParsedFormat;
        }
        else
        {
            UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Unknown film format '%s' in %s"), *FormatString, *SourcePath);
        }
    }

    float FormatScale = Preset->FilmFormatScale;
    bool bFormatScaleExplicit = false;
    if (TryGetNumberField(PresetObject, TEXT("formatScale"), FormatScale))
    {
        Preset->FilmFormatScale = FormatScale;
        bFormatScaleExplicit = true;
    }

    FString LutAssetPath;
    if (PresetObject->TryGetStringField(TEXT("lut"), LutAssetPath) || PresetObject->TryGetStringField(TEXT("lutAsset"), LutAssetPath))
    {
        if (IsLikelyFilePath(LutAssetPath))
        {
            Preset->FilmLUTPath.FilePath = LutAssetPath;
        }
        else
        {
            const FSoftObjectPath SoftPath(LutAssetPath);
            if (SoftPath.IsValid())
            {
                Preset->FilmLUTAsset = TSoftObjectPtr<UFilmEmulatorLUT>(SoftPath);
            }
        }
    }

    FString LutTexturePath;
    if (PresetObject->TryGetStringField(TEXT("lutTexture"), LutTexturePath) || PresetObject->TryGetStringField(TEXT("lut2d"), LutTexturePath))
    {
        const FSoftObjectPath SoftPath(LutTexturePath);
        if (SoftPath.IsValid())
        {
            Preset->FilmLUT = TSoftObjectPtr<UTexture2D>(SoftPath);
        }
        else if (IsLikelyFilePath(LutTexturePath))
        {
            Preset->FilmLUTPath.FilePath = LutTexturePath;
        }
    }

    FString LutFilePath;
    if (PresetObject->TryGetStringField(TEXT("lutPath"), LutFilePath) || PresetObject->TryGetStringField(TEXT("lutFile"), LutFilePath))
    {
        Preset->FilmLUTPath.FilePath = LutFilePath;
    }

    Preset->SaturationBias = GetNumberField(PresetObject, TEXT("saturationBias"), Preset->SaturationBias);
    Preset->ContrastBias = GetNumberField(PresetObject, TEXT("contrastBias"), Preset->ContrastBias);
    Preset->ExposureBias = GetNumberField(PresetObject, TEXT("exposureBias"), Preset->ExposureBias);

    FString PrintLutAssetPath;
    if (PresetObject->TryGetStringField(TEXT("printLut"), PrintLutAssetPath) || PresetObject->TryGetStringField(TEXT("printLutAsset"), PrintLutAssetPath))
    {
        if (IsLikelyFilePath(PrintLutAssetPath))
        {
            Preset->FilmPrintLUTPath.FilePath = PrintLutAssetPath;
        }
        else
        {
            const FSoftObjectPath SoftPath(PrintLutAssetPath);
            if (SoftPath.IsValid())
            {
                Preset->FilmPrintLUTAsset = TSoftObjectPtr<UFilmEmulatorLUT>(SoftPath);
            }
        }
    }

    FString PrintLutTexturePath;
    if (PresetObject->TryGetStringField(TEXT("printLutTexture"), PrintLutTexturePath) || PresetObject->TryGetStringField(TEXT("printLut2d"), PrintLutTexturePath))
    {
        const FSoftObjectPath SoftPath(PrintLutTexturePath);
        if (SoftPath.IsValid())
        {
            Preset->FilmPrintLUT = TSoftObjectPtr<UTexture2D>(SoftPath);
        }
        else if (IsLikelyFilePath(PrintLutTexturePath))
        {
            Preset->FilmPrintLUTPath.FilePath = PrintLutTexturePath;
        }
    }

    FString PrintLutFilePath;
    if (PresetObject->TryGetStringField(TEXT("printLutPath"), PrintLutFilePath) || PresetObject->TryGetStringField(TEXT("printLutFile"), PrintLutFilePath))
    {
        Preset->FilmPrintLUTPath.FilePath = PrintLutFilePath;
    }

    Preset->PrintStrength = GetNumberField(PresetObject, TEXT("printStrength"), Preset->PrintStrength);

    const TSharedPtr<FJsonObject>* GrainObject = nullptr;
    if (PresetObject->TryGetObjectField(TEXT("grain"), GrainObject) && GrainObject && GrainObject->IsValid())
    {
        Preset->Grain.bEnabled = GetBoolField(*GrainObject, TEXT("enabled"), Preset->Grain.bEnabled);

        if (!bFormatScaleExplicit)
        {
            if (TryGetNumberField(*GrainObject, TEXT("formatScale"), FormatScale))
            {
                Preset->FilmFormatScale = FormatScale;
                bFormatScaleExplicit = true;
            }
        }

        if (!TryGetNumberField(*GrainObject, TEXT("iso"), Preset->Grain.ISO))
        {
            TryGetNumberField(*GrainObject, TEXT("isoSpeed"), Preset->Grain.ISO);
        }

        Preset->Grain.Intensity = GetNumberField(*GrainObject, TEXT("intensity"), Preset->Grain.Intensity);
        Preset->Grain.Size = GetNumberField(*GrainObject, TEXT("size"), Preset->Grain.Size);
        if (!TryGetNumberField(*GrainObject, TEXT("sigmaR"), Preset->Grain.SigmaR))
        {
            if (!TryGetNumberField(*GrainObject, TEXT("radiusSigma"), Preset->Grain.SigmaR))
            {
                TryGetNumberField(*GrainObject, TEXT("grainSigma"), Preset->Grain.SigmaR);
            }
        }
        Preset->Grain.Softness = GetNumberField(*GrainObject, TEXT("softness"), Preset->Grain.Softness);
        Preset->Grain.FilterSigma = GetNumberField(*GrainObject, TEXT("filterSigma"), Preset->Grain.FilterSigma);
        Preset->Grain.Chromatic = GetNumberField(*GrainObject, TEXT("chromatic"), Preset->Grain.Chromatic);
        Preset->Grain.Response = GetNumberField(*GrainObject, TEXT("response"), Preset->Grain.Response);
        Preset->Grain.AnimationAmplitude = GetNumberField(*GrainObject, TEXT("animationAmplitude"), Preset->Grain.AnimationAmplitude);
        Preset->Grain.AnimationAmplitude = GetNumberField(*GrainObject, TEXT("animation"), Preset->Grain.AnimationAmplitude);
        Preset->Grain.AnimationAmplitude = GetNumberField(*GrainObject, TEXT("animAmplitude"), Preset->Grain.AnimationAmplitude);
    }
    if (!bFormatScaleExplicit)
    {
        Preset->FilmFormatScale = GetFilmFormatScale(Preset->FilmFormat);
    }
    Preset->GrainDefaults = Preset->Grain;

    const TSharedPtr<FJsonObject>* HalationObject = nullptr;
    if (PresetObject->TryGetObjectField(TEXT("halation"), HalationObject) && HalationObject && HalationObject->IsValid())
    {
        Preset->Halation.bEnabled = GetBoolField(*HalationObject, TEXT("enabled"), Preset->Halation.bEnabled);
        Preset->Halation.Intensity = GetNumberField(*HalationObject, TEXT("intensity"), Preset->Halation.Intensity);
        Preset->Halation.Radius = GetNumberField(*HalationObject, TEXT("radius"), Preset->Halation.Radius);
        Preset->Halation.Threshold = GetNumberField(*HalationObject, TEXT("threshold"), Preset->Halation.Threshold);
        Preset->Halation.Remjet = GetNumberField(*HalationObject, TEXT("remjet"), Preset->Halation.Remjet);
        Preset->Halation.Remjet = GetNumberField(*HalationObject, TEXT("remJet"), Preset->Halation.Remjet);
        Preset->Halation.Remjet = GetNumberField(*HalationObject, TEXT("antiHalation"), Preset->Halation.Remjet);
        TryParseLinearColor(*HalationObject, TEXT("tint"), Preset->Halation.Tint);
        Preset->Halation.RadiusMidScale = GetNumberField(*HalationObject, TEXT("radiusMidScale"), Preset->Halation.RadiusMidScale);
        Preset->Halation.RadiusFarScale = GetNumberField(*HalationObject, TEXT("radiusFarScale"), Preset->Halation.RadiusFarScale);
        TryParseLinearColor(*HalationObject, TEXT("tintNear"), Preset->Halation.TintNear);
        TryParseLinearColor(*HalationObject, TEXT("tintMid"), Preset->Halation.TintMid);
        TryParseLinearColor(*HalationObject, TEXT("tintFar"), Preset->Halation.TintFar);
        auto ApplyHalationTintFallback = [](FLinearColor& Target, const FLinearColor& Fallback)
        {
            const float Sum = Target.R + Target.G + Target.B;
            if (Target.A <= 0.0f && Sum <= KINDA_SMALL_NUMBER)
            {
                Target = Fallback;
            }
        };

        const FLinearColor BaseTint = Preset->Halation.Tint;
        const FLinearColor NearDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.6f, 0.25f, 1.0f), 0.45f);
        const FLinearColor MidDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.35f, 0.12f, 1.0f), 0.45f);
        const FLinearColor FarDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.2f, 0.05f, 1.0f), 0.7f);

        ApplyHalationTintFallback(Preset->Halation.TintNear, NearDefault);
        ApplyHalationTintFallback(Preset->Halation.TintMid, MidDefault);
        ApplyHalationTintFallback(Preset->Halation.TintFar, FarDefault);

        Preset->Halation.RadiusMidScale = FMath::Clamp(Preset->Halation.RadiusMidScale, 1.0f, 8.0f);
        Preset->Halation.RadiusFarScale = FMath::Clamp(Preset->Halation.RadiusFarScale, Preset->Halation.RadiusMidScale, 8.0f);
    }

    const TSharedPtr<FJsonObject>* GateWeaveObject = nullptr;
    if ((PresetObject->TryGetObjectField(TEXT("gateWeave"), GateWeaveObject)
         || PresetObject->TryGetObjectField(TEXT("weave"), GateWeaveObject)
         || PresetObject->TryGetObjectField(TEXT("gate"), GateWeaveObject))
        && GateWeaveObject && GateWeaveObject->IsValid())
    {
        Preset->GateWeave.bEnabled = GetBoolField(*GateWeaveObject, TEXT("enabled"), Preset->GateWeave.bEnabled);
        Preset->GateWeave.Amplitude = GetNumberField(*GateWeaveObject, TEXT("amplitude"), Preset->GateWeave.Amplitude);
        Preset->GateWeave.Amplitude = GetNumberField(*GateWeaveObject, TEXT("amount"), Preset->GateWeave.Amplitude);
        Preset->GateWeave.Amplitude = GetNumberField(*GateWeaveObject, TEXT("strength"), Preset->GateWeave.Amplitude);
        Preset->GateWeave.Frequency = GetNumberField(*GateWeaveObject, TEXT("frequency"), Preset->GateWeave.Frequency);
        Preset->GateWeave.Frequency = GetNumberField(*GateWeaveObject, TEXT("speed"), Preset->GateWeave.Frequency);
        Preset->GateWeave.Amplitude = FMath::Clamp(Preset->GateWeave.Amplitude, 0.0f, 0.5f);
        Preset->GateWeave.Frequency = FMath::Clamp(Preset->GateWeave.Frequency, 0.0f, 12.0f);
    }

    const TSharedPtr<FJsonObject>* FlickerObject = nullptr;
    if ((PresetObject->TryGetObjectField(TEXT("flicker"), FlickerObject)
         || PresetObject->TryGetObjectField(TEXT("exposureFlicker"), FlickerObject))
        && FlickerObject && FlickerObject->IsValid())
    {
        Preset->Flicker.bEnabled = GetBoolField(*FlickerObject, TEXT("enabled"), Preset->Flicker.bEnabled);
        Preset->Flicker.Intensity = GetNumberField(*FlickerObject, TEXT("intensity"), Preset->Flicker.Intensity);
        Preset->Flicker.Intensity = GetNumberField(*FlickerObject, TEXT("amount"), Preset->Flicker.Intensity);
        Preset->Flicker.Intensity = GetNumberField(*FlickerObject, TEXT("strength"), Preset->Flicker.Intensity);
        Preset->Flicker.Frequency = GetNumberField(*FlickerObject, TEXT("frequency"), Preset->Flicker.Frequency);
        Preset->Flicker.Frequency = GetNumberField(*FlickerObject, TEXT("speed"), Preset->Flicker.Frequency);
        Preset->Flicker.Intensity = FMath::Clamp(Preset->Flicker.Intensity, 0.0f, 1.0f);
        Preset->Flicker.Frequency = FMath::Clamp(Preset->Flicker.Frequency, 0.0f, 12.0f);
    }
    
    const TSharedPtr<FJsonObject>* ScratchObject = nullptr;
    if ((PresetObject->TryGetObjectField(TEXT("gateScratch"), ScratchObject)
         || PresetObject->TryGetObjectField(TEXT("scratch"), ScratchObject)
         || PresetObject->TryGetObjectField(TEXT("scratches"), ScratchObject))
        && ScratchObject && ScratchObject->IsValid())
    {
        Preset->GateScratch.bEnabled = GetBoolField(*ScratchObject, TEXT("enabled"), Preset->GateScratch.bEnabled);
        Preset->GateScratch.Intensity = GetNumberField(*ScratchObject, TEXT("intensity"), Preset->GateScratch.Intensity);
        Preset->GateScratch.Intensity = GetNumberField(*ScratchObject, TEXT("amount"), Preset->GateScratch.Intensity);
        Preset->GateScratch.Intensity = GetNumberField(*ScratchObject, TEXT("strength"), Preset->GateScratch.Intensity);
        Preset->GateScratch.Density = GetNumberField(*ScratchObject, TEXT("density"), Preset->GateScratch.Density);
        Preset->GateScratch.Density = GetNumberField(*ScratchObject, TEXT("count"), Preset->GateScratch.Density);
        Preset->GateScratch.Width = GetNumberField(*ScratchObject, TEXT("width"), Preset->GateScratch.Width);
        Preset->GateScratch.WidthJitter = GetNumberField(*ScratchObject, TEXT("widthJitter"), Preset->GateScratch.WidthJitter);
        Preset->GateScratch.Length = GetNumberField(*ScratchObject, TEXT("length"), Preset->GateScratch.Length);
        Preset->GateScratch.LengthJitter = GetNumberField(*ScratchObject, TEXT("lengthJitter"), Preset->GateScratch.LengthJitter);
        Preset->GateScratch.OpacityJitter = GetNumberField(*ScratchObject, TEXT("opacityJitter"), Preset->GateScratch.OpacityJitter);
        Preset->GateScratch.OpacityJitter = GetNumberField(*ScratchObject, TEXT("opacity"), Preset->GateScratch.OpacityJitter);
        Preset->GateScratch.Frequency = GetNumberField(*ScratchObject, TEXT("frequency"), Preset->GateScratch.Frequency);
        Preset->GateScratch.Frequency = GetNumberField(*ScratchObject, TEXT("speed"), Preset->GateScratch.Frequency);
        Preset->GateScratch.bAutoPolarity = GetBoolField(*ScratchObject, TEXT("autoPolarity"), Preset->GateScratch.bAutoPolarity);
        Preset->GateScratch.Polarity = GetNumberField(*ScratchObject, TEXT("polarity"), Preset->GateScratch.Polarity);
        TryParseLinearColor(*ScratchObject, TEXT("tint"), Preset->GateScratch.Tint);
        Preset->GateScratch.Intensity = FMath::Clamp(Preset->GateScratch.Intensity, 0.0f, 1.0f);
        Preset->GateScratch.Density = FMath::Clamp(Preset->GateScratch.Density, 0.0f, 1.0f);
        Preset->GateScratch.Width = FMath::Clamp(Preset->GateScratch.Width, 0.5f, 20.0f);
        Preset->GateScratch.WidthJitter = FMath::Clamp(Preset->GateScratch.WidthJitter, 0.0f, 1.0f);
        Preset->GateScratch.Length = FMath::Clamp(Preset->GateScratch.Length, 0.1f, 1.0f);
        Preset->GateScratch.LengthJitter = FMath::Clamp(Preset->GateScratch.LengthJitter, 0.0f, 1.0f);
        Preset->GateScratch.OpacityJitter = FMath::Clamp(Preset->GateScratch.OpacityJitter, 0.0f, 1.0f);
        Preset->GateScratch.Frequency = FMath::Clamp(Preset->GateScratch.Frequency, 0.0f, 12.0f);
        Preset->GateScratch.Polarity = FMath::Clamp(Preset->GateScratch.Polarity, -1.0f, 1.0f);
    }


    const TSharedPtr<FJsonObject>* DirtObject = nullptr;
    if ((PresetObject->TryGetObjectField(TEXT("filmDamage"), DirtObject)
         || PresetObject->TryGetObjectField(TEXT("damage"), DirtObject)
         || PresetObject->TryGetObjectField(TEXT("dirt"), DirtObject)
         || PresetObject->TryGetObjectField(TEXT("dust"), DirtObject))
        && DirtObject && DirtObject->IsValid())
    {
        Preset->Dirt.bEnabled = GetBoolField(*DirtObject, TEXT("enabled"), Preset->Dirt.bEnabled);
        Preset->Dirt.Intensity = GetNumberField(*DirtObject, TEXT("intensity"), Preset->Dirt.Intensity);
        Preset->Dirt.Intensity = GetNumberField(*DirtObject, TEXT("amount"), Preset->Dirt.Intensity);
        Preset->Dirt.Intensity = GetNumberField(*DirtObject, TEXT("strength"), Preset->Dirt.Intensity);
        Preset->Dirt.Density = GetNumberField(*DirtObject, TEXT("density"), Preset->Dirt.Density);
        Preset->Dirt.Density = GetNumberField(*DirtObject, TEXT("count"), Preset->Dirt.Density);
        Preset->Dirt.Size = GetNumberField(*DirtObject, TEXT("size"), Preset->Dirt.Size);
        Preset->Dirt.Size = GetNumberField(*DirtObject, TEXT("radius"), Preset->Dirt.Size);
        Preset->Dirt.SizeJitter = GetNumberField(*DirtObject, TEXT("sizeJitter"), Preset->Dirt.SizeJitter);
        Preset->Dirt.OpacityJitter = GetNumberField(*DirtObject, TEXT("opacityJitter"), Preset->Dirt.OpacityJitter);
        Preset->Dirt.OpacityJitter = GetNumberField(*DirtObject, TEXT("opacity"), Preset->Dirt.OpacityJitter);
        Preset->Dirt.Softness = GetNumberField(*DirtObject, TEXT("softness"), Preset->Dirt.Softness);
        Preset->Dirt.Frequency = GetNumberField(*DirtObject, TEXT("frequency"), Preset->Dirt.Frequency);
        Preset->Dirt.Frequency = GetNumberField(*DirtObject, TEXT("speed"), Preset->Dirt.Frequency);
        Preset->Dirt.bAutoPolarity = GetBoolField(*DirtObject, TEXT("autoPolarity"), Preset->Dirt.bAutoPolarity);
        Preset->Dirt.Polarity = GetNumberField(*DirtObject, TEXT("polarity"), Preset->Dirt.Polarity);
        TryParseLinearColor(*DirtObject, TEXT("tint"), Preset->Dirt.Tint);
        Preset->Dirt.bUseTexture = GetBoolField(*DirtObject, TEXT("useTexture"), Preset->Dirt.bUseTexture);
        Preset->Dirt.bInvertTexture = GetBoolField(*DirtObject, TEXT("invertTexture"), Preset->Dirt.bInvertTexture);
        Preset->Dirt.bInvertTexture = GetBoolField(*DirtObject, TEXT("textureInvert"), Preset->Dirt.bInvertTexture);
        FString DirtTexturePath;
        if ((*DirtObject)->TryGetStringField(TEXT("texture"), DirtTexturePath)
            || (*DirtObject)->TryGetStringField(TEXT("damageTexture"), DirtTexturePath)
            || (*DirtObject)->TryGetStringField(TEXT("texturePath"), DirtTexturePath))
        {
            const FSoftObjectPath SoftPath(DirtTexturePath);
            if (SoftPath.IsValid())
            {
                Preset->Dirt.DamageTexture = TSoftObjectPtr<UTexture2D>(SoftPath);
            }
        }
        Preset->Dirt.TextureTiling = GetNumberField(*DirtObject, TEXT("textureTiling"), Preset->Dirt.TextureTiling);
        Preset->Dirt.TextureTiling = GetNumberField(*DirtObject, TEXT("tiling"), Preset->Dirt.TextureTiling);
        Preset->Dirt.TextureScaleMin = GetNumberField(*DirtObject, TEXT("textureScaleMin"), Preset->Dirt.TextureScaleMin);
        Preset->Dirt.TextureScaleMax = GetNumberField(*DirtObject, TEXT("textureScaleMax"), Preset->Dirt.TextureScaleMax);
        Preset->Dirt.NoiseScale = GetNumberField(*DirtObject, TEXT("noiseScale"), Preset->Dirt.NoiseScale);
        Preset->Dirt.NoiseStrength = GetNumberField(*DirtObject, TEXT("noiseStrength"), Preset->Dirt.NoiseStrength);
        Preset->Dirt.NoiseSpeed = GetNumberField(*DirtObject, TEXT("noiseSpeed"), Preset->Dirt.NoiseSpeed);
        Preset->Dirt.Intensity = FMath::Clamp(Preset->Dirt.Intensity, 0.0f, 1.0f);
        Preset->Dirt.Density = FMath::Clamp(Preset->Dirt.Density, 0.0f, 1.0f);
        Preset->Dirt.Size = FMath::Clamp(Preset->Dirt.Size, 2.0f, 2000.0f);
        Preset->Dirt.SizeJitter = FMath::Clamp(Preset->Dirt.SizeJitter, 0.0f, 1.0f);
        Preset->Dirt.OpacityJitter = FMath::Clamp(Preset->Dirt.OpacityJitter, 0.0f, 1.0f);
        Preset->Dirt.Softness = FMath::Clamp(Preset->Dirt.Softness, 0.0f, 1.0f);
        Preset->Dirt.Frequency = FMath::Clamp(Preset->Dirt.Frequency, 0.0f, 12.0f);
        Preset->Dirt.Polarity = FMath::Clamp(Preset->Dirt.Polarity, -1.0f, 1.0f);
        Preset->Dirt.TextureTiling = FMath::Clamp(Preset->Dirt.TextureTiling, 0.1f, 8.0f);
        Preset->Dirt.TextureScaleMin = FMath::Clamp(Preset->Dirt.TextureScaleMin, 0.1f, 8.0f);
        Preset->Dirt.TextureScaleMax = FMath::Clamp(Preset->Dirt.TextureScaleMax, 0.1f, 8.0f);
        Preset->Dirt.NoiseScale = FMath::Clamp(Preset->Dirt.NoiseScale, 0.1f, 12.0f);
        Preset->Dirt.NoiseStrength = FMath::Clamp(Preset->Dirt.NoiseStrength, 0.0f, 1.0f);
        Preset->Dirt.NoiseSpeed = FMath::Clamp(Preset->Dirt.NoiseSpeed, 0.0f, 5.0f);
    }

    const TSharedPtr<FJsonObject>* PrintObject = nullptr;
    if (PresetObject->TryGetObjectField(TEXT("print"), PrintObject) && PrintObject && PrintObject->IsValid())
    {
        FString PrintLutObjectPath;
        if ((*PrintObject)->TryGetStringField(TEXT("lut"), PrintLutObjectPath) || (*PrintObject)->TryGetStringField(TEXT("lutAsset"), PrintLutObjectPath))
        {
            if (IsLikelyFilePath(PrintLutObjectPath))
            {
                Preset->FilmPrintLUTPath.FilePath = PrintLutObjectPath;
            }
            else
            {
                const FSoftObjectPath SoftPath(PrintLutObjectPath);
                if (SoftPath.IsValid())
                {
                    Preset->FilmPrintLUTAsset = TSoftObjectPtr<UFilmEmulatorLUT>(SoftPath);
                }
            }
        }

        FString PrintLutTexturePathObj;
        if ((*PrintObject)->TryGetStringField(TEXT("lutTexture"), PrintLutTexturePathObj) || (*PrintObject)->TryGetStringField(TEXT("lut2d"), PrintLutTexturePathObj))
        {
            const FSoftObjectPath SoftPath(PrintLutTexturePathObj);
            if (SoftPath.IsValid())
            {
                Preset->FilmPrintLUT = TSoftObjectPtr<UTexture2D>(SoftPath);
            }
            else if (IsLikelyFilePath(PrintLutTexturePathObj))
            {
                Preset->FilmPrintLUTPath.FilePath = PrintLutTexturePathObj;
            }
        }

        FString PrintLutObjectFile;
        if ((*PrintObject)->TryGetStringField(TEXT("lutPath"), PrintLutObjectFile) || (*PrintObject)->TryGetStringField(TEXT("lutFile"), PrintLutObjectFile))
        {
            Preset->FilmPrintLUTPath.FilePath = PrintLutObjectFile;
        }

        Preset->PrintStrength = GetNumberField(*PrintObject, TEXT("strength"), Preset->PrintStrength);
        Preset->PrintStrength = GetNumberField(*PrintObject, TEXT("intensity"), Preset->PrintStrength);
    }

    OutEntry.PresetId = PresetId;
    OutEntry.SourcePath = SourcePath;
    OutEntry.Preset = TStrongObjectPtr<UFilmStockPreset>(Preset);
    return true;
}
} // namespace

FFilmEmulatorPresetLibrary& FFilmEmulatorPresetLibrary::Get()
{
    static FFilmEmulatorPresetLibrary Instance;
    return Instance;
}

void FFilmEmulatorPresetLibrary::Reload()
{
    bLoaded = false;
    LoadPresets();
}

const TArray<FFilmPresetEntry>& FFilmEmulatorPresetLibrary::GetPresets()
{
    LoadIfNeeded();
    return Presets;
}

UFilmStockPreset* FFilmEmulatorPresetLibrary::FindPresetById(FName PresetId)
{
    LoadIfNeeded();

    if (PresetId.IsNone())
    {
        return nullptr;
    }

    for (const FFilmPresetEntry& Entry : Presets)
    {
        if (Entry.PresetId == PresetId)
        {
            return Entry.Preset.Get();
        }
    }

    return nullptr;
}

UFilmStockPreset* FFilmEmulatorPresetLibrary::GetDefaultPreset()
{
    LoadIfNeeded();

    if (!LoadedDefaultPresetId.IsNone())
    {
        if (UFilmStockPreset* Preset = FindPresetById(LoadedDefaultPresetId))
        {
            return Preset;
        }
    }

    return nullptr;
}

void FFilmEmulatorPresetLibrary::LoadIfNeeded()
{
    if (!bLoaded)
    {
        LoadPresets();
    }
}

void FFilmEmulatorPresetLibrary::LoadPresets()
{
    bLoaded = true;
    Presets.Reset();
    LoadedDefaultPresetId = NAME_None;

    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FilmEmulator"));
    if (!Plugin)
    {
        UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("FilmEmulator plugin not found while loading presets."));
        return;
    }

    const FString PresetsDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content"), TEXT("Presets"));
    if (!FPaths::DirectoryExists(PresetsDir))
    {
        UE_LOG(LogFilmEmulatorPresets, Verbose, TEXT("Presets directory not found: %s"), *PresetsDir);
        return;
    }

    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *PresetsDir, TEXT("*.json"), true, false);
    Files.Sort();

    if (Files.Num() == 0)
    {
        UE_LOG(LogFilmEmulatorPresets, Verbose, TEXT("No preset JSON files found in %s"), *PresetsDir);
        return;
    }

    TSet<FName> SeenIds;

    for (const FString& FilePath : Files)
    {
        FString JsonText;
        if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
        {
            UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Failed to read preset file: %s"), *FilePath);
            continue;
        }

        TSharedPtr<FJsonObject> RootObject;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Failed to parse preset JSON: %s"), *FilePath);
            continue;
        }

        FString DefaultIdString;
        if (RootObject->TryGetStringField(TEXT("defaultPresetId"), DefaultIdString))
        {
            if (!DefaultIdString.IsEmpty())
            {
                if (!LoadedDefaultPresetId.IsNone() && LoadedDefaultPresetId != FName(*DefaultIdString))
                {
                    UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Multiple defaultPresetId values found. Using '%s' (found in %s)."),
                        *DefaultIdString, *FilePath);
                }
                LoadedDefaultPresetId = FName(*DefaultIdString);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* PresetsArray = nullptr;
        if (RootObject->TryGetArrayField(TEXT("presets"), PresetsArray) && PresetsArray)
        {
            for (const TSharedPtr<FJsonValue>& Value : *PresetsArray)
            {
                const TSharedPtr<FJsonObject> PresetObject = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!PresetObject.IsValid())
                {
                    continue;
                }

                FFilmPresetEntry Entry;
                if (ParsePresetObject(PresetObject, FilePath, Entry))
                {
                    if (SeenIds.Contains(Entry.PresetId))
                    {
                        UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Duplicate preset id '%s' in %s. Skipping."), *Entry.PresetId.ToString(), *FilePath);
                        continue;
                    }

                    SeenIds.Add(Entry.PresetId);
                    Presets.Add(MoveTemp(Entry));
                }
            }
        }
        else
        {
            FFilmPresetEntry Entry;
            if (ParsePresetObject(RootObject, FilePath, Entry))
            {
                if (!SeenIds.Contains(Entry.PresetId))
                {
                    SeenIds.Add(Entry.PresetId);
                    Presets.Add(MoveTemp(Entry));
                }
                else
                {
                    UE_LOG(LogFilmEmulatorPresets, Warning, TEXT("Duplicate preset id '%s' in %s. Skipping."), *Entry.PresetId.ToString(), *FilePath);
                }
            }
        }
    }

    UE_LOG(LogFilmEmulatorPresets, Log, TEXT("Loaded %d film preset(s) from %s"), Presets.Num(), *PresetsDir);
}





