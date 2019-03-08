// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.cpp: Contains definitions for rendering the shader complexity viewmode.
=============================================================================*/

#include "ShaderComplexityRendering.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/PostProcessVisualizeComplexity.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

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
	if (bShowShaderComplexity)
	{
		// When rendering masked materials in the shader complexity viewmode, 
		// We want to overwrite complexity for the pixels which get depths written,
		// And accumulate complexity for pixels which get killed due to the opacity mask being below the clip value.
		// This is accomplished by forcing the masked materials to render depths in the depth only pass, 
		// Then rendering in the base pass with additive complexity blending, depth tests on, and depth writes off.
		DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();

		if (IsTranslucentBlendMode(BlendMode))
		{
			DrawRenderState.BlendState = TStaticBlendState<>::GetRHI();
		}
		else
		{
			// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
			DrawRenderState.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
		}
	}
	else
	{
		FDebugViewModeInterface::SetDrawRenderState(BlendMode, DrawRenderState);
	}
}

// Instantiate the template 
template class TComplexityAccumulatePS<false>;
template class TComplexityAccumulatePS<true>;

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
