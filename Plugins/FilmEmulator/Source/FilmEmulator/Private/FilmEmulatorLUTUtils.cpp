// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorLUTUtils.h"

#include "Engine/VolumeTexture.h"
#include "TextureResource.h"
#include "Math/Float16Color.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogFilmEmulatorLUT, Log, All);

namespace FilmEmulatorLUTUtils
{
UVolumeTexture* CreateVolumeLUTTexture(int32 Size, const TArray<FVector3f>& Samples, const FString& SourceLabel)
{
    const int32 ExpectedSamples = Size * Size * Size;
    if (Size <= 0 || Samples.Num() != ExpectedSamples)
    {
        UE_LOG(LogFilmEmulatorLUT, Warning, TEXT("Invalid LUT data for %s (size %d, samples %d)"), *SourceLabel, Size, Samples.Num());
        return nullptr;
    }

    UVolumeTexture* Texture = UVolumeTexture::CreateTransient(Size, Size, Size, PF_FloatRGBA);
    if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        UE_LOG(LogFilmEmulatorLUT, Warning, TEXT("Failed to create volume LUT for %s"), *SourceLabel);
        return nullptr;
    }

    Texture->SRGB = false;
    Texture->CompressionSettings = TC_HDR;
    #if WITH_EDITORONLY_DATA
    Texture->MipGenSettings = TMGS_NoMipmaps;
    #endif
    Texture->Filter = TF_Bilinear;
    Texture->AddressMode = TA_Clamp;
    Texture->LODGroup = TEXTUREGROUP_ColorLookupTable;
    Texture->NeverStream = true;

    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    FFloat16Color* MipData = reinterpret_cast<FFloat16Color*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
    if (!MipData)
    {
        Mip.BulkData.Unlock();
        UE_LOG(LogFilmEmulatorLUT, Warning, TEXT("Failed to lock volume LUT for %s"), *SourceLabel);
        return nullptr;
    }

    const int32 SliceSize = Size * Size;
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        const int32 R = Index % Size;
        const int32 G = (Index / Size) % Size;
        const int32 B = Index / SliceSize;
        const int32 VoxelIndex = R + G * Size + B * SliceSize;

        FVector3f Value = Samples[Index];
        Value.X = FMath::Max(0.0f, Value.X);
        Value.Y = FMath::Max(0.0f, Value.Y);
        Value.Z = FMath::Max(0.0f, Value.Z);

        MipData[VoxelIndex] = FFloat16Color(FLinearColor(Value.X, Value.Y, Value.Z, 1.0f));
    }

    Mip.BulkData.Unlock();
    Texture->UpdateResource();
    return Texture;
}

bool ParseCubeFile(const FString& ResolvedPath, int32& OutSize, TArray<FVector3f>& OutSamples, FVector3f& OutDomainMin, FVector3f& OutDomainMax)
{
    OutSize = 0;
    OutSamples.Reset();
    OutDomainMin = FVector3f(0.0f, 0.0f, 0.0f);
    OutDomainMax = FVector3f(1.0f, 1.0f, 1.0f);

    FString FileContents;
    if (!FFileHelper::LoadFileToString(FileContents, *ResolvedPath))
    {
        UE_LOG(LogFilmEmulatorLUT, Warning, TEXT("Failed to read LUT file: %s"), *ResolvedPath);
        return false;
    }

    TArray<FString> Lines;
    FileContents.ParseIntoArrayLines(Lines, true);

    for (const FString& RawLine : Lines)
    {
        FString Line = RawLine.TrimStartAndEnd();
        if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("//")))
        {
            continue;
        }

        if (Line.StartsWith(TEXT("TITLE"), ESearchCase::IgnoreCase))
        {
            continue;
        }

        if (Line.StartsWith(TEXT("LUT_3D_SIZE"), ESearchCase::IgnoreCase))
        {
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() >= 2)
            {
                OutSize = FCString::Atoi(*Tokens[1]);
            }
            continue;
        }

        if (Line.StartsWith(TEXT("DOMAIN_MIN"), ESearchCase::IgnoreCase))
        {
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() >= 4)
            {
                OutDomainMin = FVector3f(FCString::Atof(*Tokens[1]), FCString::Atof(*Tokens[2]), FCString::Atof(*Tokens[3]));
            }
            continue;
        }

        if (Line.StartsWith(TEXT("DOMAIN_MAX"), ESearchCase::IgnoreCase))
        {
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() >= 4)
            {
                OutDomainMax = FVector3f(FCString::Atof(*Tokens[1]), FCString::Atof(*Tokens[2]), FCString::Atof(*Tokens[3]));
            }
            continue;
        }

        TArray<FString> Tokens;
        Line.ParseIntoArrayWS(Tokens);
        if (Tokens.Num() >= 3)
        {
            const float R = FCString::Atof(*Tokens[0]);
            const float G = FCString::Atof(*Tokens[1]);
            const float B = FCString::Atof(*Tokens[2]);
            OutSamples.Add(FVector3f(R, G, B));
        }
    }

    if (OutSize <= 0)
    {
        UE_LOG(LogFilmEmulatorLUT, Warning, TEXT("Invalid LUT size in %s"), *ResolvedPath);
        return false;
    }

    const int32 ExpectedSamples = OutSize * OutSize * OutSize;
    if (OutSamples.Num() != ExpectedSamples)
    {
        UE_LOG(LogFilmEmulatorLUT, Warning, TEXT("LUT sample count mismatch in %s (expected %d, got %d)"), *ResolvedPath, ExpectedSamples, OutSamples.Num());
        return false;
    }

    return true;
}
} // namespace FilmEmulatorLUTUtils

