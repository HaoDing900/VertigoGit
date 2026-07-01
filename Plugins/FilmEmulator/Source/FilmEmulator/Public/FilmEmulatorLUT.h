// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FilmEmulatorLUT.generated.h"

class UAssetImportData;
class UVolumeTexture;

UCLASS(BlueprintType)
class FILMEMULATOR_API UFilmEmulatorLUT : public UDataAsset
{
    GENERATED_BODY()

public:
    UFilmEmulatorLUT();

#if WITH_EDITORONLY_DATA
    virtual void PostInitProperties() override;
    virtual void PostLoad() override;
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Film LUT")
    int32 Size = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Film LUT")
    FVector3f DomainMin = FVector3f(0.0f, 0.0f, 0.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Film LUT")
    FVector3f DomainMax = FVector3f(1.0f, 1.0f, 1.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Film LUT", meta = (AdvancedDisplay, HideInDetailPanel))
    TArray<FVector3f> Samples;

    UVolumeTexture* GetOrCreateVolumeTexture();

    void SetFromSamples(int32 InSize, const TArray<FVector3f>& InSamples, const FVector3f& InDomainMin, const FVector3f& InDomainMax);

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
    TObjectPtr<UAssetImportData> AssetImportData;
#endif

#if WITH_EDITOR
    // UE 5.3 signature (the FAssetRegistryTagsContext overload arrived in 5.4).
    virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif

private:
    UPROPERTY(Transient)
    TObjectPtr<UVolumeTexture> CachedVolumeTexture;
};

