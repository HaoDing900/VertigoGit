// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorLUT.h"

#include "FilmEmulatorLUTUtils.h"
#include "Engine/VolumeTexture.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

#if WITH_EDITOR
#include "UObject/Package.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogFilmEmulatorLUTAsset, Log, All);

UFilmEmulatorLUT::UFilmEmulatorLUT()
{
#if WITH_EDITORONLY_DATA
    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
    }
#endif
}

void UFilmEmulatorLUT::SetFromSamples(int32 InSize, const TArray<FVector3f>& InSamples, const FVector3f& InDomainMin, const FVector3f& InDomainMax)
{
    Size = InSize;
    Samples = InSamples;
    DomainMin = InDomainMin;
    DomainMax = InDomainMax;
    CachedVolumeTexture = nullptr;
}

UVolumeTexture* UFilmEmulatorLUT::GetOrCreateVolumeTexture()
{
    if (CachedVolumeTexture)
    {
        return CachedVolumeTexture;
    }

    if (Size <= 0 || Samples.Num() != Size * Size * Size)
    {
        UE_LOG(LogFilmEmulatorLUTAsset, Warning, TEXT("Invalid LUT asset data for %s (size %d, samples %d)"), *GetPathName(), Size, Samples.Num());
        return nullptr;
    }

    CachedVolumeTexture = FilmEmulatorLUTUtils::CreateVolumeLUTTexture(Size, Samples, GetPathName());
    return CachedVolumeTexture;
}

#if WITH_EDITORONLY_DATA
void UFilmEmulatorLUT::PostInitProperties()
{
    Super::PostInitProperties();

    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        if (!AssetImportData)
        {
            AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
        }
        else if (AssetImportData->GetOuter() != this)
        {
            AssetImportData = DuplicateObject<UAssetImportData>(AssetImportData, this);
        }
    }
}

void UFilmEmulatorLUT::PostLoad()
{
    Super::PostLoad();

    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        if (!AssetImportData)
        {
            AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
        }
        else if (AssetImportData->GetOuter() != this)
        {
            AssetImportData = DuplicateObject<UAssetImportData>(AssetImportData, this);
        }
    }
}
#endif

#if WITH_EDITOR
// UE 5.3 signature: the FAssetRegistryTagsContext overload arrived in 5.4.
void UFilmEmulatorLUT::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
    Super::GetAssetRegistryTags(OutTags);
    if (AssetImportData)
    {
        AssetImportData->AppendAssetRegistryTags(OutTags);
    }
}
#endif
