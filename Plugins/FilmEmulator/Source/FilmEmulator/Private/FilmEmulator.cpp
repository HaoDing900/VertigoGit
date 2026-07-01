// Copyright 2026 TOXIC STOCK All rights reserved.
#include "FilmEmulator.h"
#include "FilmEmulatorPCH.h"
#include "FilmEmulatorSettings.h"
#include "FilmEmulatorGlobalActor.h"
#include "FilmEmulatorPresetLibrary.h"
#include "FilmStockPreset.h"
#include "FilmEmulatorLUT.h"
#include "FilmEmulatorLUTUtils.h"
#include "FilmEmulatorColorizePass.h"
#include "FilmEmulatorGrainPass.h"
#include "FilmEmulatorScratchPass.h"
#include "FilmEmulatorDirtPass.h"
#include "FilmEmulatorHalationPass.h"
#include "FilmEmulatorPrintPass.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Texture2D.h"
#include "Engine/VolumeTexture.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "ScreenPass.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "SceneTextureParameters.h"
#include "EngineUtils.h"
#include "ScenePrivate.h"
#include "Math/Float16Color.h"
#include "Math/RandomStream.h"
DEFINE_LOG_CATEGORY_STATIC(LogFilmEmulator, Log, All);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorColorizePS, "/Plugin/FilmEmulator/FilmEmulatorColor.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorGrainPS, "/Plugin/FilmEmulator/FilmEmulatorGrain.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorScratchPS, "/Plugin/FilmEmulator/FilmEmulatorScratch.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorDirtPS, "/Plugin/FilmEmulator/FilmEmulatorDirt.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorHalationExtractPS, "/Plugin/FilmEmulator/FilmEmulatorHalation.usf", "HalationExtractPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorHalationDownsamplePS, "/Plugin/FilmEmulator/FilmEmulatorHalation.usf", "HalationDownsamplePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorHalationBlurPS, "/Plugin/FilmEmulator/FilmEmulatorHalation.usf", "HalationBlurPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorHalationPS, "/Plugin/FilmEmulator/FilmEmulatorHalation.usf", "HalationCompositePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFilmEmulatorPrintPS, "/Plugin/FilmEmulator/FilmEmulatorPrint.usf", "MainPS", SF_Pixel);
IMPLEMENT_MODULE(FFilmEmulatorModule, FilmEmulator)
namespace
{
static TAutoConsoleVariable<int32> CVarFilmEmulator(TEXT("r.FilmEmulator"), -1, TEXT("Master toggle: -1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorHalation(TEXT("r.FilmEmulator_Halation"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorGrain(TEXT("r.FilmEmulator_Grain"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorFlicker(TEXT("r.FilmEmulator_Flicker"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorGateWeave(TEXT("r.FilmEmulator_GateWeave"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorGateScratch(TEXT("r.FilmEmulator_GateScratch"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorDirt(TEXT("r.FilmEmulator_Dirt"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarFilmEmulatorPrint(TEXT("r.FilmEmulator_Print"), -1, TEXT("-1=use preset, 0=off, 1=on"), ECVF_RenderThreadSafe);
bool ShouldApplyFilmEmulatorToView(const FSceneView& View)
{
    return !View.bIsSceneCapture && !View.bIsSceneCaptureCube && !View.bIsReflectionCapture && !View.bIsPlanarReflection;
}
FString ResolveLUTPath(const FString& InPath)
{
    if (InPath.IsEmpty())
    {
        return FString();
    }
    FString ResolvedPath = InPath;
    if (FPaths::IsRelative(ResolvedPath))
    {
        if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FilmEmulator")))
        {
            ResolvedPath = FPaths::Combine(Plugin->GetBaseDir(), ResolvedPath);
        }
    }
    return FPaths::ConvertRelativePathToFull(ResolvedPath);
}
UVolumeTexture* CreateVolumeLUTTexture(int32 Size, const TArray<FVector3f>& Samples, const FString& SourceLabel)
{
    return FilmEmulatorLUTUtils::CreateVolumeLUTTexture(Size, Samples, SourceLabel);
}
bool ExtractLUTSamplesFromTexture2D(UTexture2D* Texture, int32& OutSize, TArray<FVector3f>& OutSamples)
{
    OutSize = 0;
    OutSamples.Reset();
    if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        return false;
    }
    const int32 Width = Texture->GetPlatformData()->SizeX;
    const int32 Height = Texture->GetPlatformData()->SizeY;
    if (Width <= 0 || Height <= 0 || Width != Height * Height)
    {
        UE_LOG(LogFilmEmulator, Warning, TEXT("Unsupported LUT texture layout (%dx%d)"), Width, Height);
        return false;
    }
    const int32 Size = Height;
    const int32 TotalSamples = Size * Size * Size;
    OutSamples.SetNumZeroed(TotalSamples);
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    const EPixelFormat Format = Texture->GetPlatformData()->PixelFormat;
    void* MipData = Mip.BulkData.Lock(LOCK_READ_ONLY);
    if (!MipData)
    {
        Mip.BulkData.Unlock();
        return false;
    }
    if (Format == PF_FloatRGBA)
    {
        const FFloat16Color* Src = reinterpret_cast<const FFloat16Color*>(MipData);
        for (int32 B = 0; B < Size; ++B)
        {
            for (int32 G = 0; G < Size; ++G)
            {
                for (int32 R = 0; R < Size; ++R)
                {
                    const int32 X = R + B * Size;
                    const int32 Y = G;
                    const int32 SrcIndex = Y * Width + X;
                    const int32 DstIndex = R + G * Size + B * Size * Size;
                    const FFloat16Color Sample = Src[SrcIndex];
                    OutSamples[DstIndex] = FVector3f(Sample.R.GetFloat(), Sample.G.GetFloat(), Sample.B.GetFloat());
                }
            }
        }
    }
    else if (Format == PF_B8G8R8A8)
    {
        const FColor* Src = reinterpret_cast<const FColor*>(MipData);
        const float Inv255 = 1.0f / 255.0f;
        const bool bSRGB = Texture->SRGB;
        for (int32 B = 0; B < Size; ++B)
        {
            for (int32 G = 0; G < Size; ++G)
            {
                for (int32 R = 0; R < Size; ++R)
                {
                    const int32 X = R + B * Size;
                    const int32 Y = G;
                    const int32 SrcIndex = Y * Width + X;
                    const int32 DstIndex = R + G * Size + B * Size * Size;
                    const FColor Sample = Src[SrcIndex];
                    if (bSRGB)
                    {
                        const FLinearColor Linear = FLinearColor::FromSRGBColor(Sample);
                        OutSamples[DstIndex] = FVector3f(Linear.R, Linear.G, Linear.B);
                    }
                    else
                    {
                        OutSamples[DstIndex] = FVector3f(Sample.R * Inv255, Sample.G * Inv255, Sample.B * Inv255);
                    }
                }
            }
        }
    }
    else
    {
        UE_LOG(LogFilmEmulator, Warning, TEXT("Unsupported LUT texture format %d"), static_cast<int32>(Format));
        Mip.BulkData.Unlock();
        return false;
    }
    Mip.BulkData.Unlock();
    OutSize = Size;
    return true;
}
UVolumeTexture* LoadRasterLUTTexture(const FString& ResolvedPath)
{
    UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(ResolvedPath);
    if (!Texture)
    {
        return nullptr;
    }
    Texture->SRGB = false;
    Texture->CompressionSettings = TC_HDR;
    #if WITH_EDITORONLY_DATA
    Texture->MipGenSettings = TMGS_NoMipmaps;
    #endif
    Texture->Filter = TF_Bilinear;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;
    Texture->LODGroup = TEXTUREGROUP_ColorLookupTable;
    Texture->NeverStream = true;
    Texture->UpdateResource();
    int32 Size = 0;
    TArray<FVector3f> Samples;
    if (!ExtractLUTSamplesFromTexture2D(Texture, Size, Samples))
    {
        UE_LOG(LogFilmEmulator, Warning, TEXT("Failed to read LUT texture %s"), *ResolvedPath);
        return nullptr;
    }
    return CreateVolumeLUTTexture(Size, Samples, ResolvedPath);
}
bool ParseCubeFile(const FString& ResolvedPath, int32& OutSize, TArray<FVector3f>& OutSamples, FVector3f& OutDomainMin, FVector3f& OutDomainMax)
{
    return FilmEmulatorLUTUtils::ParseCubeFile(ResolvedPath, OutSize, OutSamples, OutDomainMin, OutDomainMax);
}
UVolumeTexture* LoadCubeLUTTexture(const FString& ResolvedPath)
{
    int32 Size = 0;
    TArray<FVector3f> Samples;
    FVector3f DomainMin;
    FVector3f DomainMax;
    if (!ParseCubeFile(ResolvedPath, Size, Samples, DomainMin, DomainMax))
    {
        return nullptr;
    }
    if (!DomainMin.Equals(FVector3f(0.0f, 0.0f, 0.0f), 1e-4f) || !DomainMax.Equals(FVector3f(1.0f, 1.0f, 1.0f), 1e-4f))
    {
        UE_LOG(LogFilmEmulator, Warning, TEXT("LUT domain is not [0..1] in %s (min %.3f %.3f %.3f, max %.3f %.3f %.3f). Input will be clamped."), *ResolvedPath,
            DomainMin.X, DomainMin.Y, DomainMin.Z, DomainMax.X, DomainMax.Y, DomainMax.Z);
    }
    return CreateVolumeLUTTexture(Size, Samples, ResolvedPath);
}
UVolumeTexture* LoadLUTTextureFromFile(const FString& InPath)
{
    const FString ResolvedPath = ResolveLUTPath(InPath);
    if (ResolvedPath.IsEmpty() || !FPaths::FileExists(ResolvedPath))
    {
        return nullptr;
    }
    const FString Extension = FPaths::GetExtension(ResolvedPath).ToLower();
    if (Extension == TEXT("cube"))
    {
        return LoadCubeLUTTexture(ResolvedPath);
    }
    return LoadRasterLUTTexture(ResolvedPath);
}
UVolumeTexture* ConvertLUTTexture2D(UTexture2D* Texture)
{
    if (!Texture)
    {
        return nullptr;
    }
    int32 Size = 0;
    TArray<FVector3f> Samples;
    if (!ExtractLUTSamplesFromTexture2D(Texture, Size, Samples))
    {
        UE_LOG(LogFilmEmulator, Warning, TEXT("Failed to read LUT texture %s"), *Texture->GetPathName());
        return nullptr;
    }
    return CreateVolumeLUTTexture(Size, Samples, Texture->GetPathName());
}
FString BuildProfileLUTKey(const FFilmColorProfile* Profile)
{
    if (!Profile)
    {
        return FString();
    }
    if (Profile->FilmLUTAsset.ToSoftObjectPath().IsValid())
    {
        return Profile->FilmLUTAsset.ToSoftObjectPath().ToString();
    }
    if (Profile->FilmLUT.ToSoftObjectPath().IsValid())
    {
        return Profile->FilmLUT.ToSoftObjectPath().ToString();
    }
    if (!Profile->FilmLUTPath.FilePath.IsEmpty())
    {
        return ResolveLUTPath(Profile->FilmLUTPath.FilePath);
    }
    return FString();
}
FString BuildPresetLUTKey(const UFilmStockPreset* Preset)
{
    if (!Preset)
    {
        return FString();
    }
    if (Preset->FilmLUTAsset.ToSoftObjectPath().IsValid())
    {
        return Preset->FilmLUTAsset.ToSoftObjectPath().ToString();
    }
    if (Preset->FilmLUT.ToSoftObjectPath().IsValid())
    {
        return Preset->FilmLUT.ToSoftObjectPath().ToString();
    }
    if (!Preset->FilmLUTPath.FilePath.IsEmpty())
    {
        return ResolveLUTPath(Preset->FilmLUTPath.FilePath);
    }
    return FString();
}
FString BuildPrintProfileLUTKey(const FFilmPrintProfile* Profile)
{
    if (!Profile)
    {
        return FString();
    }
    if (Profile->PrintLUTAsset.ToSoftObjectPath().IsValid())
    {
        return Profile->PrintLUTAsset.ToSoftObjectPath().ToString();
    }
    if (Profile->PrintLUT.ToSoftObjectPath().IsValid())
    {
        return Profile->PrintLUT.ToSoftObjectPath().ToString();
    }
    if (!Profile->PrintLUTPath.FilePath.IsEmpty())
    {
        return ResolveLUTPath(Profile->PrintLUTPath.FilePath);
    }
    return FString();
}
FString BuildPresetPrintLUTKey(const UFilmStockPreset* Preset)
{
    if (!Preset)
    {
        return FString();
    }
    if (Preset->FilmPrintLUTAsset.ToSoftObjectPath().IsValid())
    {
        return Preset->FilmPrintLUTAsset.ToSoftObjectPath().ToString();
    }
    if (Preset->FilmPrintLUT.ToSoftObjectPath().IsValid())
    {
        return Preset->FilmPrintLUT.ToSoftObjectPath().ToString();
    }
    if (!Preset->FilmPrintLUTPath.FilePath.IsEmpty())
    {
        return ResolveLUTPath(Preset->FilmPrintLUTPath.FilePath);
    }
    return FString();
}
FString BuildSelectionKey(const UFilmStockPreset* Preset, const FFilmColorProfile* Profile)
{
    if (Preset)
    {
        return Preset->GetPathName();
    }
    if (Profile)
    {
        return FString::Printf(TEXT("Profile:%s"), *Profile->ProfileId.ToString());
    }
    return FString();
}
static bool IsCineFormat(EFilmEmulatorFilmFormat Format)
{
    switch (Format)
    {
    case EFilmEmulatorFilmFormat::Cine35:
    case EFilmEmulatorFilmFormat::Cine35Academy:
    case EFilmEmulatorFilmFormat::Cine35Techniscope:
    case EFilmEmulatorFilmFormat::Cine16:
    case EFilmEmulatorFilmFormat::Cine16Super:
    case EFilmEmulatorFilmFormat::Cine8:
    case EFilmEmulatorFilmFormat::Cine8Standard:
    case EFilmEmulatorFilmFormat::Cine65:
    case EFilmEmulatorFilmFormat::Cine70:
    case EFilmEmulatorFilmFormat::Cinerama:
    case EFilmEmulatorFilmFormat::Kinetoscope:
        return true;
    default:
        return false;
    }
}
static bool GetFilmFormatGateMm(EFilmEmulatorFilmFormat Format, float& OutWidth, float& OutHeight)
{
    switch (Format)
    {
    case EFilmEmulatorFilmFormat::Photo35:
        OutWidth = 36.0f;
        OutHeight = 24.0f;
        return true;
    case EFilmEmulatorFilmFormat::Cine35:
        OutWidth = 24.92f;
        OutHeight = 18.67f;
        return true;
    case EFilmEmulatorFilmFormat::Cine35Academy:
        OutWidth = 22.0f;
        OutHeight = 16.0f;
        return true;
    case EFilmEmulatorFilmFormat::Cine35Techniscope:
        OutWidth = 22.0f;
        OutHeight = 9.47f;
        return true;
    case EFilmEmulatorFilmFormat::Cine16:
        OutWidth = 10.26f;
        OutHeight = 7.49f;
        return true;
    case EFilmEmulatorFilmFormat::Cine16Super:
        OutWidth = 12.52f;
        OutHeight = 7.41f;
        return true;
    case EFilmEmulatorFilmFormat::Cine8:
        OutWidth = 5.46f;
        OutHeight = 4.01f;
        return true;
    case EFilmEmulatorFilmFormat::Cine8Standard:
        OutWidth = 4.8f;
        OutHeight = 3.5f;
        return true;
    case EFilmEmulatorFilmFormat::Cine65:
        OutWidth = 52.63f;
        OutHeight = 23.01f;
        return true;
    case EFilmEmulatorFilmFormat::Cine70:
        OutWidth = 48.56f;
        OutHeight = 22.10f;
        return true;
    case EFilmEmulatorFilmFormat::Cinerama:
        OutWidth = 25.2984f * 3.0f;
        OutHeight = 28.3464f;
        return true;
    case EFilmEmulatorFilmFormat::Kinetoscope:
        OutWidth = 25.40f;
        OutHeight = 19.05f;
        return true;
    default:
        return false;
    }
}
float GetLegacyFilmFormatScale(EFilmEmulatorFilmFormat Format)
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
float GetPhysicalFilmFormatScale(EFilmEmulatorFilmFormat Format)
{
    const float RefArea = 36.0f * 24.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    if (GetFilmFormatGateMm(Format, Width, Height))
    {
        const float Area = FMath::Max(Width * Height, KINDA_SMALL_NUMBER);
        return FMath::Sqrt(RefArea / Area);
    }
    return GetLegacyFilmFormatScale(Format);
}
float ResolveFilmFormatScale(const UFilmStockPreset* Preset)
{
    if (!Preset)
    {
        return 1.0f;
    }
    const float LegacyScale = GetLegacyFilmFormatScale(Preset->FilmFormat);
    float FormatScale = Preset->FilmFormatScale;
    if (IsCineFormat(Preset->FilmFormat))
    {
        const float PhysicalScale = GetPhysicalFilmFormatScale(Preset->FilmFormat);
        const float RelativeScale = (LegacyScale > KINDA_SMALL_NUMBER)
            ? (FormatScale / LegacyScale)
            : 1.0f;
        FormatScale = PhysicalScale * RelativeScale;
    }
    return FMath::Clamp(FormatScale, 0.25f, 10.0f);
}
float ComputeIsoScale(const FFilmGrainSettings& GrainSettings, float RefIso, EFilmEmulatorFilmFormat Format)
{
    const float SafeIso = FMath::Max(GrainSettings.ISO, 1.0f);
    const float SafeRefIso = FMath::Max(RefIso, 1.0f);
    const bool bIsCine = IsCineFormat(Format);
    if (!bIsCine)
    {
        return FMath::Clamp(FMath::Sqrt(SafeIso / SafeRefIso), 0.25f, 4.0f);
    }
    const float CineIsoMin = 16.0f;
    const float CineIsoMax = 500.0f;
    const float ClampedIso = FMath::Clamp(SafeIso, CineIsoMin, CineIsoMax);
    const float ClampedRef = FMath::Clamp(SafeRefIso, CineIsoMin, CineIsoMax);
    const float IsoStops = FMath::Log2(ClampedIso / ClampedRef);
    const float IsoScale = FMath::Pow(2.0f, IsoStops * 0.8f);
    return FMath::Clamp(IsoScale, 0.35f, 6.0f);
}
float ComputeIsoCoverage(const FFilmGrainSettings& GrainSettings, float RefIso, EFilmEmulatorFilmFormat Format)
{
    const float SafeIso = FMath::Max(GrainSettings.ISO, 1.0f);
    const float SafeRefIso = FMath::Max(RefIso, 1.0f);
    const bool bIsCine = IsCineFormat(Format);
    const float IsoMin = bIsCine ? 16.0f : 25.0f;
    const float IsoMax = bIsCine ? 500.0f : 3200.0f;
    const float ClampedIso = FMath::Clamp(SafeIso, IsoMin, IsoMax);
    const float ClampedRef = FMath::Clamp(SafeRefIso, IsoMin, IsoMax);
    const float IsoStops = FMath::Log2(ClampedIso / ClampedRef);
    return FMath::Clamp(IsoStops / 2.0f, 0.0f, 1.0f);
}
UVolumeTexture* LoadPresetLUTTexture(UFilmStockPreset* Preset)
{
    if (!Preset)
    {
        return nullptr;
    }
    if (Preset->FilmLUTAsset.IsValid())
    {
        return Preset->FilmLUTAsset->GetOrCreateVolumeTexture();
    }
    if (Preset->FilmLUTAsset.ToSoftObjectPath().IsValid())
    {
        if (UFilmEmulatorLUT* Asset = Preset->FilmLUTAsset.LoadSynchronous())
        {
            return Asset->GetOrCreateVolumeTexture();
        }
    }
    if (Preset->FilmLUT.IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Preset->FilmLUT.Get()))
        {
            return Volume;
        }
    }
    if (Preset->FilmLUT.ToSoftObjectPath().IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Preset->FilmLUT.LoadSynchronous()))
        {
            return Volume;
        }
    }
    if (!Preset->FilmLUTPath.FilePath.IsEmpty())
    {
        return LoadLUTTextureFromFile(Preset->FilmLUTPath.FilePath);
    }
    return nullptr;
}
UVolumeTexture* LoadPresetPrintLUTTexture(UFilmStockPreset* Preset)
{
    if (!Preset)
    {
        return nullptr;
    }
    if (Preset->FilmPrintLUTAsset.IsValid())
    {
        return Preset->FilmPrintLUTAsset->GetOrCreateVolumeTexture();
    }
    if (Preset->FilmPrintLUTAsset.ToSoftObjectPath().IsValid())
    {
        if (UFilmEmulatorLUT* Asset = Preset->FilmPrintLUTAsset.LoadSynchronous())
        {
            return Asset->GetOrCreateVolumeTexture();
        }
    }
    if (Preset->FilmPrintLUT.IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Preset->FilmPrintLUT.Get()))
        {
            return Volume;
        }
    }
    if (Preset->FilmPrintLUT.ToSoftObjectPath().IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Preset->FilmPrintLUT.LoadSynchronous()))
        {
            return Volume;
        }
    }
    if (!Preset->FilmPrintLUTPath.FilePath.IsEmpty())
    {
        return LoadLUTTextureFromFile(Preset->FilmPrintLUTPath.FilePath);
    }
    return nullptr;
}
UVolumeTexture* LoadPrintProfileLUTTexture(const FFilmPrintProfile* Profile)
{
    if (!Profile)
    {
        return nullptr;
    }
    if (Profile->PrintLUTAsset.IsValid())
    {
        return Profile->PrintLUTAsset->GetOrCreateVolumeTexture();
    }
    if (Profile->PrintLUTAsset.ToSoftObjectPath().IsValid())
    {
        if (UFilmEmulatorLUT* Asset = Profile->PrintLUTAsset.LoadSynchronous())
        {
            return Asset->GetOrCreateVolumeTexture();
        }
    }
    if (Profile->PrintLUT.IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Profile->PrintLUT.Get()))
        {
            return Volume;
        }
    }
    if (Profile->PrintLUT.ToSoftObjectPath().IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Profile->PrintLUT.LoadSynchronous()))
        {
            return Volume;
        }
    }
    if (!Profile->PrintLUTPath.FilePath.IsEmpty())
    {
        return LoadLUTTextureFromFile(Profile->PrintLUTPath.FilePath);
    }
    return nullptr;
}
UVolumeTexture* LoadProfileLUTTexture(const FFilmColorProfile* Profile)
{
    if (!Profile)
    {
        return nullptr;
    }
    if (Profile->FilmLUTAsset.IsValid())
    {
        return Profile->FilmLUTAsset->GetOrCreateVolumeTexture();
    }
    if (Profile->FilmLUTAsset.ToSoftObjectPath().IsValid())
    {
        if (UFilmEmulatorLUT* Asset = Profile->FilmLUTAsset.LoadSynchronous())
        {
            return Asset->GetOrCreateVolumeTexture();
        }
    }
    if (Profile->FilmLUT.IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Profile->FilmLUT.Get()))
        {
            return Volume;
        }
    }
    if (Profile->FilmLUT.ToSoftObjectPath().IsValid())
    {
        if (UVolumeTexture* Volume = ConvertLUTTexture2D(Profile->FilmLUT.LoadSynchronous()))
        {
            return Volume;
        }
    }
    if (!Profile->FilmLUTPath.FilePath.IsEmpty())
    {
        return LoadLUTTextureFromFile(Profile->FilmLUTPath.FilePath);
    }
    return nullptr;
}
} // namespace
FFilmEmulatorViewExtension::FFilmEmulatorViewExtension(const FAutoRegister& AutoRegister)
    : FSceneViewExtensionBase(AutoRegister)
{
    FRandomStream Stream(static_cast<int32>(FPlatformTime::Cycles()));
    GateWeaveSeedX = Stream.FRandRange(0.0f, 1000.0f);
    GateWeaveSeedY = Stream.FRandRange(0.0f, 1000.0f);
    GateWeaveSeedZ = Stream.FRandRange(0.0f, 1000.0f);
    FlickerSeedA = Stream.FRandRange(0.0f, 1000.0f);
    FlickerSeedB = Stream.FRandRange(0.0f, 1000.0f);
    ScratchSeedA = Stream.FRandRange(0.0f, 1000.0f);
    ScratchSeedB = Stream.FRandRange(0.0f, 1000.0f);
    DirtSeedA = Stream.FRandRange(0.0f, 1000.0f);
    DirtSeedB = Stream.FRandRange(0.0f, 1000.0f);
}
void FFilmEmulatorViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
    (void)InViewFamily;
}
void FFilmEmulatorViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
    const UFilmEmulatorSettings* Settings = GetDefault<UFilmEmulatorSettings>();
    if (!Settings)
    {
        return;
    }
    FFilmEmulatorParams Params = Settings->GetDefaultParams();
    if (const FSceneInterface* Scene = InViewFamily.Scene)
    {
        if (UWorld* World = Scene->GetWorld())
        {
            if (AFilmEmulatorGlobalActor* GlobalActor = AFilmEmulatorGlobalActor::FindHighPriorityActor(World))
            {
                Params = GlobalActor->Params;
            }
        }
    }
    if (!Params.bEnableFilmEmulation)
    {
        return;
    }
}

void FFilmEmulatorViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
    if (!InViewFamily.EngineShowFlags.PostProcessing)
    {
        return;
    }
    const UFilmEmulatorSettings* Settings = GetDefault<UFilmEmulatorSettings>();
    if (!Settings)
    {
        return;
    }
    bool bApplyAfterTonemap = Settings->bApplyAfterTonemapper;
    FFilmEmulatorParams NewParams = Settings->GetDefaultParams();
    UFilmStockPreset* Preset = nullptr;
    if (Settings->DefaultPreset.IsValid())
    {
        Preset = Settings->DefaultPreset.Get();
    }
    else if (Settings->DefaultPreset.ToSoftObjectPath().IsValid())
    {
        Preset = Settings->DefaultPreset.LoadSynchronous();
    }
    else if (Settings->DefaultPresetId != NAME_None)
    {
        Preset = FFilmEmulatorPresetLibrary::Get().FindPresetById(Settings->DefaultPresetId);
    }
    if (!Preset)
    {
        Preset = FFilmEmulatorPresetLibrary::Get().GetDefaultPreset();
    }
    if (const FSceneInterface* Scene = InViewFamily.Scene)
    {
        if (UWorld* World = Scene->GetWorld())
        {
            if (AFilmEmulatorGlobalActor* GlobalActor = AFilmEmulatorGlobalActor::FindHighPriorityActor(World))
            {
                bApplyAfterTonemap = GlobalActor->bApplyAfterTonemapper;
                NewParams = GlobalActor->Params;
                Preset = GlobalActor->GetEffectivePreset();
            }
        }
    }

    const FFilmColorProfile* FilmProfile = nullptr;
    if (NewParams.FilmProfileId != NAME_None)
    {
        FilmProfile = Settings->FindFilmProfile(NewParams.FilmProfileId);
    }
    if (!FilmProfile)
    {
        FilmProfile = &Settings->GetDefaultFilmProfile();
    }
    const FFilmPrintProfile* PrintProfile = nullptr;
    if (NewParams.FilmPrintProfileId != NAME_None)
    {
        PrintProfile = Settings->FindFilmPrintProfile(NewParams.FilmPrintProfileId);
    }
    if (!PrintProfile)
    {
        PrintProfile = &Settings->GetDefaultFilmPrintProfile();
    }
    float SaturationBias = 1.0f;
    float ContrastBias = 1.0f;
    float ExposureBias = 0.0f;
    FFilmGrainSettings GrainSettings;
    GrainSettings.bEnabled = false;
    FFilmHalationSettings HalationSettings;
    bool bFilmIsBW = false;
    FFilmGateWeaveSettings GateWeaveSettings;
    FFilmFlickerSettings FlickerSettings;
    FFilmGateScratchSettings ScratchSettings;
    FFilmDirtSettings DirtSettings;
    HalationSettings.bEnabled = false;
    GateWeaveSettings.bEnabled = false;
    FlickerSettings.bEnabled = false;
    ScratchSettings.bEnabled = false;
    DirtSettings.bEnabled = false;
    float GrainIsoCoverage = 0.0f;
    float PrintStrength = NewParams.bEnableFilmPrint ? NewParams.PrintStrength : 0.0f;
    float PrintExposureEV = NewParams.PrintExposureEV;
    EFilmEmulatorFilmType FilmType = FilmProfile ? FilmProfile->FilmType : EFilmEmulatorFilmType::ColorNegative;
    if (Preset)
    {
        FilmType = Preset->FilmType;
    }
    bFilmIsBW = (FilmType == EFilmEmulatorFilmType::BWNegative || FilmType == EFilmEmulatorFilmType::BWPositive);
    FString SelectionKey;
    FString LutKey;
    FString PrintKey;
    bool bUsePresetLUT = false;
    bool bUsePresetPrintLUT = false;
    if (Preset)
    {
        SelectionKey = BuildSelectionKey(Preset, nullptr);
        SaturationBias = Preset->SaturationBias;
        ContrastBias = Preset->ContrastBias;
        ExposureBias = Preset->ExposureBias;
        GrainSettings = Preset->Grain;
        HalationSettings = Preset->Halation;
        GateWeaveSettings = Preset->GateWeave;
        FlickerSettings = Preset->Flicker;
        ScratchSettings = Preset->GateScratch;
        DirtSettings = Preset->Dirt;
        const float FormatScale = ResolveFilmFormatScale(Preset);
        GrainSettings.FormatScale = FormatScale;
        GateWeaveSettings.FormatScale = FormatScale;
        ScratchSettings.FormatScale = FormatScale;
        DirtSettings.FormatScale = FormatScale;
        if (ScratchSettings.bAutoPolarity)
        {
            const bool bNegative = (FilmType == EFilmEmulatorFilmType::ColorNegative || FilmType == EFilmEmulatorFilmType::BWNegative);
            ScratchSettings.Polarity = bNegative ? -1.0f : 1.0f;
        }
        if (DirtSettings.bAutoPolarity)
        {
            const bool bNegative = (FilmType == EFilmEmulatorFilmType::ColorNegative || FilmType == EFilmEmulatorFilmType::BWNegative);
            DirtSettings.Polarity = bNegative ? -1.0f : 1.0f;
        }
        if (bFilmIsBW)
        {
            const float L = ScratchSettings.Tint.GetLuminance();
            ScratchSettings.Tint = FLinearColor(L, L, L, ScratchSettings.Tint.A);
            const float DL = DirtSettings.Tint.GetLuminance();
            DirtSettings.Tint = FLinearColor(DL, DL, DL, DirtSettings.Tint.A);
        }
        if (IsCineFormat(Preset->FilmFormat))
        {
            HalationSettings.Radius *= FormatScale;
        }
        const float RefIso = (Preset->GrainDefaults.ISO > 0.0f) ? Preset->GrainDefaults.ISO : GrainSettings.ISO;
        const float IsoScale = ComputeIsoScale(GrainSettings, RefIso, Preset->FilmFormat);
        GrainSettings.Size *= IsoScale;
        GrainIsoCoverage = ComputeIsoCoverage(GrainSettings, RefIso, Preset->FilmFormat);
        const FString PresetLutKey = BuildPresetLUTKey(Preset);
        if (!PresetLutKey.IsEmpty())
        {
            bUsePresetLUT = true;
            LutKey = PresetLutKey;
        }
        const FString PresetPrintKey = BuildPresetPrintLUTKey(Preset);
        if (!PresetPrintKey.IsEmpty())
        {
            bUsePresetPrintLUT = true;
            PrintKey = PresetPrintKey;
            PrintStrength *= Preset->PrintStrength;
        }
    }
    if (!bUsePresetLUT && FilmProfile)
    {
        if (SelectionKey.IsEmpty())
        {
            SelectionKey = BuildSelectionKey(nullptr, FilmProfile);
        }
        SaturationBias = FilmProfile->SaturationBias;
        ContrastBias = FilmProfile->ContrastBias;
        ExposureBias = FilmProfile->ExposureBias;
        LutKey = BuildProfileLUTKey(FilmProfile);
    }
    if (!bUsePresetPrintLUT && PrintProfile)
    {
        PrintKey = BuildPrintProfileLUTKey(PrintProfile);
    }
    if (!NewParams.bEnableFilmPrint || PrintStrength <= 0.001f)
    {
        PrintKey.Reset();
    }
    UVolumeTexture* FilmLUT = nullptr;
    if (!LutKey.IsEmpty())
    {
        if (LutKey == CachedLUTSourcePath && CachedLUTTexture.IsValid())
        {
            FilmLUT = CachedLUTTexture.Get();
        }
        else
        {
            FilmLUT = bUsePresetLUT ? LoadPresetLUTTexture(Preset) : LoadProfileLUTTexture(FilmProfile);
        }
    }
    UVolumeTexture* PrintLUT = nullptr;
    UTexture2D* DirtTexture = nullptr;
    if (!PrintKey.IsEmpty())
    {
        if (PrintKey == CachedPrintLUTSourcePath && CachedPrintLUTTexture.IsValid())
        {
            PrintLUT = CachedPrintLUTTexture.Get();
        }
        else
        {
            PrintLUT = bUsePresetPrintLUT ? LoadPresetPrintLUTTexture(Preset) : LoadPrintProfileLUTTexture(PrintProfile);
        }
    }
    if (Preset && DirtSettings.bUseTexture)
    {
        if (Preset->Dirt.DamageTexture.IsValid())
        {
            DirtTexture = Preset->Dirt.DamageTexture.Get();
        }
        else if (Preset->Dirt.DamageTexture.ToSoftObjectPath().IsValid())
        {
            DirtTexture = Preset->Dirt.DamageTexture.LoadSynchronous();
        }
    }
    {
    const int32 MasterOverride = CVarFilmEmulator.GetValueOnAnyThread();
    if (MasterOverride >= 0)
    {
        NewParams.bEnableFilmEmulation = (MasterOverride != 0);
    }
    auto ApplyBoolOverride = [](int32 Value, bool& bEnabled)
    {
        if (Value >= 0)
        {
            bEnabled = (Value != 0);
        }
    };
    ApplyBoolOverride(CVarFilmEmulatorHalation.GetValueOnAnyThread(), HalationSettings.bEnabled);
    ApplyBoolOverride(CVarFilmEmulatorGrain.GetValueOnAnyThread(), GrainSettings.bEnabled);
    ApplyBoolOverride(CVarFilmEmulatorFlicker.GetValueOnAnyThread(), FlickerSettings.bEnabled);
    ApplyBoolOverride(CVarFilmEmulatorGateWeave.GetValueOnAnyThread(), GateWeaveSettings.bEnabled);
    ApplyBoolOverride(CVarFilmEmulatorGateScratch.GetValueOnAnyThread(), ScratchSettings.bEnabled);
    ApplyBoolOverride(CVarFilmEmulatorDirt.GetValueOnAnyThread(), DirtSettings.bEnabled);
    const int32 PrintOverride = CVarFilmEmulatorPrint.GetValueOnAnyThread();
    if (PrintOverride >= 0)
    {
        NewParams.bEnableFilmPrint = (PrintOverride != 0);
        if (!NewParams.bEnableFilmPrint)
        {
            PrintStrength = 0.0f;
        }
    }
    if (!NewParams.bEnableFilmEmulation)
    {
        HalationSettings.bEnabled = false;
        GrainSettings.bEnabled = false;
        FlickerSettings.bEnabled = false;
        GateWeaveSettings.bEnabled = false;
        ScratchSettings.bEnabled = false;
        DirtSettings.bEnabled = false;
        NewParams.bEnableFilmPrint = false;
        PrintStrength = 0.0f;
    }
        FScopeLock Lock(&ParamsMutex);
        CachedParams = NewParams;
        CachedGrain = GrainSettings;
        CachedGrainIsoCoverage = GrainIsoCoverage;
        CachedHalation = HalationSettings;
        CachedFilmIsBW = bFilmIsBW;
        CachedGateWeave = GateWeaveSettings;
        CachedFlicker = FlickerSettings;
        CachedGateScratch = ScratchSettings;
        CachedDirt = DirtSettings;
        CachedSaturationBias = SaturationBias;
        CachedContrastBias = ContrastBias;
        CachedExposureBias = ExposureBias;
        CachedPrintStrength = PrintStrength;
        CachedPrintExposureEV = PrintExposureEV;
        CachedApplyAfterTonemap = bApplyAfterTonemap;
        CachedPresetPath = SelectionKey;
        CachedLUTSourcePath = LutKey;
        CachedPrintLUTSourcePath = PrintKey;
        CachedLUTTexture.Reset(FilmLUT);
        CachedPrintLUTTexture.Reset(PrintLUT);
        CachedDirtTexture.Reset(DirtTexture);
    }
}
void FFilmEmulatorViewExtension::PrePostProcessPass_RenderThread(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessingInputs& Inputs)
{
    // UE 5.3 has no always-on, pre-tonemap entry in EPostProcessingPass (the 5.6 build hooked
    // BeforeDOF/AfterDOF, which don't exist here). MotionBlur -- the only pre-tonemap pass that
    // affects the visible image -- is skipped whenever motion blur is off, which is the case in
    // editor viewports and in PIE without motion blur, so subscribing there meant the effect was
    // never inserted. PrePostProcessPass runs unconditionally right before post processing, in
    // linear pre-tonemap scene-color space, which is where the 5.6 build applied the effect.
    if (!ShouldApplyFilmEmulatorToView(View))
    {
        return;
    }
    {
        FScopeLock Lock(&ParamsMutex);
        if (!CachedParams.bEnableFilmEmulation)
        {
            return;
        }
    }
    if (!Inputs.SceneTextures)
    {
        return;
    }
    FRDGTextureRef SceneColorTexture = (*Inputs.SceneTextures)->SceneColorTexture;
    if (!SceneColorTexture)
    {
        return;
    }
    check(View.bIsViewInfo);
    const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
    const FScreenPassTexture SceneColor(SceneColorTexture, ViewInfo.ViewRect);
    const FScreenPassTexture Result = ApplyFilmEmulationChain_RenderThread(
        GraphBuilder, View, SceneColor, FScreenPassRenderTarget());
    // The chain renders into a fresh full-extent texture; copy this view's rect back into scene
    // color so the rest of post processing (DOF, bloom, tonemap) reads the emulated result.
    if (Result.IsValid() && Result.Texture && Result.Texture != SceneColorTexture)
    {
        FRHICopyTextureInfo CopyInfo;
        CopyInfo.Size = FIntVector(ViewInfo.ViewRect.Width(), ViewInfo.ViewRect.Height(), 1);
        CopyInfo.SourcePosition = FIntVector(ViewInfo.ViewRect.Min.X, ViewInfo.ViewRect.Min.Y, 0);
        CopyInfo.DestPosition = FIntVector(ViewInfo.ViewRect.Min.X, ViewInfo.ViewRect.Min.Y, 0);
        AddCopyTexturePass(GraphBuilder, Result.Texture, SceneColorTexture, CopyInfo);
    }
}
FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmEmulationChain_RenderThread(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    const FScreenPassRenderTarget& OverrideOutput)
{
    if (!SceneColor.IsValid())
    {
        return SceneColor;
    }
    FScreenPassRenderTarget Output = OverrideOutput;
    FFilmEmulatorParams ParamsCopy;
    FFilmGrainSettings GrainSettings;
    FFilmHalationSettings HalationSettings;
    bool bFilmIsBW = false;
    FFilmGateWeaveSettings GateWeaveSettings;
    FFilmFlickerSettings FlickerSettings;
    FFilmGateScratchSettings ScratchSettings;
    FFilmDirtSettings DirtSettings;
    float GrainIsoCoverage = 0.0f;
    float SaturationBias = 1.0f;
    float ContrastBias = 1.0f;
    float ExposureBias = 0.0f;
    UVolumeTexture* FilmLUT = nullptr;
    UVolumeTexture* PrintLUT = nullptr;
    UTexture2D* DirtTexture = nullptr;
    float PrintStrength = 0.0f;
    float PrintExposureEV = 0.0f;
    bool bApplyAfterTonemap = false;
    {
        FScopeLock Lock(&ParamsMutex);
        ParamsCopy = CachedParams;
        GrainSettings = CachedGrain;
        HalationSettings = CachedHalation;
        bFilmIsBW = CachedFilmIsBW;
        GateWeaveSettings = CachedGateWeave;
        FlickerSettings = CachedFlicker;
        ScratchSettings = CachedGateScratch;
        DirtSettings = CachedDirt;
        GrainIsoCoverage = CachedGrainIsoCoverage;
        SaturationBias = CachedSaturationBias;
        ContrastBias = CachedContrastBias;
        ExposureBias = CachedExposureBias;
        FilmLUT = CachedLUTTexture.Get();
        PrintLUT = CachedPrintLUTTexture.Get();
        DirtTexture = CachedDirtTexture.Get();
        PrintStrength = CachedPrintStrength;
        PrintExposureEV = CachedPrintExposureEV;
        bApplyAfterTonemap = CachedApplyAfterTonemap;
        bApplyAfterTonemap = false;
    }
    check(View.bIsViewInfo);
    const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
    const float PreExposure = bApplyAfterTonemap ? 1.0f : ViewInfo.PreExposure;
    if (!ParamsCopy.bEnableFilmEmulation)
    {
        return SceneColor;
    }
    const FIntPoint ViewSize = SceneColor.ViewRect.Size();
    const float TimeSeconds = View.Family ? static_cast<float>(View.Family->Time.GetWorldTimeSeconds()) : static_cast<float>(FApp::GetCurrentTime());
    float FlickerEV = 0.0f;
    if (FlickerSettings.bEnabled && FlickerSettings.Intensity > 0.0f && FlickerSettings.Frequency > 0.0f)
    {
        const float t = TimeSeconds * FlickerSettings.Frequency;
        const float n0 = FMath::PerlinNoise1D(t + FlickerSeedA);
        const float n1 = FMath::PerlinNoise1D(t * 2.37f + FlickerSeedB);
        FlickerEV = FlickerSettings.Intensity * (0.7f * n0 + 0.3f * n1);
    }

    FVector2f GateWeaveOffsetUV(0.0f, 0.0f);
    if (GateWeaveSettings.bEnabled && GateWeaveSettings.Amplitude > 0.0f && GateWeaveSettings.Frequency > 0.0f
        && ViewSize.X > 0 && ViewSize.Y > 0)
    {
        const float t = TimeSeconds * GateWeaveSettings.Frequency;
        const float nX = FMath::PerlinNoise1D(t + GateWeaveSeedX);
        const float nY = FMath::PerlinNoise1D(t * 1.31f + GateWeaveSeedY);
        const float nZ = FMath::PerlinNoise1D(t * 0.77f + GateWeaveSeedZ);
        const float pixelsPerMm = (static_cast<float>(ViewSize.X) * GateWeaveSettings.FormatScale) / 36.0f;
        const float amplitudePx = GateWeaveSettings.Amplitude * pixelsPerMm;
        const FVector2f offsetPx = FVector2f(nX + 0.35f * nZ, nY - 0.25f * nZ) * amplitudePx;
        GateWeaveOffsetUV = FVector2f(offsetPx.X / static_cast<float>(ViewSize.X), offsetPx.Y / static_cast<float>(ViewSize.Y));
    }

    const bool bHasFilmLUT = FilmLUT && FilmLUT->GetResource();
    const bool bHasPrintLUT = PrintLUT && PrintLUT->GetResource();
    const bool bApplyPrint = ParamsCopy.bEnableFilmPrint && bHasPrintLUT && PrintStrength > 0.001f;
    if (bHasFilmLUT)
    {
        return ApplyFilmEmulationPass(
            GraphBuilder,
            View,
            SceneColor,
            ParamsCopy,
            SaturationBias,
            ContrastBias,
            ExposureBias,
            HalationSettings,
            bFilmIsBW,
            GrainSettings,
            ScratchSettings,
            DirtSettings,
            GrainIsoCoverage,
            GateWeaveOffsetUV,
            FlickerEV,
            TimeSeconds,
            DirtTexture,
            FilmLUT,
            PrintLUT,
            PrintStrength,
            PrintExposureEV,
            PreExposure,
            bApplyAfterTonemap,
            Output);
    }
    FScreenPassTexture CurrentColor = SceneColor;
    const bool bApplyHalation = HalationSettings.bEnabled && HalationSettings.Intensity > 0.0f && bHasPrintLUT;
    const bool bApplyGrain = GrainSettings.bEnabled && GrainSettings.Intensity > 0.0f;
    const bool bApplyScratch = ScratchSettings.bEnabled && ScratchSettings.Intensity > 0.0f && ScratchSettings.Density > 0.0f;
    const bool bApplyDirt = DirtSettings.bEnabled && DirtSettings.Intensity > 0.0f && DirtSettings.Density > 0.0f;
    if (bApplyHalation)
    {
        FScreenPassRenderTarget HalationOutput = Output;
        if (!HalationOutput.IsValid() || bApplyGrain || bApplyScratch || bApplyDirt)
        {
            HalationOutput = FScreenPassRenderTarget();
        }
        CurrentColor = ApplyFilmHalationPass(
            GraphBuilder,
            View,
            CurrentColor,
            HalationSettings,
            bFilmIsBW,
            PrintLUT,
            bApplyPrint ? PrintStrength : 0.0f,
            bApplyPrint ? PrintExposureEV : 0.0f,
            PreExposure,
            HalationOutput);
    }
    else if (bApplyPrint)
    {
        FScreenPassRenderTarget PrintOutput = Output;
        if (!PrintOutput.IsValid() || bApplyGrain || bApplyScratch || bApplyDirt)
        {
            PrintOutput = FScreenPassRenderTarget();
        }
        CurrentColor = ApplyFilmPrintPass(
            GraphBuilder,
            View,
            CurrentColor,
            PrintLUT,
            PrintStrength,
            PrintExposureEV,
            PreExposure,
            PrintOutput);
    }
    if (bApplyGrain)
    {
        FScreenPassRenderTarget GrainOutput = Output;
        if (!GrainOutput.IsValid() || bApplyScratch || bApplyDirt)
        {
            GrainOutput = FScreenPassRenderTarget();
        }
        CurrentColor = ApplyFilmGrainPass(
            GraphBuilder,
            View,
            CurrentColor,
            GrainSettings,
            GrainIsoCoverage,
            GateWeaveOffsetUV,
            PreExposure,
            GrainOutput);
    }
    if (bApplyScratch)
    {
        FScreenPassRenderTarget ScratchOutput = Output;
        if (!ScratchOutput.IsValid() || bApplyDirt)
        {
            ScratchOutput = FScreenPassRenderTarget();
        }
        CurrentColor = ApplyFilmScratchPass(
            GraphBuilder,
            View,
            CurrentColor,
            ScratchSettings,
            GateWeaveOffsetUV,
            TimeSeconds,
            PreExposure,
            ScratchOutput);
    }
    if (bApplyDirt)
    {
        FScreenPassRenderTarget DirtOutput = Output;
        if (!DirtOutput.IsValid())
        {
            DirtOutput = FScreenPassRenderTarget();
        }
        CurrentColor = ApplyFilmDirtPass(
            GraphBuilder,
            View,
            CurrentColor,
            DirtSettings,
            DirtTexture,
            GateWeaveOffsetUV,
            TimeSeconds,
            PreExposure,
            DirtOutput);
    }

    return CurrentColor;
}
FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmEmulationPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    const FFilmEmulatorParams& ParamsCopy,
    float SaturationBias,
    float ContrastBias,
    float ExposureBias,
    const FFilmHalationSettings& HalationSettings,
    bool bFilmIsBW,
    const FFilmGrainSettings& GrainSettings,
    const FFilmGateScratchSettings& ScratchSettings,
    const FFilmDirtSettings& DirtSettings,
    float GrainIsoCoverage,
    const FVector2f& GateWeaveOffsetUV,
    float FlickerEV,
    float ScratchTime,
    UTexture2D* DirtTexture,
    UVolumeTexture* FilmLUT,
    UVolumeTexture* FilmPrintLUT,
    float PrintStrength,
    float PrintExposureEV,
    float PreExposure,
    bool bApplyAfterTonemap,
    const FScreenPassRenderTarget& OverrideOutput)
{
    FRDGTextureRef SceneColorTexture = SceneColor.Texture;
    if (!SceneColorTexture)
    {
        return SceneColor;
    }
    const FScreenPassTextureViewport SceneViewport(SceneColor);
    const bool bApplyHalation = HalationSettings.bEnabled && HalationSettings.Intensity > 0.0f;
    const bool bApplyPrint = ParamsCopy.bEnableFilmPrint
        && FilmPrintLUT
        && FilmPrintLUT->GetResource()
        && PrintStrength > 0.001f;
    const bool bApplyGrain = GrainSettings.bEnabled && GrainSettings.Intensity > 0.0f;
    const bool bApplyScratch = ScratchSettings.bEnabled && ScratchSettings.Intensity > 0.0f && ScratchSettings.Density > 0.0f;
    const bool bApplyDirt = DirtSettings.bEnabled && DirtSettings.Intensity > 0.0f && DirtSettings.Density > 0.0f;
    const bool bPrintInHalation = bApplyPrint && bApplyHalation;
    const bool bPrintInColor = bApplyPrint && !bApplyHalation;
    UVolumeTexture* SafePrintLUT = FilmLUT;
    if (FilmPrintLUT && FilmPrintLUT->GetResource())
    {
        SafePrintLUT = FilmPrintLUT;
    }
    const float ColorPrintStrength = bPrintInColor ? PrintStrength : 0.0f;
    const float ColorPrintExposure = bPrintInColor ? PrintExposureEV : 0.0f;
    const float HalationPrintStrength = bPrintInHalation ? PrintStrength : 0.0f;
    const float HalationPrintExposure = bPrintInHalation ? PrintExposureEV : 0.0f;
    FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
    OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
    FScreenPassRenderTarget ColorOutput = OverrideOutput;
    if (!ColorOutput.IsValid() || bApplyHalation || bApplyGrain || bApplyScratch || bApplyDirt)
    {
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.ColorOutput"));
        ColorOutput = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
    }
    const float Saturation = ParamsCopy.Saturation * SaturationBias;
    const float Contrast = ParamsCopy.Contrast * ContrastBias;
    const float Exposure = ParamsCopy.ExposureEV + ExposureBias;
    FFilmEmulatorColorizePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorColorizePS::FParameters>();
    PassParameters->SceneColorTexture = SceneColorTexture;
    PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
    PassParameters->FilmLUT = FilmLUT->GetResource()->TextureRHI;
    PassParameters->FilmLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->ColorParams = FVector4f(ParamsCopy.Strength, Exposure, Saturation, Contrast);
    PassParameters->GateWeaveOffset = FVector2f(GateWeaveOffsetUV);
    PassParameters->FlickerEV = FlickerEV;
    PassParameters->FilmPrintLUT = SafePrintLUT->GetResource()->TextureRHI;
    PassParameters->FilmPrintLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->PrintStrength = ColorPrintStrength;
    PassParameters->PrintExposureEV = ColorPrintExposure;
    PassParameters->PreExposure = PreExposure;
    PassParameters->ApplyAfterTonemap = bApplyAfterTonemap ? 1.0f : 0.0f;
    PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));
    PassParameters->RenderTargets[0] = ColorOutput.GetRenderTargetBinding();
    TShaderMapRef<FFilmEmulatorColorizePS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    AddDrawScreenPass(
        GraphBuilder,
        RDG_EVENT_NAME("FilmEmulatorColorize"),
        View,
        SceneViewport,
        SceneViewport,
        PixelShader,
        PassParameters);
    FScreenPassTexture CurrentColor = MoveTemp(ColorOutput);
    if (bApplyHalation)
    {
        FScreenPassRenderTarget HalationOutput = OverrideOutput;
        if (!HalationOutput.IsValid() || bApplyGrain || bApplyScratch || bApplyDirt)
        {
            FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.HalationOutput"));
            HalationOutput = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
        }
        CurrentColor = ApplyFilmHalationPass(
            GraphBuilder,
            View,
            CurrentColor,
            HalationSettings,
            bFilmIsBW,
            SafePrintLUT,
            HalationPrintStrength,
            HalationPrintExposure,
            PreExposure,
            HalationOutput);
    }
    if (bApplyGrain)
    {
        FScreenPassRenderTarget GrainOutput = OverrideOutput;
        if (!GrainOutput.IsValid() || bApplyScratch || bApplyDirt)
        {
            FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.GrainOutput"));
            GrainOutput = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
        }
        CurrentColor = ApplyFilmGrainPass(
            GraphBuilder,
            View,
            CurrentColor,
            GrainSettings,
            GrainIsoCoverage,
            GateWeaveOffsetUV,
            PreExposure,
            GrainOutput);
    }
    if (bApplyScratch)
    {
        FScreenPassRenderTarget ScratchOutput = OverrideOutput;
        if (!ScratchOutput.IsValid() || bApplyDirt)
        {
            FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.ScratchOutput"));
            ScratchOutput = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
        }
        CurrentColor = ApplyFilmScratchPass(
            GraphBuilder,
            View,
            CurrentColor,
            ScratchSettings,
            GateWeaveOffsetUV,
            ScratchTime,
            PreExposure,
            ScratchOutput);
    }

    if (bApplyDirt)
    {
        FScreenPassRenderTarget DirtOutput = OverrideOutput;
        if (!DirtOutput.IsValid())
        {
            FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.DirtOutput"));
            DirtOutput = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
        }
        CurrentColor = ApplyFilmDirtPass(
            GraphBuilder,
            View,
            CurrentColor,
            DirtSettings,
            DirtTexture,
            GateWeaveOffsetUV,
            ScratchTime,
            PreExposure,
            DirtOutput);
    }

    return CurrentColor;
}
FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmHalationPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    const FFilmHalationSettings& HalationSettings,
    bool bFilmIsBW,
    UVolumeTexture* FilmPrintLUT,
    float PrintStrength,
    float PrintExposureEV,
    float PreExposure,
    const FScreenPassRenderTarget& OverrideOutput)
{
    FRDGTextureRef SceneColorTexture = SceneColor.Texture;
    if (!SceneColorTexture)
    {
        return SceneColor;
    }
    const FScreenPassTextureViewport SceneViewport(SceneColor);
    FScreenPassRenderTarget Output = OverrideOutput;
    if (!Output.IsValid())
    {
        FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
        OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.HalationOutput"));
        Output = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
    }
    const FIntPoint SceneSize = SceneViewport.Rect.Size();
    auto CalcHalationSize = [&SceneSize](int32 Divisor)
    {
        return FIntPoint(
            FMath::Max(SceneSize.X / Divisor, 8),
            FMath::Max(SceneSize.Y / Divisor, 8));
    };
    const FIntPoint HalationSize0 = CalcHalationSize(2);
    const FIntPoint HalationSize1 = CalcHalationSize(4);
    const FIntPoint HalationSize2 = CalcHalationSize(8);
    const FIntRect HalationRect0(FIntPoint::ZeroValue, HalationSize0);
    const FIntRect HalationRect1(FIntPoint::ZeroValue, HalationSize1);
    const FIntRect HalationRect2(FIntPoint::ZeroValue, HalationSize2);
    FRDGTextureDesc MaskDesc0 = FRDGTextureDesc::Create2D(
        HalationSize0,
        PF_R16F,
        FClearValueBinding::Black,
        TexCreate_ShaderResource | TexCreate_RenderTargetable);
    FRDGTextureDesc MaskDesc1 = FRDGTextureDesc::Create2D(
        HalationSize1,
        PF_R16F,
        FClearValueBinding::Black,
        TexCreate_ShaderResource | TexCreate_RenderTargetable);
    FRDGTextureDesc MaskDesc2 = FRDGTextureDesc::Create2D(
        HalationSize2,
        PF_R16F,
        FClearValueBinding::Black,
        TexCreate_ShaderResource | TexCreate_RenderTargetable);
    FRDGTextureRef HalationMask0 = GraphBuilder.CreateTexture(MaskDesc0, TEXT("FilmEmulator.HalationMask0"));
    FRDGTextureRef HalationBlur0H = GraphBuilder.CreateTexture(MaskDesc0, TEXT("FilmEmulator.HalationBlur0H"));
    FRDGTextureRef HalationBlur0V = GraphBuilder.CreateTexture(MaskDesc0, TEXT("FilmEmulator.HalationBlur0V"));
    FRDGTextureRef HalationMask1 = GraphBuilder.CreateTexture(MaskDesc1, TEXT("FilmEmulator.HalationMask1"));
    FRDGTextureRef HalationBlur1H = GraphBuilder.CreateTexture(MaskDesc1, TEXT("FilmEmulator.HalationBlur1H"));
    FRDGTextureRef HalationBlur1V = GraphBuilder.CreateTexture(MaskDesc1, TEXT("FilmEmulator.HalationBlur1V"));
    FRDGTextureRef HalationMask2 = GraphBuilder.CreateTexture(MaskDesc2, TEXT("FilmEmulator.HalationMask2"));
    FRDGTextureRef HalationBlur2H = GraphBuilder.CreateTexture(MaskDesc2, TEXT("FilmEmulator.HalationBlur2H"));
    FRDGTextureRef HalationBlur2V = GraphBuilder.CreateTexture(MaskDesc2, TEXT("FilmEmulator.HalationBlur2V"));
    FScreenPassTexture HalationMaskPass0(HalationMask0, HalationRect0);
    const FScreenPassTextureViewport HalationViewport0(HalationMaskPass0);
    FScreenPassTexture HalationMaskPass1(HalationMask1, HalationRect1);
    const FScreenPassTextureViewport HalationViewport1(HalationMaskPass1);
    FScreenPassTexture HalationMaskPass2(HalationMask2, HalationRect2);
    const FScreenPassTextureViewport HalationViewport2(HalationMaskPass2);
    const float RadiusMidScale = FMath::Clamp(HalationSettings.RadiusMidScale, 1.0f, 8.0f);
    const float RadiusFarScale = FMath::Clamp(HalationSettings.RadiusFarScale, RadiusMidScale, 8.0f);
    const FVector4f HalationParams0(HalationSettings.Intensity, HalationSettings.Radius, HalationSettings.Threshold, HalationSettings.Remjet);
    const FVector4f HalationParams1(HalationSettings.Intensity, HalationSettings.Radius * RadiusMidScale, HalationSettings.Threshold, HalationSettings.Remjet);
    const FVector4f HalationParams2(HalationSettings.Intensity, HalationSettings.Radius * RadiusFarScale, HalationSettings.Threshold, HalationSettings.Remjet);
    // Extract halation mask (half-res)
    {
        FFilmEmulatorHalationExtractPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationExtractPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
        PassParameters->SceneColorTexture = SceneColorTexture;
        PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams0;
        PassParameters->PreExposure = PreExposure;
        PassParameters->ApplyAfterTonemap = 0.0f;
        PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));
    PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationMask0, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationExtractPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationExtract"),
            View,
            HalationViewport0,
            SceneViewport,
            PixelShader,
            PassParameters);
    }
    // Horizontal blur (level 0)
    {
        FFilmEmulatorHalationBlurPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationBlurPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationViewport0);
        PassParameters->HalationMaskTexture = HalationMask0;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams0;
        PassParameters->BlurDirection = FVector2f(1.0f, 0.0f);
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationBlur0H, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationBlurPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationBlurH0"),
            View,
            HalationViewport0,
            HalationViewport0,
            PixelShader,
            PassParameters);
    }
    // Vertical blur (level 0)
    {
        FFilmEmulatorHalationBlurPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationBlurPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationViewport0);
        PassParameters->HalationMaskTexture = HalationBlur0H;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams0;
        PassParameters->BlurDirection = FVector2f(0.0f, 1.0f);
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationBlur0V, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationBlurPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationBlurV0"),
            View,
            HalationViewport0,
            HalationViewport0,
            PixelShader,
            PassParameters);
    }
    // Downsample to level 1
    {
        FScreenPassTexture HalationBlur0Pass(HalationBlur0V, HalationRect0);
        const FScreenPassTextureViewport HalationBlurViewport0(HalationBlur0Pass);
        FFilmEmulatorHalationDownsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationDownsamplePS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationBlurViewport0);
        PassParameters->HalationSourceTexture = HalationBlur0V;
        PassParameters->HalationSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationMask1, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationDownsamplePS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationDownsample1"),
            View,
            HalationViewport1,
            HalationBlurViewport0,
            PixelShader,
            PassParameters);
    }
    // Horizontal blur (level 1)
    {
        FFilmEmulatorHalationBlurPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationBlurPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationViewport1);
        PassParameters->HalationMaskTexture = HalationMask1;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams1;
        PassParameters->BlurDirection = FVector2f(1.0f, 0.0f);
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationBlur1H, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationBlurPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationBlurH1"),
            View,
            HalationViewport1,
            HalationViewport1,
            PixelShader,
            PassParameters);
    }
    // Vertical blur (level 1)
    {
        FFilmEmulatorHalationBlurPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationBlurPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationViewport1);
        PassParameters->HalationMaskTexture = HalationBlur1H;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams1;
        PassParameters->BlurDirection = FVector2f(0.0f, 1.0f);
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationBlur1V, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationBlurPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationBlurV1"),
            View,
            HalationViewport1,
            HalationViewport1,
            PixelShader,
            PassParameters);
    }
    // Downsample to level 2
    {
        FScreenPassTexture HalationBlur1Pass(HalationBlur1V, HalationRect1);
        const FScreenPassTextureViewport HalationBlurViewport1(HalationBlur1Pass);
        FFilmEmulatorHalationDownsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationDownsamplePS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationBlurViewport1);
        PassParameters->HalationSourceTexture = HalationBlur1V;
        PassParameters->HalationSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationMask2, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationDownsamplePS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationDownsample2"),
            View,
            HalationViewport2,
            HalationBlurViewport1,
            PixelShader,
            PassParameters);
    }
    // Horizontal blur (level 2)
    {
        FFilmEmulatorHalationBlurPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationBlurPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationViewport2);
        PassParameters->HalationMaskTexture = HalationMask2;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams2;
        PassParameters->BlurDirection = FVector2f(1.0f, 0.0f);
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationBlur2H, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationBlurPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationBlurH2"),
            View,
            HalationViewport2,
            HalationViewport2,
            PixelShader,
            PassParameters);
    }
    // Vertical blur (level 2)
    {
        FFilmEmulatorHalationBlurPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationBlurPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(HalationViewport2);
        PassParameters->HalationMaskTexture = HalationBlur2H;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams2;
        PassParameters->BlurDirection = FVector2f(0.0f, 1.0f);
        PassParameters->RenderTargets[0] = FRenderTargetBinding(HalationBlur2V, ERenderTargetLoadAction::ENoAction);
        TShaderMapRef<FFilmEmulatorHalationBlurPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalationBlurV2"),
            View,
            HalationViewport2,
            HalationViewport2,
            PixelShader,
            PassParameters);
    }
    // Composite
    {
        FFilmEmulatorHalationPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorHalationPS::FParameters>();
        PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
        PassParameters->SceneColorTexture = SceneColorTexture;
        PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationMaskTexture = HalationBlur0V;
        PassParameters->HalationMaskTexture2 = HalationBlur1V;
        PassParameters->HalationMaskTexture3 = HalationBlur2V;
        PassParameters->HalationMaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->HalationParams0 = HalationParams0;
        const FLinearColor BaseTint = HalationSettings.Tint;
        const FLinearColor NearDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.6f, 0.25f, 1.0f), 0.45f);
        const FLinearColor MidDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.35f, 0.12f, 1.0f), 0.45f);
        const FLinearColor FarDefault = FMath::Lerp(BaseTint, FLinearColor(1.0f, 0.2f, 0.05f, 1.0f), 0.7f);
        auto ResolveTint = [](const FLinearColor& Override, const FLinearColor& Fallback)
        {
            const float Sum = Override.R + Override.G + Override.B;
            if (Override.A > 0.0f || Sum > KINDA_SMALL_NUMBER)
            {
                return Override;
            }
            return Fallback;
        };
        FLinearColor Tint0 = ResolveTint(HalationSettings.TintNear, NearDefault);
        FLinearColor Tint1 = ResolveTint(HalationSettings.TintMid, MidDefault);
        FLinearColor Tint2 = ResolveTint(HalationSettings.TintFar, FarDefault);
        if (bFilmIsBW)
        {
            auto ToGray = [](const FLinearColor& C)
            {
                const float L = C.R * 0.2126f + C.G * 0.7152f + C.B * 0.0722f;
                return FLinearColor(L, L, L, C.A);
            };
            Tint0 = ToGray(Tint0);
            Tint1 = ToGray(Tint1);
            Tint2 = ToGray(Tint2);
        }
        PassParameters->HalationTint = FVector4f(Tint0.R, Tint0.G, Tint0.B, Tint0.A);
        PassParameters->HalationTint2 = FVector4f(Tint1.R, Tint1.G, Tint1.B, Tint1.A);
        PassParameters->HalationTint3 = FVector4f(Tint2.R, Tint2.G, Tint2.B, Tint2.A);
        PassParameters->FilmPrintLUT = FilmPrintLUT->GetResource()->TextureRHI;
        PassParameters->FilmPrintLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        PassParameters->PrintStrength = PrintStrength;
        PassParameters->PrintExposureEV = PrintExposureEV;
        PassParameters->PreExposure = PreExposure;
        PassParameters->ApplyAfterTonemap = 0.0f;
        PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));
    PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
        TShaderMapRef<FFilmEmulatorHalationPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
        AddDrawScreenPass(
            GraphBuilder,
            RDG_EVENT_NAME("FilmEmulatorHalation"),
            View,
            SceneViewport,
            SceneViewport,
            PixelShader,
            PassParameters);
    }
    return MoveTemp(Output);
}
FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmGrainPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    const FFilmGrainSettings& GrainSettings,
    float GrainIsoCoverage,
    const FVector2f& GateWeaveOffsetUV,
    float PreExposure,
    const FScreenPassRenderTarget& OverrideOutput)
{
    FRDGTextureRef SceneColorTexture = SceneColor.Texture;
    if (!SceneColorTexture)
    {
        return SceneColor;
    }
    const FScreenPassTextureViewport SceneViewport(SceneColor);
    FScreenPassRenderTarget Output = OverrideOutput;
    if (!Output.IsValid())
    {
        FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
        OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.GrainOutput"));
        Output = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
    }
    FFilmEmulatorGrainPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorGrainPS::FParameters>();
    PassParameters->SceneColorTexture = SceneColorTexture;
    PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
    PassParameters->GateWeaveOffset = GateWeaveOffsetUV;
    const float SigmaRFactor = (GrainSettings.SigmaR > 0.0f) ? GrainSettings.SigmaR : GrainSettings.Softness;
    const float SigmaR = FMath::Max(SigmaRFactor, 0.0f) * GrainSettings.Size;
    const float FilterSigma = FMath::Max(GrainSettings.FilterSigma, 0.0f);
    const float Frame = static_cast<float>(View.Family ? View.Family->FrameNumber : 0);
    PassParameters->GrainParams0 = FVector4f(GrainSettings.Intensity, GrainSettings.Size, SigmaR, GrainSettings.Chromatic);
    PassParameters->GrainParams1 = FVector4f(GrainSettings.FormatScale, GrainSettings.Response, Frame, FilterSigma);
    PassParameters->GrainAnimationAmplitude = FMath::Clamp(GrainSettings.AnimationAmplitude, 0.0f, 1.0f);
    PassParameters->GrainIsoCoverage = FMath::Clamp(GrainIsoCoverage, 0.0f, 1.0f);
    PassParameters->PreExposure = PreExposure;
    PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
    TShaderMapRef<FFilmEmulatorGrainPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    AddDrawScreenPass(
        GraphBuilder,
        RDG_EVENT_NAME("FilmEmulatorGrain"),
        View,
        SceneViewport,
        SceneViewport,
        PixelShader,
        PassParameters);
    return MoveTemp(Output);
}
FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmScratchPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    const FFilmGateScratchSettings& ScratchSettings,
    const FVector2f& GateWeaveOffsetUV,
    float ScratchTime,
    float PreExposure,
    const FScreenPassRenderTarget& OverrideOutput)
{
    FRDGTextureRef SceneColorTexture = SceneColor.Texture;
    if (!SceneColorTexture)
    {
        return SceneColor;
    }
    const FScreenPassTextureViewport SceneViewport(SceneColor);
    FScreenPassRenderTarget Output = OverrideOutput;
    if (!Output.IsValid())
    {
        FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
        OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.ScratchOutput"));
        Output = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
    }
    FFilmEmulatorScratchPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorScratchPS::FParameters>();
    PassParameters->SceneColorTexture = SceneColorTexture;
    PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
    PassParameters->ScratchParams0 = FVector4f(ScratchSettings.Intensity, ScratchSettings.Density, ScratchSettings.Width, ScratchSettings.Length);
    PassParameters->ScratchParams1 = FVector4f(ScratchSettings.Frequency, ScratchSettings.OpacityJitter, ScratchSettings.LengthJitter, ScratchSettings.Polarity);
    PassParameters->ScratchParams2 = FVector4f(ScratchSettings.WidthJitter, 0.0f, 0.0f, 0.0f);
    PassParameters->ScratchTint = FVector4f(ScratchSettings.Tint.R, ScratchSettings.Tint.G, ScratchSettings.Tint.B, ScratchSettings.Tint.A);
    PassParameters->GateWeaveOffset = GateWeaveOffsetUV;
    PassParameters->ScratchSeed = FVector2f(ScratchSeedA, ScratchSeedB);
    PassParameters->ScratchTime = ScratchTime;
    PassParameters->ScratchFormatScale = ScratchSettings.FormatScale;
    PassParameters->PreExposure = PreExposure;
    PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
    TShaderMapRef<FFilmEmulatorScratchPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    AddDrawScreenPass(
        GraphBuilder,
        RDG_EVENT_NAME("FilmEmulatorScratch"),
        View,
        SceneViewport,
        SceneViewport,
        PixelShader,
        PassParameters);
    return MoveTemp(Output);
}
FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmDirtPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    const FFilmDirtSettings& DirtSettings,
    UTexture2D* DirtTexture,
    const FVector2f& GateWeaveOffsetUV,
    float DirtTime,
    float PreExposure,
    const FScreenPassRenderTarget& OverrideOutput)
{
    FRDGTextureRef SceneColorTexture = SceneColor.Texture;
    if (!SceneColorTexture)
    {
        return SceneColor;
    }
    const FScreenPassTextureViewport SceneViewport(SceneColor);
    FScreenPassRenderTarget Output = OverrideOutput;
    if (!Output.IsValid())
    {
        FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
        OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.DirtOutput"));
        Output = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
    }
    FFilmEmulatorDirtPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorDirtPS::FParameters>();
    PassParameters->SceneColorTexture = SceneColorTexture;
    PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
    const bool bUseTexture = DirtSettings.bUseTexture && DirtTexture && DirtTexture->GetResource();
    const float Tiling = FMath::Max(DirtSettings.TextureTiling, 0.001f);
    const float ScaleMin = FMath::Max(DirtSettings.TextureScaleMin, 0.01f);
    const float ScaleMax = FMath::Max(DirtSettings.TextureScaleMax, ScaleMin);

    PassParameters->DirtParams0 = FVector4f(DirtSettings.Intensity, DirtSettings.Density, DirtSettings.Size, DirtSettings.Softness);
    PassParameters->DirtParams1 = FVector4f(DirtSettings.Frequency, DirtSettings.SizeJitter, DirtSettings.OpacityJitter, DirtSettings.Polarity);
    PassParameters->DirtParams2 = FVector4f(bUseTexture ? 1.0f : 0.0f, Tiling, ScaleMin, ScaleMax);
    PassParameters->DirtParams3 = FVector4f(DirtSettings.NoiseScale, DirtSettings.NoiseStrength, DirtSettings.NoiseSpeed, DirtSettings.bInvertTexture ? 1.0f : 0.0f);
    PassParameters->DirtTint = FVector4f(DirtSettings.Tint.R, DirtSettings.Tint.G, DirtSettings.Tint.B, DirtSettings.Tint.A);
    PassParameters->GateWeaveOffset = GateWeaveOffsetUV;
    PassParameters->DirtTexture = bUseTexture ? DirtTexture->GetResource()->TextureRHI : GBlackTexture->TextureRHI;
    PassParameters->DirtTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
    PassParameters->DirtSeed = FVector2f(DirtSeedA, DirtSeedB);
    PassParameters->DirtTime = DirtTime;
    PassParameters->DirtFormatScale = DirtSettings.FormatScale;
    PassParameters->PreExposure = PreExposure;
    PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
    TShaderMapRef<FFilmEmulatorDirtPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    AddDrawScreenPass(
        GraphBuilder,
        RDG_EVENT_NAME("FilmEmulatorDirt"),
        View,
        SceneViewport,
        SceneViewport,
        PixelShader,
        PassParameters);
    return MoveTemp(Output);
}

FScreenPassTexture FFilmEmulatorViewExtension::ApplyFilmPrintPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FScreenPassTexture& SceneColor,
    UVolumeTexture* FilmPrintLUT,
    float PrintStrength,
    float PrintExposureEV,
    float PreExposure,
    const FScreenPassRenderTarget& OverrideOutput)
{
    FRDGTextureRef SceneColorTexture = SceneColor.Texture;
    if (!SceneColorTexture || !FilmPrintLUT || !FilmPrintLUT->GetResource())
    {
        return SceneColor;
    }
    const FScreenPassTextureViewport SceneViewport(SceneColor);
    FScreenPassRenderTarget Output = OverrideOutput;
    if (!Output.IsValid())
    {
        FRDGTextureDesc OutputDesc = SceneColorTexture->Desc;
        OutputDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("FilmEmulator.PrintOutput"));
        Output = FScreenPassRenderTarget(OutputTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
    }
    FFilmEmulatorPrintPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmEmulatorPrintPS::FParameters>();
    PassParameters->InputViewportParams = GetScreenPassTextureViewportParameters(SceneViewport);
    PassParameters->SceneColorTexture = SceneColorTexture;
    PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->FilmPrintLUT = FilmPrintLUT->GetResource()->TextureRHI;
    PassParameters->FilmPrintLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParameters->PrintStrength = PrintStrength;
    PassParameters->PrintExposureEV = PrintExposureEV;
    PassParameters->PreExposure = PreExposure;
    PassParameters->ApplyAfterTonemap = 0.0f;
    PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));
    PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
    TShaderMapRef<FFilmEmulatorPrintPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
    AddDrawScreenPass(
        GraphBuilder,
        RDG_EVENT_NAME("FilmEmulatorPrint"),
        View,
        SceneViewport,
        SceneViewport,
        PixelShader,
        PassParameters);
    return MoveTemp(Output);
}
// ---------------------------------------------------------------------------
// FFilmEmulatorModule
// ---------------------------------------------------------------------------
void FFilmEmulatorModule::StartupModule()
{
    if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FilmEmulator")))
    {
        const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/FilmEmulator"), ShaderDir);
    }
    OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([this]()
    {
        EnsureViewExtensionCreated();
    });
    if (GEngine != nullptr)
    {
        EnsureViewExtensionCreated();
    }
}
void FFilmEmulatorModule::EnsureViewExtensionCreated()
{
    if (!ViewExtension.IsValid())
    {
        ViewExtension = FSceneViewExtensions::NewExtension<FFilmEmulatorViewExtension>();
    }
}
void FFilmEmulatorModule::ShutdownModule()
{
    if (OnPostEngineInitHandle.IsValid())
    {
        FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
        OnPostEngineInitHandle.Reset();
    }
    ViewExtension.Reset();
}
FFilmEmulatorModule& FFilmEmulatorModule::Get()
{
    return FModuleManager::LoadModuleChecked<FFilmEmulatorModule>("FilmEmulator");
}




























