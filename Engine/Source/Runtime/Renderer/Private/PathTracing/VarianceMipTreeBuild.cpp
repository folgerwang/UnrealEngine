// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VarianceMipTreeBuild.cpp: SkyLight CDF build algorithm.
=============================================================================*/

#include "RendererPrivate.h"

#if RHI_RAYTRACING

#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RHI/Public/PipelineStateCache.h"

class FBuildVarianceMipTreeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildVarianceMipTreeCS, Global)
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

	FBuildVarianceMipTreeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		RadianceTextureParameter.Bind(Initializer.ParameterMap, TEXT("RadianceTexture"));
		RadianceTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("RadianceTextureSampler"));
		ViewSizeParameter.Bind(Initializer.ParameterMap, TEXT("ViewSize"));
		VarianceMapDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("VarianceMapDimensions"));
		MipLevelParameter.Bind(Initializer.ParameterMap, TEXT("MipLevel"));
		VarianceMipTreeParameter.Bind(Initializer.ParameterMap, TEXT("VarianceMipTree"));
	}

	FBuildVarianceMipTreeCS() {}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTextureRHIRef RadianceTexture,
		const FIntPoint& ViewSize,
		const FIntVector& VarianceMapDimensions,
		uint32 MipLevel,
		FRWBuffer& VarianceMipTree)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, ViewSizeParameter, ViewSize);
		SetShaderValue(RHICmdList, ShaderRHI, VarianceMapDimensionsParameter, VarianceMapDimensions);
		SetShaderValue(RHICmdList, ShaderRHI, MipLevelParameter, MipLevel);
		SetTextureParameter(RHICmdList, ShaderRHI, RadianceTextureParameter, RadianceTextureSamplerParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), RadianceTexture);

		check(VarianceMipTreeParameter.IsBound());
		VarianceMipTreeParameter.SetBuffer(RHICmdList, ShaderRHI, VarianceMipTree);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& VarianceMap,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		VarianceMipTreeParameter.UnsetUAV(RHICmdList, ShaderRHI);
		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, VarianceMap.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << RadianceTextureParameter;
		Ar << RadianceTextureSamplerParameter;
		Ar << ViewSizeParameter;
		Ar << VarianceMapDimensionsParameter;
		Ar << MipLevelParameter;
		Ar << VarianceMipTreeParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter RadianceTextureParameter;
	FShaderResourceParameter RadianceTextureSamplerParameter;
	FShaderParameter ViewSizeParameter;

	FShaderParameter VarianceMapDimensionsParameter;
	FShaderParameter MipLevelParameter;
	FRWShaderParameter VarianceMipTreeParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildVarianceMipTreeCS, TEXT("/Engine/Private/PathTracing/BuildVarianceMipTreeComputeShader.usf"), TEXT("BuildVarianceMipTreeCS"), SF_Compute)

void FDeferredShadingSceneRenderer::BuildVarianceMipTree(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FTextureRHIRef MeanAndDeviationTexture,
	FRWBuffer& VarianceMipTree, FIntVector& VarianceMipTreeDimensions)
{
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FBuildVarianceMipTreeCS> BuildVarianceMipTreeComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(BuildVarianceMipTreeComputeShader->GetComputeShader());

	// Allocate MIP tree
	FIntPoint ViewSize = View.ViewRect.Size();
	uint32 MipLevelCount = FMath::Min(FMath::CeilLogTwo(ViewSize.X), FMath::CeilLogTwo(ViewSize.Y));
	VarianceMipTreeDimensions = FIntVector(1 << MipLevelCount, 1 << MipLevelCount, 1);
	uint32 NumElements = VarianceMipTreeDimensions.X * VarianceMipTreeDimensions.Y;
	for (uint32 MipLevel = 1; MipLevel <= MipLevelCount; ++MipLevel)
	{
		NumElements += (VarianceMipTreeDimensions.X >> MipLevel) * (VarianceMipTreeDimensions.Y >> MipLevel);
	}
	VarianceMipTree.Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);
	// BUG: Figure out why clearing the UAV makes it impossible to rebuild the variance mip chain
	//ClearUAV(RHICmdList, VarianceMipTree, 0.0);

	// Execute hierarchical build
	for (uint32 MipLevel = 0; MipLevel <= MipLevelCount; ++MipLevel)
	{
		FComputeFenceRHIRef MipLevelFence = RHICmdList.CreateComputeFence(TEXT("VarianceMipTree Build"));
		BuildVarianceMipTreeComputeShader->SetParameters(RHICmdList, MeanAndDeviationTexture, ViewSize, VarianceMipTreeDimensions, MipLevel, VarianceMipTree);
		FIntVector MipLevelDimensions = FIntVector(VarianceMipTreeDimensions.X >> MipLevel, VarianceMipTreeDimensions.Y >> MipLevel, 1);
		FIntVector NumGroups = FIntVector::DivideAndRoundUp(MipLevelDimensions, FBuildVarianceMipTreeCS::GetGroupSize());
		DispatchComputeShader(RHICmdList, *BuildVarianceMipTreeComputeShader, NumGroups.X, NumGroups.Y, 1);
		BuildVarianceMipTreeComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, VarianceMipTree, MipLevelFence);
	}
	FComputeFenceRHIRef TransitionFence = RHICmdList.CreateComputeFence(TEXT("VarianceMipTree Transition"));
	BuildVarianceMipTreeComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, VarianceMipTree, TransitionFence);
}

class FVisualizeMipTreePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeMipTreePS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return ShouldCompileRayTracingShadersForProject(Platform);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FVisualizeMipTreePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		MipTreeParameter.Bind(Initializer.ParameterMap, TEXT("MipTree"));
	}

	FVisualizeMipTreePS() {}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FIntVector Dimensions,
		const FRWBuffer& MipTree)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreeParameter, MipTree.SRV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DimensionsParameter;
		Ar << MipTreeParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter DimensionsParameter;
	FShaderResourceParameter MipTreeParameter;
};

IMPLEMENT_SHADER_TYPE(, FVisualizeMipTreePS, TEXT("/Engine/Private/PathTracing/VisualizeMipTreePixelShader.usf"), TEXT("VisualizeMipTreePS"), SF_Pixel)

void FDeferredShadingSceneRenderer::VisualizeVarianceMipTree(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FRWBuffer& VarianceMipTree, FIntVector VarianceMipTreeDimensions)
{
	// Allocate render target
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	TRefCountPtr<IPooledRenderTarget> VarianceMipTreeRT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VarianceMipTreeRT, TEXT("VarianceMipTreeRT"));

	// Define shaders
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FVisualizeMipTreePS> PixelShader(ShaderMap);
	FTextureRHIParamRef RenderTargets[2] =
	{
		SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture, 
		VarianceMipTreeRT->GetRenderTargetItem().TargetableTexture
	};
	FRHIRenderPassInfo RenderPassInfo(2, RenderTargets, ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("VarianceMipTree Visualization"));

	// PSO definition
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, VarianceMipTree.UAV);

	// Draw
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	PixelShader->SetParameters(RHICmdList, View, VarianceMipTreeDimensions, VarianceMipTree);
	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
		SceneContext.GetBufferSizeXY(),
		*VertexShader);
	ResolveSceneColor(RHICmdList);
	RHICmdList.EndRenderPass();

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, VarianceMipTree.UAV);

	// Declare RT as visualizable
	RHICmdList.CopyToResolveTarget(VarianceMipTreeRT->GetRenderTargetItem().TargetableTexture, VarianceMipTreeRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	GVisualizeTexture.SetCheckPoint(RHICmdList, VarianceMipTreeRT);
}

#endif // RHI_RAYTRACING
