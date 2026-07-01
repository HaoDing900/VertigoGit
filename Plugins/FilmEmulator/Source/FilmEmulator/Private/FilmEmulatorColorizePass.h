// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

class FILMEMULATOR_API FFilmEmulatorColorizePS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorColorizePS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorColorizePS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER_TEXTURE(Texture3D, FilmLUT)
        SHADER_PARAMETER_SAMPLER(SamplerState, FilmLUTSampler)
        SHADER_PARAMETER_TEXTURE(Texture3D, FilmPrintLUT)
        SHADER_PARAMETER_SAMPLER(SamplerState, FilmPrintLUTSampler)
        SHADER_PARAMETER(FVector4f, ColorParams)
        SHADER_PARAMETER(FVector2f, GateWeaveOffset)
        SHADER_PARAMETER(float, FlickerEV)
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

