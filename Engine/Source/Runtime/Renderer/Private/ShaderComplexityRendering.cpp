// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.cpp: Contains definitions for rendering the shader complexity viewmode.
=============================================================================*/

#include "ShaderComplexityRendering.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/PostProcessVisualizeComplexity.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

int32 GShaderComplexityBaselineForwardVS = 134;
static FAutoConsoleVariableRef CVarShaderComplexityBaselineForwardVS(
	TEXT("r.ShaderComplexity.Baseline.Forward.VS"),
	GShaderComplexityBaselineForwardVS,
	TEXT("Minimum number of instructions for vertex shaders in forward shading (default=134)"),
	ECVF_Default
);

int32 GShaderComplexityBaselineForwardPS = 635;
static FAutoConsoleVariableRef CVarShaderComplexityBaselineForwardPS(
	TEXT("r.ShaderComplexity.Baseline.Forward.PS"),
	GShaderComplexityBaselineForwardPS,
	TEXT("Minimum number of instructions for pixel shaders in forward shading (default=635)"),
	ECVF_Default
);

int32 GShaderComplexityBaselineForwardUnlitPS = 47;
static FAutoConsoleVariableRef CVarShaderComplexityBaselineForwardUnlitPS(
	TEXT("r.ShaderComplexity.Baseline.Forward.UnlitPS"),
	GShaderComplexityBaselineForwardUnlitPS,
	TEXT("Minimum number of instructions for unlit material pixel shaders in forward shading (default=47)"),
	ECVF_Default
);

int32 GShaderComplexityBaselineDeferredVS = 41;
static FAutoConsoleVariableRef CVarShaderComplexityBaselineDeferredVS(
	TEXT("r.ShaderComplexity.Baseline.Deferred.VS"),
	GShaderComplexityBaselineDeferredVS,
	TEXT("Minimum number of instructions for vertex shaders in deferred shading (default=41)"),
	ECVF_Default
);

int32 GShaderComplexityBaselineDeferredPS = 111;
static FAutoConsoleVariableRef CVarShaderComplexityBaselineDeferredPS(
	TEXT("r.ShaderComplexity.Baseline.Deferred.PS"),
	GShaderComplexityBaselineDeferredPS,
	TEXT("Minimum number of instructions for pixel shaders in deferred shading (default=111)"),
	ECVF_Default
);

int32 GShaderComplexityBaselineDeferredUnlitPS = 33;
static FAutoConsoleVariableRef CVarShaderComplexityBaselineDeferredUnlitPS(
	TEXT("r.ShaderComplexity.Baseline.Deferred.UnlitPS"),
	GShaderComplexityBaselineDeferredUnlitPS,
	TEXT("Minimum number of instructions for unlit material pixel shaders in deferred shading (default=33)"),
	ECVF_Default
);

IMPLEMENT_SHADER_TYPE(template<>,TComplexityAccumulatePS<false>,TEXT("/Engine/Private/ShaderComplexityAccumulatePixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TComplexityAccumulatePS<true>,TEXT("/Engine/Private/QuadComplexityAccumulatePixelShader.usf"),TEXT("Main"),SF_Pixel);

template <bool bQuadComplexity>
void TComplexityAccumulatePS<bQuadComplexity>::GetDebugViewModeShaderBindings(
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT Material,
	EDebugViewShaderMode DebugViewMode,
	const FVector& ViewOrigin,
	int32 VisualizeLODIndex,
	int32 VisualizeElementIndex,
	int32 NumVSInstructions,
	int32 NumPSInstructions,
	int32 ViewModeParam,
	FName ViewModeParamName,
	FMeshDrawSingleShaderBindings& ShaderBindings
) const
{
	// normalize the complexity so we can fit it in a low precision scene color which is necessary on some platforms
	// late value is for overdraw which can be problematic with a low precision float format, at some point the precision isn't there any more and it doesn't accumulate
	if (DebugViewMode == DVSM_QuadComplexity)
	{
		ShaderBindings.Add(NormalizedComplexity, FVector4(NormalizedQuadComplexityValue));
	}
	else
	{
		const float NormalizeMul = 1.0f / GetMaxShaderComplexityCount(Material.GetFeatureLevel());
		ShaderBindings.Add(NormalizedComplexity, FVector4(NumPSInstructions * NormalizeMul, NumVSInstructions * NormalizeMul, 1 / 32.0f));
	}
	ShaderBindings.Add(ShowQuadOverdraw, DebugViewMode != DVSM_ShaderComplexity ? 1 : 0);
}

FDebugViewModePS* FComplexityAccumulateInterface::GetPixelShader(const FMaterial* InMaterial, FVertexFactoryType* VertexFactoryType) const
{
	if (bShowQuadComplexity)
	{
		return InMaterial->GetShader<TComplexityAccumulatePS<true>>(VertexFactoryType);
	}
	else
	{
		return InMaterial->GetShader<TComplexityAccumulatePS<false>>(VertexFactoryType);
	}

}
void FComplexityAccumulateInterface::SetDrawRenderState(EBlendMode BlendMode, FRenderState& DrawRenderState) const
{
	DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
	DrawRenderState.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
}

// Instantiate the template 
template class TComplexityAccumulatePS<false>;
template class TComplexityAccumulatePS<true>;

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
