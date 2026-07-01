// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

class FILMEMULATOR_API FFilmEmulatorHalationPS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorHalationPS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorHalationPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalationMaskTexture)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalationMaskTexture2)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalationMaskTexture3)
        SHADER_PARAMETER_SAMPLER(SamplerState, HalationMaskSampler)
        SHADER_PARAMETER(FVector4f, HalationParams0)
        SHADER_PARAMETER(FVector4f, HalationTint)
        SHADER_PARAMETER(FVector4f, HalationTint2)
        SHADER_PARAMETER(FVector4f, HalationTint3)
        SHADER_PARAMETER_TEXTURE(Texture3D, FilmPrintLUT)
        SHADER_PARAMETER_SAMPLER(SamplerState, FilmPrintLUTSampler)
        SHADER_PARAMETER(float, PrintStrength)
        SHADER_PARAMETER(float, PrintExposureEV)
        SHADER_PARAMETER(float, PreExposure)
        SHADER_PARAMETER(float, ApplyAfterTonemap)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};


class FILMEMULATOR_API FFilmEmulatorHalationDownsamplePS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorHalationDownsamplePS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorHalationDownsamplePS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalationSourceTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, HalationSourceSampler)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};

class FILMEMULATOR_API FFilmEmulatorHalationExtractPS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorHalationExtractPS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorHalationExtractPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
        SHADER_PARAMETER(FVector4f, HalationParams0)
        SHADER_PARAMETER(float, PreExposure)
        SHADER_PARAMETER(float, ApplyAfterTonemap)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};

class FILMEMULATOR_API FFilmEmulatorHalationBlurPS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorHalationBlurPS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorHalationBlurPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalationMaskTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, HalationMaskSampler)
        SHADER_PARAMETER(FVector4f, HalationParams0)
        SHADER_PARAMETER(FVector2f, BlurDirection)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};
