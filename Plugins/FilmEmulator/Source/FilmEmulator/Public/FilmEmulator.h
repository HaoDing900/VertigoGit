// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "SceneViewExtension.h"
#include "UObject/StrongObjectPtr.h"
#include "FilmEmulatorSettings.h"

struct FPostProcessingInputs;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;
struct FScreenPassRenderTarget;
class UVolumeTexture;
class UTexture2D;
class UFilmStockPreset;

class FILMEMULATOR_API FFilmEmulatorViewExtension : public FSceneViewExtensionBase
{
public:
    explicit FFilmEmulatorViewExtension(const FAutoRegister& AutoRegister);

    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
    // UE 5.3 has no always-on, pre-tonemap EPostProcessingPass entry (the 5.6 build hooked
    // BeforeDOF/AfterDOF, which don't exist here, and MotionBlur is skipped whenever motion blur
    // is off -- i.e. in editor viewports and PIE without motion blur). PrePostProcessPass runs
    // unconditionally right before post processing, in the same linear pre-tonemap scene-color
    // space the 5.6 build applied the effect in.
    virtual void PrePostProcessPass_RenderThread(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FPostProcessingInputs& Inputs) override;

private:
    FScreenPassTexture ApplyFilmEmulationChain_RenderThread(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FScreenPassTexture& SceneColor,
        const FScreenPassRenderTarget& OverrideOutput);

    FScreenPassTexture ApplyFilmEmulationPass(
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
        const FScreenPassRenderTarget& OverrideOutput);

    FScreenPassTexture ApplyFilmHalationPass(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FScreenPassTexture& SceneColor,
        const FFilmHalationSettings& HalationSettings,
        bool bFilmIsBW,
        UVolumeTexture* FilmPrintLUT,
        float PrintStrength,
        float PrintExposureEV,
        float PreExposure,
        const FScreenPassRenderTarget& OverrideOutput);

    FScreenPassTexture ApplyFilmGrainPass(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FScreenPassTexture& SceneColor,
        const FFilmGrainSettings& GrainSettings,
        float GrainIsoCoverage,
        const FVector2f& GateWeaveOffsetUV,
        float PreExposure,
        const FScreenPassRenderTarget& OverrideOutput);
    FScreenPassTexture ApplyFilmScratchPass(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FScreenPassTexture& SceneColor,
        const FFilmGateScratchSettings& ScratchSettings,
        const FVector2f& GateWeaveOffsetUV,
        float ScratchTime,
        float PreExposure,
        const FScreenPassRenderTarget& OverrideOutput);

    FScreenPassTexture ApplyFilmDirtPass(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FScreenPassTexture& SceneColor,
        const FFilmDirtSettings& DirtSettings,
        UTexture2D* DirtTexture,
        const FVector2f& GateWeaveOffsetUV,
        float DirtTime,
        float PreExposure,
        const FScreenPassRenderTarget& OverrideOutput);

    FScreenPassTexture ApplyFilmPrintPass(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FScreenPassTexture& SceneColor,
        UVolumeTexture* FilmPrintLUT,
        float PrintStrength,
        float PrintExposureEV,
        float PreExposure,
        const FScreenPassRenderTarget& OverrideOutput);

    FCriticalSection ParamsMutex;
    FFilmEmulatorParams CachedParams;
    FFilmGrainSettings CachedGrain;
    float CachedGrainIsoCoverage = 0.0f;
    FFilmHalationSettings CachedHalation;
    bool CachedFilmIsBW = false;
    FFilmGateWeaveSettings CachedGateWeave;
    FFilmFlickerSettings CachedFlicker;
    FFilmGateScratchSettings CachedGateScratch;
    FFilmDirtSettings CachedDirt;
    float CachedSaturationBias = 1.0f;
    float CachedContrastBias = 1.0f;
    float CachedExposureBias = 0.0f;
    TStrongObjectPtr<UVolumeTexture> CachedLUTTexture;
    TStrongObjectPtr<UVolumeTexture> CachedPrintLUTTexture;
    TStrongObjectPtr<UTexture2D> CachedDirtTexture;
    FString CachedPresetPath;
    FString CachedLUTSourcePath;
    FString CachedPrintLUTSourcePath;
    float CachedPrintStrength = 0.0f;
    float CachedPrintExposureEV = 0.0f;
    bool CachedApplyAfterTonemap = false;
    float GateWeaveSeedX = 0.0f;
    float GateWeaveSeedY = 0.0f;
    float GateWeaveSeedZ = 0.0f;
    float FlickerSeedA = 0.0f;
    float FlickerSeedB = 0.0f;
    float ScratchSeedA = 0.0f;
    float ScratchSeedB = 0.0f;
    float DirtSeedA = 0.0f;
    float DirtSeedB = 0.0f;
};

class FILMEMULATOR_API FFilmEmulatorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static FFilmEmulatorModule& Get();

private:
    void EnsureViewExtensionCreated();

    TSharedPtr<FFilmEmulatorViewExtension, ESPMode::ThreadSafe> ViewExtension;
    FDelegateHandle OnPostEngineInitHandle;
};












