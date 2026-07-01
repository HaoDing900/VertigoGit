// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

class FILMEMULATOR_API FFilmEmulatorScratchPS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FFilmEmulatorScratchPS);
    SHADER_USE_PARAMETER_STRUCT(FFilmEmulatorScratchPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
        SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewportParams)
        SHADER_PARAMETER(FVector4f, ScratchParams0)
        SHADER_PARAMETER(FVector4f, ScratchParams1)
        SHADER_PARAMETER(FVector4f, ScratchParams2)
        SHADER_PARAMETER(FVector4f, ScratchTint)
        SHADER_PARAMETER(FVector2f, GateWeaveOffset)
        SHADER_PARAMETER(FVector2f, ScratchSeed)
        SHADER_PARAMETER(float, ScratchTime)
        SHADER_PARAMETER(float, ScratchFormatScale)
        SHADER_PARAMETER(float, PreExposure)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};

