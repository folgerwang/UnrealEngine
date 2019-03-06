// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkyLightCdfBuild.cpp: SkyLight CDF build algorithm.
=============================================================================*/

#include "RendererPrivate.h"

#if RHI_RAYTRACING

#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RHI/Public/PipelineStateCache.h"

class FBuildSkyLightRowCdfCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildSkyLightRowCdfCS, Global)
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

	FBuildSkyLightRowCdfCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ModeParameter.Bind(Initializer.ParameterMap, TEXT("Mode"));
		TextureCubeParameter.Bind(Initializer.ParameterMap, TEXT("TextureCube0"));
		TextureCubeSamplerParameter.Bind(Initializer.ParameterMap, TEXT("TextureCubeSampler0"));
		CubeFaceParameter.Bind(Initializer.ParameterMap, TEXT("CubeFace"));
		LevelParameter.Bind(Initializer.ParameterMap, TEXT("Level"));
		RowCdfDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("RowCdfDimensions"));
		RowCdfParameter.Bind(Initializer.ParameterMap, TEXT("RowCdf"));
	}

	FBuildSkyLightRowCdfCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, uint32 Mode, const FTexture& TextureCube, uint32 CubeFace, uint32 Level, FIntVector RowCdfDimensions, const FRWBuffer& RowCdf)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, ModeParameter, Mode);
		SetTextureParameter(RHICmdList, ShaderRHI, TextureCubeParameter, TextureCubeSamplerParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), TextureCube.TextureRHI);
		SetShaderValue(RHICmdList, ShaderRHI, CubeFaceParameter, CubeFace);
		SetShaderValue(RHICmdList, ShaderRHI, LevelParameter, Level);
		SetShaderValue(RHICmdList, ShaderRHI, RowCdfDimensionsParameter, RowCdfDimensions);
		check(RowCdfParameter.IsBound());
		bool IsBound = RowCdfParameter.IsUAVBound();
		RowCdfParameter.SetBuffer(RHICmdList, ShaderRHI, RowCdf);

		FUnorderedAccessViewRHIParamRef UAVs[] = {
			RowCdf.UAV
		};
		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, UAVs, 1);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, EResourceTransitionAccess TransitionAccess, EResourceTransitionPipeline TransitionPipeline,
		const FRWBuffer& Buffer, FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		RowCdfParameter.UnsetUAV(RHICmdList, ShaderRHI);

		FUnorderedAccessViewRHIParamRef UAVs[] = {
			Buffer.UAV
		};
		RHICmdList.TransitionResources(TransitionAccess, TransitionPipeline, UAVs, 1, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ModeParameter;
		Ar << TextureCubeParameter;
		Ar << TextureCubeSamplerParameter;
		Ar << CubeFaceParameter;
		Ar << LevelParameter;
		Ar << RowCdfDimensionsParameter;
		Ar << RowCdfParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ModeParameter;
	FShaderResourceParameter TextureCubeParameter;
	FShaderResourceParameter TextureCubeSamplerParameter;
	FShaderParameter CubeFaceParameter;
	FShaderParameter LevelParameter;
	FShaderParameter RowCdfDimensionsParameter;
	FRWShaderParameter RowCdfParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildSkyLightRowCdfCS, TEXT("/Engine/Private/PathTracing/BuildSkyLightRowCdfComputeShader.usf"), TEXT("BuildSkyLightRowCdfCS"), SF_Compute)

class FBuildSkyLightColumnCdfCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildSkyLightColumnCdfCS, Global)
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

	FBuildSkyLightColumnCdfCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ModeParameter.Bind(Initializer.ParameterMap, TEXT("Mode"));
		RowCdfDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("RowCdfDimensions"));
		RowCdfParameter.Bind(Initializer.ParameterMap, TEXT("RowCdf"));
		LevelParameter.Bind(Initializer.ParameterMap, TEXT("Level"));
		ColumnCdfParameter.Bind(Initializer.ParameterMap, TEXT("ColumnCdf"));
	}

	FBuildSkyLightColumnCdfCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, uint32 Mode, FIntVector RowCdfDimensions, const FRWBuffer& RowCdf, uint32 Level, const FRWBuffer& ColumnCdf)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, ModeParameter, Mode);
		SetShaderValue(RHICmdList, ShaderRHI, RowCdfDimensionsParameter, RowCdfDimensions);
		RowCdfParameter.SetBuffer(RHICmdList, ShaderRHI, RowCdf);
		SetShaderValue(RHICmdList, ShaderRHI, LevelParameter, Level);
		ColumnCdfParameter.SetBuffer(RHICmdList, ShaderRHI, ColumnCdf);

		FUnorderedAccessViewRHIParamRef UAVs[] = {
			ColumnCdf.UAV
		};
		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, UAVs, 1);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, EResourceTransitionAccess TransitionAccess, EResourceTransitionPipeline TransitionPipeline,
		const FRWBuffer& ColumnCdf, FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		ColumnCdfParameter.UnsetUAV(RHICmdList, ShaderRHI);

		FUnorderedAccessViewRHIParamRef UAVs[] = {
			ColumnCdf.UAV
		};
		RHICmdList.TransitionResources(TransitionAccess, TransitionPipeline, UAVs, 1, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ModeParameter;
		Ar << RowCdfDimensionsParameter;
		Ar << RowCdfParameter;
		Ar << LevelParameter;
		Ar << ColumnCdfParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ModeParameter;
	FShaderParameter RowCdfDimensionsParameter;
	FRWShaderParameter RowCdfParameter;
	FShaderParameter LevelParameter;
	FRWShaderParameter ColumnCdfParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildSkyLightColumnCdfCS, TEXT("/Engine/Private/PathTracing/BuildSkyLightColumnCdfComputeShader.usf"), TEXT("BuildSkyLightColumnCdfCS"), SF_Compute)

class FBuildSkyLightCubeFaceCdfCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildSkyLightCubeFaceCdfCS, Global)
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

	FBuildSkyLightCubeFaceCdfCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ColumnCdfDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ColumnCdfDimensions"));
		ColumnCdfParameter.Bind(Initializer.ParameterMap, TEXT("ColumnCdf"));
		CubeFaceCdfParameter.Bind(Initializer.ParameterMap, TEXT("CubeFaceCdf"));
	}

	FBuildSkyLightCubeFaceCdfCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, FIntVector ColumnCdfDimensions, const FRWBuffer& ColumnCdf, const FRWBuffer& CubeFaceCdf)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, ColumnCdfDimensionsParameter, ColumnCdfDimensions);
		ColumnCdfParameter.SetBuffer(RHICmdList, ShaderRHI, ColumnCdf);
		CubeFaceCdfParameter.SetBuffer(RHICmdList, ShaderRHI, CubeFaceCdf);

		FUnorderedAccessViewRHIParamRef UAVs[] = {
			CubeFaceCdf.UAV
		};
		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, UAVs, 1);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, EResourceTransitionAccess TransitionAccess, EResourceTransitionPipeline TransitionPipeline,
		const FRWBuffer& CubeFaceCdf, FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		CubeFaceCdfParameter.UnsetUAV(RHICmdList, ShaderRHI);

		FUnorderedAccessViewRHIParamRef UAVs[] = {
			CubeFaceCdf.UAV
		};
		RHICmdList.TransitionResources(TransitionAccess, TransitionPipeline, UAVs, 1, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ColumnCdfDimensionsParameter;
		Ar << ColumnCdfParameter;
		Ar << CubeFaceCdfParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ColumnCdfDimensionsParameter;
	FRWShaderParameter ColumnCdfParameter;
	FRWShaderParameter CubeFaceCdfParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildSkyLightCubeFaceCdfCS, TEXT("/Engine/Private/PathTracing/BuildSkyLightCubeFaceCdfComputeShader.usf"), TEXT("BuildSkyLightCubeFaceCdfCS"), SF_Compute)

class FVisualizeCdfPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeCdfPS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FVisualizeCdfPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		RowCdfParameter.Bind(Initializer.ParameterMap, TEXT("RowCdf"));
		ColumnCdfParameter.Bind(Initializer.ParameterMap, TEXT("ColumnCdf"));
		CubeFaceCdfParameter.Bind(Initializer.ParameterMap, TEXT("CubeFaceCdf"));
	}

	FVisualizeCdfPS()
	{
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FIntVector Dimensions,
		const FRWBuffer& RowCdf,
		const FRWBuffer& ColumnCdf,
		const FRWBuffer& CubeFaceCdf)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		//check(BufferParameter.IsBound());
		SetSRVParameter(RHICmdList, ShaderRHI, RowCdfParameter, RowCdf.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ColumnCdfParameter, ColumnCdf.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, CubeFaceCdfParameter, CubeFaceCdf.SRV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOudatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DimensionsParameter;
		Ar << RowCdfParameter;
		Ar << ColumnCdfParameter;
		Ar << CubeFaceCdfParameter;
		return bShaderHasOudatedParameters;
	}

public:
	FShaderParameter DimensionsParameter;
	FShaderResourceParameter RowCdfParameter;
	FShaderResourceParameter ColumnCdfParameter;
	FShaderResourceParameter CubeFaceCdfParameter;
};

IMPLEMENT_SHADER_TYPE(, FVisualizeCdfPS, TEXT("/Engine/Private/PathTracing/VisualizeSkyLightCdfPixelShader.usf"), TEXT("VisualizeSkyLightCdfPS"), SF_Pixel);

void FDeferredShadingSceneRenderer::BuildSkyLightCdf(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FTexture& SkyLightTextureCube,
	FRWBuffer& RowCdf, FRWBuffer& ColumnCdf, FRWBuffer& CubeFaceCdf)
{
	FIntVector Dimensions = FIntVector(SkyLightTextureCube.GetSizeX(), SkyLightTextureCube.GetSizeY(), 6);

	// Buffer allocation
	//FRWBuffer RowCdf;
	uint32 NumElements = Dimensions.X * Dimensions.Y * Dimensions.Z;
	RowCdf.Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);

	//FRWBuffer ColumnCdf;
	NumElements = Dimensions.Y * Dimensions.Z;
	ColumnCdf.Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);

	//FRWBuffer CubeFaceCdf;
	NumElements = Dimensions.Z;
	CubeFaceCdf.Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);

	// Define row CDF
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FBuildSkyLightRowCdfCS> RowCdfComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(RowCdfComputeShader->GetComputeShader());

	enum Mode {
		PrefixSum = 0,
		Normalize = 1
	};

	// Prefix sum
	FIntVector NumRowCdfGroups = FIntVector::DivideAndRoundUp(Dimensions, FBuildSkyLightRowCdfCS::GetGroupSize());
	for (int CubeFace = 0; CubeFace < Dimensions.Z; ++CubeFace)
	{
		uint32 LevelCount = FMath::Log2(Dimensions.X);
		for (uint32 Level = 0; Level <= LevelCount; ++Level)
		{
			FComputeFenceRHIRef PrefixSumFence = RHICmdList.CreateComputeFence(TEXT("RowCdf Prefix Sum"));
			RowCdfComputeShader->SetParameters(RHICmdList, Mode::PrefixSum, SkyLightTextureCube, CubeFace, Level, Dimensions, RowCdf);
			DispatchComputeShader(RHICmdList, *RowCdfComputeShader, NumRowCdfGroups.X, NumRowCdfGroups.Y, 1);
			RowCdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, RowCdf, PrefixSumFence);
		}
	}

	// Define column cdf
	TShaderMapRef<FBuildSkyLightColumnCdfCS> ColumnCdfComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(ColumnCdfComputeShader->GetComputeShader());

	// Prefix sum
	FIntVector NumColumnCdfGroups = FIntVector::DivideAndRoundUp(FIntVector(Dimensions.Y, Dimensions.Z, 0), FBuildSkyLightColumnCdfCS::GetGroupSize());
	uint32 LevelCount = FMath::Log2(Dimensions.Y);
	for (uint32 Level = 0; Level <= LevelCount; ++Level)
	{
		FComputeFenceRHIRef PrefixSumFence = RHICmdList.CreateComputeFence(TEXT("ColumnCdf Prefix Sum"));
		ColumnCdfComputeShader->SetParameters(RHICmdList, Mode::PrefixSum, Dimensions, RowCdf, Level, ColumnCdf);
		DispatchComputeShader(RHICmdList, *ColumnCdfComputeShader, NumColumnCdfGroups.X, NumColumnCdfGroups.Y, 1);
		ColumnCdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ColumnCdf, PrefixSumFence);
	}

	// Define cube face cdf
	TShaderMapRef<FBuildSkyLightCubeFaceCdfCS> CubeFaceCdfComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(CubeFaceCdfComputeShader->GetComputeShader());

	// Prefix sum and cdf normalization
	FComputeFenceRHIRef CubeFaceFence = RHICmdList.CreateComputeFence(TEXT("CubeFaceCdf"));
	CubeFaceCdfComputeShader->SetParameters(RHICmdList, Dimensions, ColumnCdf, CubeFaceCdf);
	DispatchComputeShader(RHICmdList, *CubeFaceCdfComputeShader, 1, 1, 1);
	CubeFaceCdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, CubeFaceCdf, CubeFaceFence);

	// Normalization of column cdf
	RHICmdList.SetComputeShader(ColumnCdfComputeShader->GetComputeShader());
	FComputeFenceRHIRef ColumnCdfFence = RHICmdList.CreateComputeFence(TEXT("ColumnCdf"));
	ColumnCdfComputeShader->SetParameters(RHICmdList, Mode::Normalize, Dimensions, RowCdf, 0, ColumnCdf);
	DispatchComputeShader(RHICmdList, *ColumnCdfComputeShader, NumColumnCdfGroups.X, NumColumnCdfGroups.Y, 1);
	ColumnCdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, ColumnCdf, ColumnCdfFence);

	// Normalization of row cdf
	RHICmdList.SetComputeShader(RowCdfComputeShader->GetComputeShader());
	for (int CubeFace = 0; CubeFace < Dimensions.Z; ++CubeFace)
	{
		FComputeFenceRHIRef RowCdfNormalizationFence = RHICmdList.CreateComputeFence(TEXT("RowCdf Normalization"));
		RowCdfComputeShader->SetParameters(RHICmdList, Mode::Normalize, SkyLightTextureCube, CubeFace, 0, Dimensions, RowCdf);
		DispatchComputeShader(RHICmdList, *RowCdfComputeShader, NumRowCdfGroups.X, NumRowCdfGroups.Y, 1);
		RowCdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, RowCdf, RowCdfNormalizationFence);
	}
	FComputeFenceRHIRef RowCdfFence = RHICmdList.CreateComputeFence(TEXT("RowCdf"));
	RowCdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, RowCdf, RowCdfFence);

	// DEBUG: Visualize cdfs. Writes to scene color
	//VisualizeSkyLightCdf(RHICmdList, View, Dimensions, RowCdf, ColumnCdf, CubeFaceCdf);
}

void FDeferredShadingSceneRenderer::VisualizeSkyLightCdf(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FIntVector Dimensions, const FRWBuffer& RowCdf, const FRWBuffer& ColumnCdf, const FRWBuffer& CubeFaceCdf)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	//Desc.Format = PF_R32_FLOAT; // TODO: Make as scalar

	TRefCountPtr<IPooledRenderTarget> OutputRT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutputRT, TEXT("SkylightCdfRT"));

	// Run compositing engine
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FVisualizeCdfPS> PixelShader(ShaderMap);
	FTextureRHIParamRef RenderTargets[1] =
	{
		OutputRT->GetRenderTargetItem().TargetableTexture
	};
	FRHIRenderPassInfo RenderPassInfo(1, RenderTargets, ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("SkyLight Visualization"));

	// DEBUG: Inspect render target in isolation
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		PixelShader->SetParameters(RHICmdList, View, Dimensions, RowCdf, ColumnCdf, CubeFaceCdf);
		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*VertexShader);
	}
	RHICmdList.EndRenderPass();
	GVisualizeTexture.SetCheckPoint(RHICmdList, OutputRT);
}

#endif // RHI_RAYTRACING

