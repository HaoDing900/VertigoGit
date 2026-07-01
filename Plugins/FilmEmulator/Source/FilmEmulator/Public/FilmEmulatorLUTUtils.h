// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UVolumeTexture;

namespace FilmEmulatorLUTUtils
{
    FILMEMULATOR_API bool ParseCubeFile(
        const FString& ResolvedPath,
        int32& OutSize,
        TArray<FVector3f>& OutSamples,
        FVector3f& OutDomainMin,
        FVector3f& OutDomainMax);

    FILMEMULATOR_API UVolumeTexture* CreateVolumeLUTTexture(
        int32 Size,
        const TArray<FVector3f>& Samples,
        const FString& SourceLabel);
}
