// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PathCompactionCompute.cpp: Compute path continuation shader.
=============================================================================*/

#include "RendererPrivate.h"

#if RHI_RAYTRACING

#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

class FPathCompactionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPathCompactionCS, Global)
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	FPathCompactionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Input
		ViewParameter.Bind(Initializer.ParameterMap, TEXT("View"));
		RadianceTextureParameter.Bind(Initializer.ParameterMap, TEXT("RadianceTexture"));
		SampleCountTextureParameter.Bind(Initializer.ParameterMap, TEXT("SampleCountTexture"));
		PixelPositionTextureParameter.Bind(Initializer.ParameterMap, TEXT("PixelPositionTexture"));

		// Output
		RadianceSortedRedUAVParameter.Bind(Initializer.ParameterMap, TEXT("RadianceSortedRedRT"));
		RadianceSortedGreenUAVParameter.Bind(Initializer.ParameterMap, TEXT("RadianceSortedGreenRT"));
		RadianceSortedBlueUAVParameter.Bind(Initializer.ParameterMap, TEXT("RadianceSortedBlueRT"));
		RadianceSortedAlphaUAVParameter.Bind(Initializer.ParameterMap, TEXT("RadianceSortedAlphaRT"));
		SampleCountSortedUAVParameter.Bind(Initializer.ParameterMap, TEXT("SampleCountSortedRT"));
	}

	FPathCompactionCS() {}

	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		const FViewInfo& View,
		FTextureRHIParamRef RadianceTexture,
		FTextureRHIParamRef SampleCountTexture,
		FTextureRHIParamRef PixelPositionTexture,
		FUnorderedAccessViewRHIParamRef RadianceSortedRedUAV,
		FUnorderedAccessViewRHIParamRef RadianceSortedGreenUAV,
		FUnorderedAccessViewRHIParamRef RadianceSortedBlueUAV,
		FUnorderedAccessViewRHIParamRef RadianceSortedAlphaUAV,
		FUnorderedAccessViewRHIParamRef SampleCountSortedUAV)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		// Input textures
		SetTextureParameter(RHICmdList, ShaderRHI, RadianceTextureParameter, RadianceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, SampleCountTextureParameter, SampleCountTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, PixelPositionTextureParameter, PixelPositionTexture);

		// Output UAVs
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedRedUAVParameter, RadianceSortedRedUAV);
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedGreenUAVParameter, RadianceSortedGreenUAV);
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedBlueUAVParameter, RadianceSortedBlueUAV);
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedAlphaUAVParameter, RadianceSortedAlphaUAV);
		SetUAVParameter(RHICmdList, ShaderRHI, SampleCountSortedUAVParameter, SampleCountSortedUAV);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FUnorderedAccessViewRHIParamRef RadianceSortedRedUAV,
		FUnorderedAccessViewRHIParamRef RadianceSortedGreenUAV,
		FUnorderedAccessViewRHIParamRef RadianceSortedBlueUAV,
		FUnorderedAccessViewRHIParamRef RadianceSortedAlphaUAV,
		FUnorderedAccessViewRHIParamRef SampleCountSortedUAV,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedRedUAVParameter, FUnorderedAccessViewRHIRef());
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedGreenUAVParameter, FUnorderedAccessViewRHIRef());
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedBlueUAVParameter, FUnorderedAccessViewRHIRef());
		SetUAVParameter(RHICmdList, ShaderRHI, RadianceSortedAlphaUAVParameter, FUnorderedAccessViewRHIRef());
		SetUAVParameter(RHICmdList, ShaderRHI, SampleCountSortedUAVParameter, FUnorderedAccessViewRHIRef());
		FUnorderedAccessViewRHIParamRef UAVs[] = {
			RadianceSortedRedUAV,
			RadianceSortedGreenUAV,
			RadianceSortedBlueUAV,
			RadianceSortedAlphaUAV,
			SampleCountSortedUAV
		};
		RHICmdList.TransitionResources(TransitionAccess, TransitionPipeline, UAVs, 5, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ViewParameter;
		Ar << RadianceTextureParameter;
		Ar << SampleCountTextureParameter;
		Ar << PixelPositionTextureParameter;
		Ar << RadianceSortedRedUAVParameter;
		Ar << RadianceSortedGreenUAVParameter;
		Ar << RadianceSortedBlueUAVParameter;
		Ar << RadianceSortedAlphaUAVParameter;
		Ar << SampleCountSortedUAVParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	// Input parameters
	FShaderResourceParameter ViewParameter;
	FShaderResourceParameter RadianceTextureParameter;
	FShaderResourceParameter SampleCountTextureParameter;
	FShaderResourceParameter PixelPositionTextureParameter;

	// Output parameters
	FShaderResourceParameter RadianceSortedRedUAVParameter;
	FShaderResourceParameter RadianceSortedGreenUAVParameter;
	FShaderResourceParameter RadianceSortedBlueUAVParameter;
	FShaderResourceParameter RadianceSortedAlphaUAVParameter;
	FShaderResourceParameter SampleCountSortedUAVParameter;
};

IMPLEMENT_SHADER_TYPE(, FPathCompactionCS, TEXT("/Engine/Private/PathTracing/PathCompaction.usf"), TEXT("PathCompactionCS"), SF_Compute)

void FDeferredShadingSceneRenderer::ComputePathCompaction(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	FTextureRHIParamRef RadianceTexture,
	FTextureRHIParamRef SampleCountTexture,
	FTextureRHIParamRef PixelPositionTexture,
	FUnorderedAccessViewRHIParamRef RadianceSortedRedUAV,
	FUnorderedAccessViewRHIParamRef RadianceSortedGreenUAV,
	FUnorderedAccessViewRHIParamRef RadianceSortedBlueUAV,
	FUnorderedAccessViewRHIParamRef RadianceSortedAlphaUAV,
	FUnorderedAccessViewRHIParamRef SampleCountSortedUAV)
{
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPathCompactionCS> PathCompactionComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(PathCompactionComputeShader->GetComputeShader());

	FComputeFenceRHIRef Fence = RHICmdList.CreateComputeFence(TEXT("PathCompaction"));
	PathCompactionComputeShader->SetParameters(RHICmdList, View, RadianceTexture, SampleCountTexture, PixelPositionTexture, RadianceSortedRedUAV, RadianceSortedGreenUAV, RadianceSortedBlueUAV, RadianceSortedAlphaUAV, SampleCountSortedUAV);
	FIntPoint ViewSize = View.ViewRect.Size();
	FIntVector NumGroups = FIntVector::DivideAndRoundUp(FIntVector(ViewSize.X, ViewSize.Y, 0), FPathCompactionCS::GetGroupSize());
	DispatchComputeShader(RHICmdList, *PathCompactionComputeShader, NumGroups.X, NumGroups.Y, 1);
	PathCompactionComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, RadianceSortedRedUAV, RadianceSortedGreenUAV, RadianceSortedBlueUAV, RadianceSortedAlphaUAV, SampleCountSortedUAV, Fence);
}
#endif // RHI_RAYTRACING
