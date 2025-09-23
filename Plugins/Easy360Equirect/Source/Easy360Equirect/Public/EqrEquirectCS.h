#pragma once
#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FEqrCubeToEqrCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FEqrCubeToEqrCS);
    SHADER_USE_PARAMETER_STRUCT(FEqrCubeToEqrCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FVector2f, OutSize)
        SHADER_PARAMETER(FVector2f, ViewportMin)
        SHADER_PARAMETER(FVector2f, ViewportMax)
        SHADER_PARAMETER(FMatrix44f, RotMat)
        SHADER_PARAMETER(FVector4f, MiscFlags)
        SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
        SHADER_PARAMETER_TEXTURE(TextureCube, InCube)
        SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutEqr)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    { return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); }
};
