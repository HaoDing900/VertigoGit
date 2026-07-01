// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "FilmEmulatorColorProfiles.generated.h"

class UTexture2D;
class UFilmEmulatorLUT;

UENUM(BlueprintType)
enum class EFilmEmulatorFilmType : uint8
{
    ColorNegative    UMETA(DisplayName = "Color Negative"),
    ColorSlide       UMETA(DisplayName = "Color Positive (Slide)"),
    BWNegative       UMETA(DisplayName = "Black & White Negative"),
    BWPositive       UMETA(DisplayName = "Black & White Print")
};

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmColorProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile")
    FName ProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile")
    EFilmEmulatorFilmType FilmType = EFilmEmulatorFilmType::ColorNegative;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile", meta = (DisplayName = "Film LUT (3D)"))
    TSoftObjectPtr<UFilmEmulatorLUT> FilmLUTAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile", meta = (DisplayName = "Film LUT (Legacy 2D)", AdvancedDisplay))
    TSoftObjectPtr<UTexture2D> FilmLUT;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile", meta = (FilePathFilter = "png;bmp;exr;hdr;cube", AdvancedDisplay))
    FFilePath FilmLUTPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float SaturationBias = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ContrastBias = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Color Profile")
    float ExposureBias = 0.0f;
};

USTRUCT(BlueprintType)
struct FILMEMULATOR_API FFilmPrintProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print Profile")
    FName ProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print Profile")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print Profile", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print Profile", meta = (DisplayName = "Print LUT (3D)"))
    TSoftObjectPtr<UFilmEmulatorLUT> PrintLUTAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print Profile", meta = (DisplayName = "Print LUT (Legacy 2D)", AdvancedDisplay))
    TSoftObjectPtr<UTexture2D> PrintLUT;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Film Print Profile", meta = (FilePathFilter = "png;bmp;exr;hdr;cube", AdvancedDisplay))
    FFilePath PrintLUTPath;
};

inline const TCHAR* LexToString(EFilmEmulatorFilmType Type)
{
    switch (Type)
    {
    case EFilmEmulatorFilmType::ColorNegative: return TEXT("Color Negative");
    case EFilmEmulatorFilmType::ColorSlide:    return TEXT("Color Slide");
    case EFilmEmulatorFilmType::BWNegative:    return TEXT("Black & White Negative");
    case EFilmEmulatorFilmType::BWPositive:    return TEXT("Black & White Print");
    default: return TEXT("Unknown");
    }
}
