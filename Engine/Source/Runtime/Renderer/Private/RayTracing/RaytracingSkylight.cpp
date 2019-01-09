// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RaytracingSkyLight.cpp implements sky lighting with ray tracing
=============================================================================*/

#include "DeferredShadingRenderer.h"

static int32 GRayTracingSkyLight = 0;
bool IsRayTracingSkyLightSelected()
{
#if RHI_RAYTRACING
	return IsRayTracingEnabled() && GRayTracingSkyLight > 0;
#else
	return false;
#endif
}

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "VisualizeTexture.h"
#include "RayGenShaderUtils.h"
#include "SceneViewFamilyBlackboard.h"

#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

static FAutoConsoleVariableRef CVarRayTracingSkyLight(
	TEXT("r.RayTracing.SkyLight"),
	GRayTracingSkyLight,
	TEXT("Enables ray tracing SkyLight (default = 0)")
);

static int32 GRayTracingSkyLightSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingSkyLightSamplesPerPixel(
	TEXT("r.RayTracing.SkyLight.SamplesPerPixel"),
	GRayTracingSkyLightSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for ray tracing SkyLight (default = 1)")
);

static int32 GRayTracingSkyLightSamplingStopLevel = 0;
static FAutoConsoleVariableRef CVarRayTracingSkyLightSamplingStopLevel(
	TEXT("r.RayTracing.SkyLight.Sampling.StopLevel"),
	GRayTracingSkyLightSamplingStopLevel,
	TEXT("Sets the stop level for MIP-sampling (default = 0)")
);


#if 0
class FSkylightRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSkylightRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FSkylightRG, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SamplesPerPixel)
		SHADER_PARAMETER(FVector, SkyLightColor)

		//SHADER_PARAMETER_TEXTURE(TextureCube, SkyLightTexture)
		//SHADER_PARAMETER_SAMPLER(SamplerState, SkyLightTextureSampler)

		// Ray tracing parameters
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWOcclusionMaskUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWRayDistanceUAV)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FSceneTexturesUniformParameters, SceneTexturesStruct)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSkylightRG, "/Engine/Private/Raytracing/RaytracingSkylightRGS.usf", "SkyLightRGS", SF_RayGen);

void FDeferredShadingSceneRenderer::RenderRayTracingSkyLight(
	FRHICommandListImmediate& RHICmdList,
	TRefCountPtr<IPooledRenderTarget>& OutSkyLightTexture
)
{
	//check(LightSceneInfo);
	check(Views.Num() == 1);
	const FViewInfo& View = Views[0];

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FRDGBuilder GraphBuilder(RHICmdList);

	// Render targets
	FRDGTextureRef ScreenShadowMaskTexture;
	{
		FRDGTextureDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		ScreenShadowMaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingSkylight"));
	}

	FRDGTextureRef RayDistanceTexture;
	{
		FRDGTextureDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Format = PF_R16F;
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		RayDistanceTexture = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingOcclusionDistance"));
	}

	// TODO(RDG): use FSceneViewFamilyBlackboard.
	FSceneTexturesUniformParameters SceneTextures;
	SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);

	FSkylightRG::FParameters* PassParameters = GraphBuilder.AllocParameters<FSkylightRG::FParameters>();
	PassParameters->RWOcclusionMaskUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenShadowMaskTexture));
	PassParameters->RWRayDistanceUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RayDistanceTexture));
	PassParameters->SamplesPerPixel = GRayTracingSkyLightSamplesPerPixel;
	PassParameters->SamplingStopLevel = GRayTracingSkyLightSamplingStopLevel;
	PassParameters->SkyLightColor = FVector(1.0);
	//PassParameters->SkyLightTexture = Scene->SkyLight->ProcessedTexture->TextureRHI;
	//PassParameters->SkyLightTextureSampler = Scene->SkyLight->ProcessedTexture->SamplerStateRHI;
	PassParameters->TLAS = View.PerViewRayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->SceneTexturesStruct = CreateUniformBufferImmediate(SceneTextures, EUniformBufferUsage::UniformBuffer_SingleDraw);

	TShaderMapRef<FSkylightRG> RayGenerationShader(GetGlobalShaderMap(FeatureLevel));

	FRayGenShaderUtils::AddRayTraceDispatchPass(
		GraphBuilder,
		RDG_EVENT_NAME("RayTracedSkyLight %dx%d", View.ViewRect.Width(), View.ViewRect.Height()),
		*RayGenerationShader,
		PassParameters,
		View.ViewRect.Size());

	GraphBuilder.QueueTextureExtraction(ScreenShadowMaskTexture, &OutSkyLightTexture);
	GraphBuilder.Execute();
}
#else

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkyLightData, )
SHADER_PARAMETER(int, SamplesPerPixel)
SHADER_PARAMETER(int, SamplingStopLevel)
SHADER_PARAMETER(FVector, Color)
SHADER_PARAMETER(FIntVector, MipDimensions)
SHADER_PARAMETER_TEXTURE(TextureCube, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePosX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreeNegX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePosY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreeNegY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePosZ)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreeNegZ)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfPosX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfNegX)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfPosY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfNegY)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfPosZ)
SHADER_PARAMETER_SRV(Buffer<float>, MipTreePdfNegZ)
SHADER_PARAMETER_SRV(Buffer<float>, SolidAnglePdf)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSkyLightData, "SkyLight");

DECLARE_GPU_STAT_NAMED(RayTracingSkyLight, TEXT("Ray Tracing SkyLight"));
DECLARE_GPU_STAT_NAMED(BuildSkyLightMipTree, TEXT("Build SkyLight Mip Tree"));

class FSkylightRG : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSkylightRG, Global)

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FSkylightRG() {}
	virtual ~FSkylightRG() {}

	FSkylightRG(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ViewParameter.Bind(Initializer.ParameterMap, TEXT("View"));
		TLASParameter.Bind(Initializer.ParameterMap, TEXT("TLAS"));
		SceneTexturesParameter.Bind(Initializer.ParameterMap, TEXT("SceneTexturesStruct"));
		SkyLightParameter.Bind(Initializer.ParameterMap, TEXT("SkyLight"));

		OcclusionMaskUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWOcclusionMaskUAV"));
		RayDistanceUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWRayDistanceUAV"));
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ViewParameter;
		Ar << TLASParameter;
		Ar << SceneTexturesParameter;
		Ar << SkyLightParameter;
		Ar << OcclusionMaskUAVParameter;
		Ar << RayDistanceUAVParameter;
		return bShaderHasOutdatedParameters;
	}

	void Dispatch(
		FRHICommandListImmediate& RHICmdList,
		const FRayTracingScene& RayTracingScene,
		FUniformBufferRHIParamRef ViewUniformBuffer,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer,
		FUniformBufferRHIParamRef SkyLightUniformBuffer,
		FUnorderedAccessViewRHIParamRef OcclusionMaskUAV,
		FUnorderedAccessViewRHIParamRef RayDistanceUAV,
		uint32 Width, uint32 Height
	)
	{
		FRayTracingPipelineStateInitializer Initializer;
		Initializer.RayGenShaderRHI = GetRayTracingShader();

		FRHIRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer); // #dxr_todo: this should be done once at load-time and cached

		FRayTracingShaderBindingsWriter GlobalResources;
		GlobalResources.Set(TLASParameter, RayTracingScene.RayTracingSceneRHI->GetShaderResourceView());
		GlobalResources.Set(ViewParameter, ViewUniformBuffer);
		GlobalResources.Set(SceneTexturesParameter, SceneTexturesUniformBuffer);
		GlobalResources.Set(SkyLightParameter, SkyLightUniformBuffer);
		GlobalResources.Set(OcclusionMaskUAVParameter, OcclusionMaskUAV);
		GlobalResources.Set(RayDistanceUAVParameter, RayDistanceUAV);

		RHICmdList.RayTraceDispatch(Pipeline, GlobalResources, Width, Height);
	}

private:
	// Input
	FShaderResourceParameter TLASParameter;
	FShaderUniformBufferParameter ViewParameter;
	FShaderUniformBufferParameter SceneTexturesParameter;
	FShaderUniformBufferParameter SkyLightParameter;

	// Output
	FShaderResourceParameter OcclusionMaskUAVParameter;
	FShaderResourceParameter RayDistanceUAVParameter;
};

IMPLEMENT_SHADER_TYPE(, FSkylightRG, TEXT("/Engine/Private/Raytracing/RaytracingSkylightRGS.usf"), TEXT("SkyLightRGS"), SF_RayGen);

class FBuildMipTreeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildMipTreeCS, Global)

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

	FBuildMipTreeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TextureParameter.Bind(Initializer.ParameterMap, TEXT("Texture"));
		TextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("TextureSampler"));
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		FaceIndexParameter.Bind(Initializer.ParameterMap, TEXT("FaceIndex"));
		MipLevelParameter.Bind(Initializer.ParameterMap, TEXT("MipLevel"));
		MipTreeParameter.Bind(Initializer.ParameterMap, TEXT("MipTree"));
	}

	FBuildMipTreeCS() {}

	void SetParameters(
		FRHICommandList& RHICmdList,
		FTextureRHIRef Texture,
		const FIntVector& Dimensions,
		uint32 FaceIndex,
		uint32 MipLevel,
		FRWBuffer& MipTree)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		SetShaderValue(RHICmdList, ShaderRHI, FaceIndexParameter, FaceIndex);
		SetShaderValue(RHICmdList, ShaderRHI, MipLevelParameter, MipLevel);
		SetTextureParameter(RHICmdList, ShaderRHI, TextureParameter, TextureSamplerParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), Texture);

		check(MipTreeParameter.IsBound());
		MipTreeParameter.SetBuffer(RHICmdList, ShaderRHI, MipTree);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& MipTree,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		MipTreeParameter.UnsetUAV(RHICmdList, ShaderRHI);
		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, MipTree.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TextureParameter;
		Ar << TextureSamplerParameter;
		Ar << DimensionsParameter;
		Ar << FaceIndexParameter;
		Ar << MipLevelParameter;
		Ar << MipTreeParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter TextureParameter;
	FShaderResourceParameter TextureSamplerParameter;

	FShaderParameter DimensionsParameter;
	FShaderParameter FaceIndexParameter;
	FShaderParameter MipLevelParameter;
	FRWShaderParameter MipTreeParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildMipTreeCS, TEXT("/Engine/Private/Raytracing/BuildMipTreeCS.usf"), TEXT("BuildMipTreeCS"), SF_Compute)

void FDeferredShadingSceneRenderer::BuildSkyLightMipTree(
	FRHICommandListImmediate& RHICmdList,
	FTextureRHIRef SkyLightTexture,
	FRWBuffer& SkyLightMipTreePosX,
	FRWBuffer& SkyLightMipTreeNegX,
	FRWBuffer& SkyLightMipTreePosY,
	FRWBuffer& SkyLightMipTreeNegY,
	FRWBuffer& SkyLightMipTreePosZ,
	FRWBuffer& SkyLightMipTreeNegZ,
	FIntVector& SkyLightMipTreeDimensions
)
{
	SCOPED_DRAW_EVENT(RHICmdList, BuildSkyLightMipTree);
	SCOPED_GPU_STAT(RHICmdList, BuildSkyLightMipTree);

	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FBuildMipTreeCS> BuildSkyLightMipTreeComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(BuildSkyLightMipTreeComputeShader->GetComputeShader());

	FRWBuffer* MipTrees[] = {
		&SkyLightMipTreePosX,
		&SkyLightMipTreeNegX,
		&SkyLightMipTreePosY,
		&SkyLightMipTreeNegY,
		&SkyLightMipTreePosZ,
		&SkyLightMipTreeNegZ
	};

	// Allocate MIP tree
	FIntVector TextureSize = SkyLightTexture->GetSizeXYZ();
	uint32 MipLevelCount = FMath::Min(FMath::CeilLogTwo(TextureSize.X), FMath::CeilLogTwo(TextureSize.Y));
	SkyLightMipTreeDimensions = FIntVector(1 << MipLevelCount, 1 << MipLevelCount, 1);
	uint32 NumElements = SkyLightMipTreeDimensions.X * SkyLightMipTreeDimensions.Y;
	for (uint32 MipLevel = 1; MipLevel <= MipLevelCount; ++MipLevel)
	{
		uint32 NumElementsInLevel = (SkyLightMipTreeDimensions.X >> MipLevel) * (SkyLightMipTreeDimensions.Y >> MipLevel);
		NumElements += NumElementsInLevel;
	}

	for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		MipTrees[FaceIndex]->Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);

		// Execute hierarchical build
		for (uint32 MipLevel = 0; MipLevel <= MipLevelCount; ++MipLevel)
		{
			FComputeFenceRHIRef MipLevelFence = RHICmdList.CreateComputeFence(TEXT("SkyLightMipTree Build"));
			BuildSkyLightMipTreeComputeShader->SetParameters(RHICmdList, SkyLightTexture, SkyLightMipTreeDimensions, FaceIndex, MipLevel, *MipTrees[FaceIndex]);
			FIntVector MipLevelDimensions = FIntVector(SkyLightMipTreeDimensions.X >> MipLevel, SkyLightMipTreeDimensions.Y >> MipLevel, 1);
			FIntVector NumGroups = FIntVector::DivideAndRoundUp(MipLevelDimensions, FBuildMipTreeCS::GetGroupSize());
			DispatchComputeShader(RHICmdList, *BuildSkyLightMipTreeComputeShader, NumGroups.X, NumGroups.Y, 1);
			BuildSkyLightMipTreeComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, *MipTrees[FaceIndex], MipLevelFence);
		}
		FComputeFenceRHIRef TransitionFence = RHICmdList.CreateComputeFence(TEXT("SkyLightMipTree Transition"));
		BuildSkyLightMipTreeComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, *MipTrees[FaceIndex], TransitionFence);
	}
}

class FBuildSolidAnglePdfCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildSolidAnglePdfCS, Global)

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

	FBuildSolidAnglePdfCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MipLevelParameter.Bind(Initializer.ParameterMap, TEXT("MipLevel"));
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		SolidAnglePdfParameter.Bind(Initializer.ParameterMap, TEXT("SolidAnglePdf"));
	}

	FBuildSolidAnglePdfCS() {}

	void SetParameters(
		FRHICommandList& RHICmdList,
		uint32 MipLevel,
		const FIntVector& Dimensions,
		FRWBuffer& SolidAnglePdf
	)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, MipLevelParameter, MipLevel);
		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		check(SolidAnglePdfParameter.IsBound());
		SolidAnglePdfParameter.SetBuffer(RHICmdList, ShaderRHI, SolidAnglePdf);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& MipTreePdf,
		FComputeFenceRHIParamRef Fence
	)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SolidAnglePdfParameter.UnsetUAV(RHICmdList, ShaderRHI);
		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, MipTreePdf.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MipLevelParameter;
		Ar << DimensionsParameter;
		Ar << SolidAnglePdfParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter MipLevelParameter;
	FShaderParameter DimensionsParameter;
	FRWShaderParameter SolidAnglePdfParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildSolidAnglePdfCS, TEXT("/Engine/Private/Raytracing/BuildSolidAnglePdfCS.usf"), TEXT("BuildSolidAnglePdfCS"), SF_Compute)

void FDeferredShadingSceneRenderer::BuildSolidAnglePdf(
	FRHICommandListImmediate& RHICmdList,
	const FIntVector& Dimensions,
	FRWBuffer& SolidAnglePdf
)
{
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FBuildSolidAnglePdfCS> BuildSolidAnglePdfComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(BuildSolidAnglePdfComputeShader->GetComputeShader());

	uint32 NumElements = Dimensions.X * Dimensions.Y;
	uint32 MipLevelCount = FMath::Log2(Dimensions.X);
	for (uint32 MipLevel = 1; MipLevel <= MipLevelCount; ++MipLevel)
	{
		NumElements += (Dimensions.X >> MipLevel) * (Dimensions.Y >> MipLevel);
	}
	SolidAnglePdf.Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);

	for (uint32 MipLevel = 0; MipLevel <= MipLevelCount; ++MipLevel)
	{
		FComputeFenceRHIRef ComputeFence = RHICmdList.CreateComputeFence(TEXT("SkyLight SolidAnglePdf Build"));
		BuildSolidAnglePdfComputeShader->SetParameters(RHICmdList, MipLevel, Dimensions, SolidAnglePdf);
		FIntVector NumGroups = FIntVector::DivideAndRoundUp(Dimensions, FBuildSolidAnglePdfCS::GetGroupSize());
		DispatchComputeShader(RHICmdList, *BuildSolidAnglePdfComputeShader, NumGroups.X, NumGroups.Y, 1);
		BuildSolidAnglePdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, SolidAnglePdf, ComputeFence);
	}
}

class FBuildMipTreePdfCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildMipTreePdfCS, Global)

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

	FBuildMipTreePdfCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MipTreeParameter.Bind(Initializer.ParameterMap, TEXT("MipTree"));
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		MipLevelParameter.Bind(Initializer.ParameterMap, TEXT("MipLevel"));
		MipTreePdfParameter.Bind(Initializer.ParameterMap, TEXT("MipTreePdf"));
	}

	FBuildMipTreePdfCS() {}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FRWBuffer& MipTree,
		const FIntVector& Dimensions,
		uint32 MipLevel,
		FRWBuffer& MipTreePdf)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetSRVParameter(RHICmdList, ShaderRHI, MipTreeParameter, MipTree.SRV);
		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		SetShaderValue(RHICmdList, ShaderRHI, MipLevelParameter, MipLevel);

		check(MipTreePdfParameter.IsBound());
		MipTreePdfParameter.SetBuffer(RHICmdList, ShaderRHI, MipTreePdf);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& MipTreePdf,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		MipTreePdfParameter.UnsetUAV(RHICmdList, ShaderRHI);
		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, MipTreePdf.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MipTreeParameter;
		Ar << DimensionsParameter;
		Ar << MipLevelParameter;
		Ar << MipTreePdfParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter MipTreeParameter;
	FShaderParameter DimensionsParameter;
	FShaderParameter MipLevelParameter;

	FRWShaderParameter MipTreePdfParameter;
};

IMPLEMENT_SHADER_TYPE(, FBuildMipTreePdfCS, TEXT("/Engine/Private/Raytracing/BuildMipTreePdfCS.usf"), TEXT("BuildMipTreePdfCS"), SF_Compute)

void FDeferredShadingSceneRenderer::BuildSkyLightMipTreePdf(
	FRHICommandListImmediate& RHICmdList,
	const FRWBuffer& SkyLightMipTreePosX,
	const FRWBuffer& SkyLightMipTreeNegX,
	const FRWBuffer& SkyLightMipTreePosY,
	const FRWBuffer& SkyLightMipTreeNegY,
	const FRWBuffer& SkyLightMipTreePosZ,
	const FRWBuffer& SkyLightMipTreeNegZ,
	const FIntVector& SkyLightMipTreeDimensions,
	FRWBuffer& SkyLightMipTreePdfPosX,
	FRWBuffer& SkyLightMipTreePdfNegX,
	FRWBuffer& SkyLightMipTreePdfPosY,
	FRWBuffer& SkyLightMipTreePdfNegY,
	FRWBuffer& SkyLightMipTreePdfPosZ,
	FRWBuffer& SkyLightMipTreePdfNegZ
)
{
	const FRWBuffer* MipTrees[] = {
		&SkyLightMipTreePosX,
		&SkyLightMipTreeNegX,
		&SkyLightMipTreePosY,
		&SkyLightMipTreeNegY,
		&SkyLightMipTreePosZ,
		&SkyLightMipTreeNegZ
	};

	FRWBuffer* MipTreePdfs[] = {
		&SkyLightMipTreePdfPosX,
		&SkyLightMipTreePdfNegX,
		&SkyLightMipTreePdfPosY,
		&SkyLightMipTreePdfNegY,
		&SkyLightMipTreePdfPosZ,
		&SkyLightMipTreePdfNegZ
	};

	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FBuildMipTreePdfCS> BuildSkyLightMipTreePdfComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(BuildSkyLightMipTreePdfComputeShader->GetComputeShader());

	uint32 NumElements = SkyLightMipTreePosX.NumBytes / sizeof(float);
	uint32 MipLevelCount = FMath::Log2(SkyLightMipTreeDimensions.X);
	for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		MipTreePdfs[FaceIndex]->Initialize(sizeof(float), NumElements, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);

		// Execute hierarchical build
		for (uint32 MipLevel = 0; MipLevel <= MipLevelCount; ++MipLevel)
		{
			FComputeFenceRHIRef MipLevelFence = RHICmdList.CreateComputeFence(TEXT("SkyLightMipTree Build"));
			BuildSkyLightMipTreePdfComputeShader->SetParameters(RHICmdList, *MipTrees[FaceIndex], SkyLightMipTreeDimensions, MipLevel, *MipTreePdfs[FaceIndex]);
			FIntVector MipLevelDimensions = FIntVector(SkyLightMipTreeDimensions.X >> MipLevel, SkyLightMipTreeDimensions.Y >> MipLevel, 1);
			FIntVector NumGroups = FIntVector::DivideAndRoundUp(MipLevelDimensions, FBuildMipTreeCS::GetGroupSize());
			DispatchComputeShader(RHICmdList, *BuildSkyLightMipTreePdfComputeShader, NumGroups.X, NumGroups.Y, 1);
			//BuildSkyLightMipTreePdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, *MipTreePdfs[FaceIndex], MipLevelFence);
		}
		FComputeFenceRHIRef TransitionFence = RHICmdList.CreateComputeFence(TEXT("SkyLightMipTree Transition"));
		BuildSkyLightMipTreePdfComputeShader->UnsetParameters(RHICmdList, EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, *MipTreePdfs[FaceIndex], TransitionFence);
	}
}

class FVisualizeSkyLightMipTreePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeSkyLightMipTreePS, Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FVisualizeSkyLightMipTreePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DimensionsParameter.Bind(Initializer.ParameterMap, TEXT("Dimensions"));
		MipTreePosXParameter.Bind(Initializer.ParameterMap, TEXT("MipTreePosX"));
		MipTreeNegXParameter.Bind(Initializer.ParameterMap, TEXT("MipTreeNegX"));
		MipTreePosYParameter.Bind(Initializer.ParameterMap, TEXT("MipTreePosY"));
		MipTreeNegYParameter.Bind(Initializer.ParameterMap, TEXT("MipTreeNegY"));
		MipTreePosZParameter.Bind(Initializer.ParameterMap, TEXT("MipTreePosZ"));
		MipTreeNegZParameter.Bind(Initializer.ParameterMap, TEXT("MipTreeNegZ"));
	}

	FVisualizeSkyLightMipTreePS() {}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FIntVector Dimensions,
		const FRWBuffer& MipTreePosX,
		const FRWBuffer& MipTreeNegX,
		const FRWBuffer& MipTreePosY,
		const FRWBuffer& MipTreeNegY,
		const FRWBuffer& MipTreePosZ,
		const FRWBuffer& MipTreeNegZ)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, DimensionsParameter, Dimensions);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreePosXParameter, MipTreePosX.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreeNegXParameter, MipTreeNegX.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreePosYParameter, MipTreePosY.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreeNegYParameter, MipTreeNegY.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreePosZParameter, MipTreePosZ.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, MipTreeNegZParameter, MipTreeNegZ.SRV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DimensionsParameter;
		Ar << MipTreePosXParameter;
		Ar << MipTreeNegXParameter;
		Ar << MipTreePosYParameter;
		Ar << MipTreeNegYParameter;
		Ar << MipTreePosZParameter;
		Ar << MipTreeNegZParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter DimensionsParameter;
	FShaderResourceParameter MipTreePosXParameter;
	FShaderResourceParameter MipTreeNegXParameter;
	FShaderResourceParameter MipTreePosYParameter;
	FShaderResourceParameter MipTreeNegYParameter;
	FShaderResourceParameter MipTreePosZParameter;
	FShaderResourceParameter MipTreeNegZParameter;
};

IMPLEMENT_SHADER_TYPE(, FVisualizeSkyLightMipTreePS, TEXT("/Engine/Private/RayTracing/VisualizeSkyLightMipTreePS.usf"), TEXT("VisualizeSkyLightMipTreePS"), SF_Pixel)

void FDeferredShadingSceneRenderer::VisualizeSkyLightMipTree(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	FRWBuffer& SkyLightMipTreePosX,
	FRWBuffer& SkyLightMipTreeNegX,
	FRWBuffer& SkyLightMipTreePosY,
	FRWBuffer& SkyLightMipTreeNegY,
	FRWBuffer& SkyLightMipTreePosZ,
	FRWBuffer& SkyLightMipTreeNegZ,
	const FIntVector& SkyLightMipDimensions)
{
	// Allocate render target
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	TRefCountPtr<IPooledRenderTarget> SkyLightMipTreeRT;
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SkyLightMipTreeRT, TEXT("SkyLightMipTreeRT"));

	// Define shaders
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FVisualizeSkyLightMipTreePS> PixelShader(ShaderMap);
	FTextureRHIParamRef RenderTargets[2] =
	{
		SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture,
		SkyLightMipTreeRT->GetRenderTargetItem().TargetableTexture
	};
	SetRenderTargets(RHICmdList, 2, RenderTargets, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);

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

	// Transition to graphics
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyLightMipTreePosX.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyLightMipTreeNegX.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyLightMipTreePosY.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyLightMipTreeNegY.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyLightMipTreePosZ.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SkyLightMipTreeNegZ.UAV);

	// Draw
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	PixelShader->SetParameters(RHICmdList, View, SkyLightMipDimensions, SkyLightMipTreePosX, SkyLightMipTreeNegX, SkyLightMipTreePosY, SkyLightMipTreeNegY, SkyLightMipTreePosZ, SkyLightMipTreeNegZ);
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
	GVisualizeTexture.SetCheckPoint(RHICmdList, SkyLightMipTreeRT);

	// Transition to compute
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SkyLightMipTreePosX.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SkyLightMipTreeNegX.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SkyLightMipTreePosY.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SkyLightMipTreeNegY.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SkyLightMipTreePosZ.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SkyLightMipTreeNegZ.UAV);
}

void FDeferredShadingSceneRenderer::RenderRayTracingSkyLight(
	FRHICommandListImmediate& RHICmdList,
	TRefCountPtr<IPooledRenderTarget>& SkyLightRT,
	TRefCountPtr<IPooledRenderTarget>& HitDistanceRT
)
{
	check(Scene->SkyLight);
	check(Scene->SkyLight->ProcessedTexture);

	SCOPED_DRAW_EVENT(RHICmdList, RayTracingSkyLight);
	SCOPED_GPU_STAT(RHICmdList, RayTracingSkyLight);

	if (Scene->SkyLight->ShouldRebuildCdf())
	{
		BuildSkyLightMipTree(RHICmdList, Scene->SkyLight->ProcessedTexture->TextureRHI, Scene->SkyLight->SkyLightMipTreePosX, Scene->SkyLight->SkyLightMipTreeNegX, Scene->SkyLight->SkyLightMipTreePosY, Scene->SkyLight->SkyLightMipTreeNegY, Scene->SkyLight->SkyLightMipTreePosZ, Scene->SkyLight->SkyLightMipTreeNegZ, Scene->SkyLight->SkyLightMipDimensions);
		BuildSkyLightMipTreePdf(RHICmdList, Scene->SkyLight->SkyLightMipTreePosX, Scene->SkyLight->SkyLightMipTreeNegX, Scene->SkyLight->SkyLightMipTreePosY, Scene->SkyLight->SkyLightMipTreeNegY, Scene->SkyLight->SkyLightMipTreePosZ, Scene->SkyLight->SkyLightMipTreeNegZ, Scene->SkyLight->SkyLightMipDimensions,
			Scene->SkyLight->SkyLightMipTreePdfPosX, Scene->SkyLight->SkyLightMipTreePdfNegX, Scene->SkyLight->SkyLightMipTreePdfPosY, Scene->SkyLight->SkyLightMipTreePdfNegY, Scene->SkyLight->SkyLightMipTreePdfPosZ, Scene->SkyLight->SkyLightMipTreePdfNegZ);
		BuildSolidAnglePdf(RHICmdList, Scene->SkyLight->SkyLightMipDimensions, Scene->SkyLight->SolidAnglePdf);
		Scene->SkyLight->IsDirtyImportanceSamplingData = false;
	}
	//VisualizeSkyLightMipTree(RHICmdList, Views[0], Scene->SkyLight->SkyLightMipTreePosX, Scene->SkyLight->SkyLightMipTreeNegX, Scene->SkyLight->SkyLightMipTreePosY, Scene->SkyLight->SkyLightMipTreeNegY, Scene->SkyLight->SkyLightMipTreePosZ, Scene->SkyLight->SkyLightMipTreeNegZ, Scene->SkyLight->SkyLightMipDimensions);
	//VisualizeSkyLightMipTree(RHICmdList, Views[0], Scene->SkyLight->SkyLightMipTreePdfPosX, Scene->SkyLight->SkyLightMipTreePdfNegX, Scene->SkyLight->SkyLightMipTreePdfPosY, Scene->SkyLight->SkyLightMipTreePdfNegY, Scene->SkyLight->SkyLightMipTreePdfPosZ, Scene->SkyLight->SkyLightMipTreePdfNegZ, Scene->SkyLight->SkyLightMipDimensions);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_FloatRGBA;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SkyLightRT, TEXT("RayTracingSkylight"));
	ClearUAV(RHICmdList, SkyLightRT->GetRenderTargetItem(), FLinearColor::Black);

	Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_R16F;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, HitDistanceRT, TEXT("RayTracingSkyLightHitDistance"));
	ClearUAV(RHICmdList, HitDistanceRT->GetRenderTargetItem(), FLinearColor::Black);

	// Add SkyLight parameters to uniform buffer
	FSkyLightData SkyLightData;
	SkyLightData.SamplesPerPixel = GRayTracingSkyLightSamplesPerPixel;
	SkyLightData.SamplingStopLevel = GRayTracingSkyLightSamplingStopLevel;
	SkyLightData.Color = FVector(Scene->SkyLight->LightColor);
	SkyLightData.Texture = Scene->SkyLight->ProcessedTexture->TextureRHI;
	SkyLightData.TextureSampler = Scene->SkyLight->ProcessedTexture->SamplerStateRHI;
	SkyLightData.MipDimensions = Scene->SkyLight->SkyLightMipDimensions;
	SkyLightData.MipTreePosX = Scene->SkyLight->SkyLightMipTreePosX.SRV;
	SkyLightData.MipTreeNegX = Scene->SkyLight->SkyLightMipTreeNegX.SRV;
	SkyLightData.MipTreePosY = Scene->SkyLight->SkyLightMipTreePosY.SRV;
	SkyLightData.MipTreeNegY = Scene->SkyLight->SkyLightMipTreeNegY.SRV;
	SkyLightData.MipTreePosZ = Scene->SkyLight->SkyLightMipTreePosZ.SRV;
	SkyLightData.MipTreeNegZ = Scene->SkyLight->SkyLightMipTreeNegZ.SRV;

	SkyLightData.MipTreePdfPosX = Scene->SkyLight->SkyLightMipTreePdfPosX.SRV;
	SkyLightData.MipTreePdfNegX = Scene->SkyLight->SkyLightMipTreePdfNegX.SRV;
	SkyLightData.MipTreePdfPosY = Scene->SkyLight->SkyLightMipTreePdfPosY.SRV;
	SkyLightData.MipTreePdfNegY = Scene->SkyLight->SkyLightMipTreePdfNegY.SRV;
	SkyLightData.MipTreePdfPosZ = Scene->SkyLight->SkyLightMipTreePdfPosZ.SRV;
	SkyLightData.MipTreePdfNegZ = Scene->SkyLight->SkyLightMipTreePdfNegZ.SRV;
	SkyLightData.SolidAnglePdf = Scene->SkyLight->SolidAnglePdf.SRV;

	FUniformBufferRHIRef SkyLightUniformBuffer = RHICreateUniformBuffer(&SkyLightData, FSkyLightData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		FIntPoint ViewSize = View.ViewRect.Size();

		TShaderMapRef<FSkylightRG> SkyLightRayGenerationShader(GetGlobalShaderMap(FeatureLevel));
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		FUniformBufferRHIRef SceneTexturesUniformBuffer = RHICreateUniformBuffer(&SceneTextures, FSceneTexturesUniformParameters::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

		SkyLightRayGenerationShader->Dispatch(
			RHICmdList,
			View.PerViewRayTracingScene,
			View.ViewUniformBuffer,
			SceneTexturesUniformBuffer,
			SkyLightUniformBuffer,
			SkyLightRT->GetRenderTargetItem().UAV,
			HitDistanceRT->GetRenderTargetItem().UAV,
			ViewSize.X, ViewSize.Y
		);
	}

	// Transition to graphics pipeline
	FComputeFenceRHIRef Fence = RHICmdList.CreateComputeFence(TEXT("RayTracingSkyLight"));
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, SkyLightRT->GetRenderTargetItem().UAV, Fence);
	GVisualizeTexture.SetCheckPoint(RHICmdList, SkyLightRT);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, HitDistanceRT->GetRenderTargetItem().UAV);
}

class FCompositeSkyLightPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeSkyLightPS, Global);

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

	FCompositeSkyLightPS() {}
	virtual ~FCompositeSkyLightPS() {}

	FCompositeSkyLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		SkyLightTextureParameter.Bind(Initializer.ParameterMap, TEXT("SkyLightTexture"));
		SkyLightTextureSamplerParameter.Bind(Initializer.ParameterMap, TEXT("SkyLightTextureSampler"));
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		const FViewInfo& View,
		FTextureRHIParamRef SkyLightTexture,
		FTextureRHIParamRef HitDistanceTexture)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		SetTextureParameter(RHICmdList, ShaderRHI, SkyLightTextureParameter, SkyLightTextureSamplerParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), SkyLightTexture);
		// #dxr_todo: Use hit-distance texture for denoising
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SkyLightTextureParameter;
		Ar << SkyLightTextureSamplerParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter SkyLightTextureParameter;
	FShaderResourceParameter SkyLightTextureSamplerParameter;
};

IMPLEMENT_SHADER_TYPE(, FCompositeSkyLightPS, TEXT("/Engine/Private/RayTracing/CompositeSkyLightPS.usf"), TEXT("CompositeSkyLightPS"), SF_Pixel)

void FDeferredShadingSceneRenderer::CompositeRayTracingSkyLight(
	FRHICommandListImmediate& RHICmdList,
	TRefCountPtr<IPooledRenderTarget>& SkyLightRT,
	TRefCountPtr<IPooledRenderTarget>& HitDistanceRT
)
{
	check(SkyLightRT);

	// Define shaders
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FCompositeSkyLightPS> PixelShader(ShaderMap);

	// PSO definition
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		PixelShader->SetParameters(RHICmdList, View, SkyLightRT->GetRenderTargetItem().ShaderResourceTexture, HitDistanceRT->GetRenderTargetItem().ShaderResourceTexture);
		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*VertexShader
		);
	}

	ResolveSceneColor(RHICmdList);
	SceneContext.FinishRenderingSceneColor(RHICmdList);
}


#endif

#endif // RHI_RAYTRACING
