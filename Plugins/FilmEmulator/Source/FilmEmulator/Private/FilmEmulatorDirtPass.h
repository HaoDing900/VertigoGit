// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

class FILMEMULATOR_API FFilmEmulatorDirtPS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorDirtPS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorDirtPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER_TEXTURE(Texture2D, DirtTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, DirtTextureSampler)
        SHADER_PARAMETER(FVector4f, DirtParams0)
        SHADER_PARAMETER(FVector4f, DirtParams1)
        SHADER_PARAMETER(FVector4f, DirtParams2)
        SHADER_PARAMETER(FVector4f, DirtParams3)
        SHADER_PARAMETER(FVector4f, DirtTint)
        SHADER_PARAMETER(FVector2f, GateWeaveOffset)
        SHADER_PARAMETER(FVector2f, DirtSeed)
        SHADER_PARAMETER(float, DirtTime)
        SHADER_PARAMETER(float, DirtFormatScale)
        SHADER_PARAMETER(float, PreExposure)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};
