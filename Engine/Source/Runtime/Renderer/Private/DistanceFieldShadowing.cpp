// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldShadowing.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "ShadowRendering.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

int32 GDistanceFieldShadowing = 1;
FAutoConsoleVariableRef CVarDistanceFieldShadowing(
	TEXT("r.DistanceFieldShadowing"),
	GDistanceFieldShadowing,
	TEXT("Whether the distance field shadowing feature is allowed."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GDFShadowQuality = 2;
FAutoConsoleVariableRef CVarDFShadowQuality(
	TEXT("r.DFShadowQuality"),
	GDFShadowQuality,
	TEXT("Defines the distance field shadow method which allows to adjust for quality or performance.\n")
	TEXT(" 0:off, 1:medium (less samples, no SSS), 2:high (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GFullResolutionDFShadowing = 0;
FAutoConsoleVariableRef CVarFullResolutionDFShadowing(
	TEXT("r.DFFullResolution"),
	GFullResolutionDFShadowing,
	TEXT("1 = full resolution distance field shadowing, 0 = half resolution with bilateral upsample."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GShadowScatterTileCulling = 1;
FAutoConsoleVariableRef CVarShadowScatterTileCulling(
	TEXT("r.DFShadowScatterTileCulling"),
	GShadowScatterTileCulling,
	TEXT("Whether to use the rasterizer to scatter objects onto the tile grid for culling."),
	ECVF_RenderThreadSafe
	);

float GShadowCullTileWorldSize = 200.0f;
FAutoConsoleVariableRef CVarShadowCullTileWorldSize(
	TEXT("r.DFShadowCullTileWorldSize"),
	GShadowCullTileWorldSize,
	TEXT("World space size of a tile used for culling for directional lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GTwoSidedMeshDistanceBias = 4;
FAutoConsoleVariableRef CVarTwoSidedMeshDistanceBias(
	TEXT("r.DFTwoSidedMeshDistanceBias"),
	GTwoSidedMeshDistanceBias,
	TEXT("World space amount to expand distance field representations of two sided meshes.  This is useful to get tree shadows to match up with standard shadow mapping."),
	ECVF_RenderThreadSafe
	);

int32 GAverageObjectsPerShadowCullTile = 128;
FAutoConsoleVariableRef CVarAverageObjectsPerShadowCullTile(
	TEXT("r.DFShadowAverageObjectsPerCullTile"),
	GAverageObjectsPerShadowCullTile,
	TEXT("Determines how much memory should be allocated in distance field object culling data structures.  Too much = memory waste, too little = flickering due to buffer overflow."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

int32 const GDistanceFieldShadowTileSizeX = 8;
int32 const GDistanceFieldShadowTileSizeY = 8;

int32 GetDFShadowDownsampleFactor()
{
	return GFullResolutionDFShadowing ? 1 : GAODownsampleFactor;
}

FIntPoint GetBufferSizeForDFShadows()
{
	return FIntPoint::DivideAndRoundDown(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), GetDFShadowDownsampleFactor());
}

TGlobalResource<FDistanceFieldObjectBufferResource> GShadowCulledObjectBuffers;

class FCullObjectsForShadowCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsForShadowCS,Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCullObjectsForShadowCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		CulledObjectParameters.Bind(Initializer.ParameterMap);
		ObjectBoundingGeometryIndexCount.Bind(Initializer.ParameterMap, TEXT("ObjectBoundingGeometryIndexCount"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		NumShadowHullPlanes.Bind(Initializer.ParameterMap, TEXT("NumShadowHullPlanes"));
		ShadowBoundingSphere.Bind(Initializer.ParameterMap, TEXT("ShadowBoundingSphere"));
		ShadowConvexHull.Bind(Initializer.ParameterMap, TEXT("ShadowConvexHull"));
	}

	FCullObjectsForShadowCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, const FMatrix& WorldToShadowValue, int32 NumPlanes, const FPlane* PlaneData, const FVector4& ShadowBoundingSphereValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer);

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GShadowCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GShadowCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GShadowCulledObjectBuffers.Buffers.BoxBounds.UAV;		
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		CulledObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundingGeometryIndexCount, ARRAY_COUNT(GCubeIndices));
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowValue);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowBoundingSphere, ShadowBoundingSphereValue);

		if (NumPlanes <= 12)
		{
			SetShaderValue(RHICmdList, ShaderRHI, NumShadowHullPlanes, NumPlanes);
			SetShaderValueArray(RHICmdList, ShaderRHI, ShadowConvexHull, PlaneData, NumPlanes);
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, NumShadowHullPlanes, 0);
		}
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers));
		CulledObjectParameters.UnsetParameters(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GShadowCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GShadowCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GShadowCulledObjectBuffers.Buffers.BoxBounds.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectBufferParameters;
		Ar << CulledObjectParameters;
		Ar << ObjectBoundingGeometryIndexCount;
		Ar << WorldToShadow;
		Ar << NumShadowHullPlanes;
		Ar << ShadowBoundingSphere;
		Ar << ShadowConvexHull;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FDistanceFieldCulledObjectBufferParameters CulledObjectParameters;
	FShaderParameter ObjectBoundingGeometryIndexCount;
	FShaderParameter WorldToShadow;
	FShaderParameter NumShadowHullPlanes;
	FShaderParameter ShadowBoundingSphere;
	FShaderParameter ShadowConvexHull;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsForShadowCS,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("CullObjectsForShadowCS"),SF_Compute);

/**  */
class FShadowObjectCullVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowObjectCullVS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform); 
	}

	FShadowObjectCullVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		MinExpandRadius.Bind(Initializer.ParameterMap, TEXT("MinExpandRadius"));
	}

	FShadowObjectCullVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FVector2D NumGroupsValue, const FMatrix& WorldToShadowMatrixValue, float ShadowRadius)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		
		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		float MinExpandRadiusValue = 1.414f * ShadowRadius / FMath::Min(NumGroupsValue.X, NumGroupsValue.Y);
		SetShaderValue(RHICmdList, ShaderRHI, MinExpandRadius, MinExpandRadiusValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << WorldToShadow;
		Ar << MinExpandRadius;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderParameter WorldToShadow;
	FShaderParameter MinExpandRadius;
};

IMPLEMENT_SHADER_TYPE(,FShadowObjectCullVS,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("ShadowObjectCullVS"),SF_Vertex);

template <bool bCountingPass>
class FShadowObjectCullPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowObjectCullPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform) && RHISupportsPixelShaderUAVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters,OutEnvironment);
		FLightTileIntersectionParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SCATTER_CULLING_COUNT_PASS"), bCountingPass ? 1 : 0);
	}

	/** Default constructor. */
	FShadowObjectCullPS() {}

	/** Initialization constructor. */
	FShadowObjectCullPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		LightTileIntersectionParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);
		LightTileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
	}

	void GetUAVs(const FSceneView& View, FLightTileIntersectionResources* TileIntersectionResources, TArray<FUnorderedAccessViewRHIParamRef>& UAVs)
	{
		LightTileIntersectionParameters.GetUAVs(*TileIntersectionResources, UAVs);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << LightTileIntersectionParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FLightTileIntersectionParameters LightTileIntersectionParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,FShadowObjectCullPS<true>,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("ShadowObjectCullPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FShadowObjectCullPS<false>,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("ShadowObjectCullPS"),SF_Pixel);

enum EDistanceFieldShadowingType
{
	DFS_DirectionalLightScatterTileCulling,
	DFS_DirectionalLightTiledCulling,
	DFS_PointLightTiledCulling
};

template<EDistanceFieldShadowingType ShadowingType, uint32 DFShadowQuality>
class TDistanceFieldShadowingCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldShadowingCS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLightTileIntersectionParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldShadowTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldShadowTileSizeY);
		OutEnvironment.SetDefine(TEXT("SCATTER_TILE_CULLING"), ShadowingType == DFS_DirectionalLightScatterTileCulling);
		OutEnvironment.SetDefine(TEXT("POINT_LIGHT"), ShadowingType == DFS_PointLightTiledCulling);
		OutEnvironment.SetDefine(TEXT("DF_SHADOW_QUALITY"), DFShadowQuality);
	}

	/** Default constructor. */
	TDistanceFieldShadowingCS() {}

	/** Initialization constructor. */
	TDistanceFieldShadowingCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ShadowFactors.Bind(Initializer.ParameterMap, TEXT("ShadowFactors"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightSourceRadius.Bind(Initializer.ParameterMap, TEXT("LightSourceRadius"));
		RayStartOffsetDepthScale.Bind(Initializer.ParameterMap, TEXT("RayStartOffsetDepthScale"));
		LightPositionAndInvRadius.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		TanLightAngleAndNormalThreshold.Bind(Initializer.ParameterMap, TEXT("TanLightAngleAndNormalThreshold"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap, TEXT("ScissorRectMinAndSize"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		SceneTextureParameters.Bind(Initializer);
		LightTileIntersectionParameters.Bind(Initializer.ParameterMap);
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		TwoSidedMeshDistanceBias.Bind(Initializer.ParameterMap, TEXT("TwoSidedMeshDistanceBias"));
		MinDepth.Bind(Initializer.ParameterMap, TEXT("MinDepth"));
		MaxDepth.Bind(Initializer.ParameterMap, TEXT("MaxDepth"));
		DownsampleFactor.Bind(Initializer.ParameterMap, TEXT("DownsampleFactor"));
	}

	template<typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FProjectedShadowInfo* ProjectedShadowInfo,
		FSceneRenderTargetItem& ShadowFactorsValue, 
		FVector2D NumGroupsValue,
		const FIntRect& ScissorRect,
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ShadowFactorsValue.UAV);
		ShadowFactors.SetTexture(RHICmdList, ShaderRHI, ShadowFactorsValue.ShaderResourceTexture, ShadowFactorsValue.UAV);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);

		const FLightSceneProxy& LightProxy = *(ProjectedShadowInfo->GetLightSceneInfo().Proxy);

		FLightParameters LightParameters;

		LightProxy.GetParameters(LightParameters);

		SetShaderValue(RHICmdList, ShaderRHI, LightDirection, LightParameters.NormalizedLightDirection);
		SetShaderValue(RHICmdList, ShaderRHI, LightPositionAndInvRadius, LightParameters.LightPositionAndInvRadius);
		// Default light source radius of 0 gives poor results
		SetShaderValue(RHICmdList, ShaderRHI, LightSourceRadius, LightParameters.LightSourceRadius == 0 ? 20 : FMath::Clamp(LightParameters.LightSourceRadius, .001f, 1.0f / (4 * LightParameters.LightPositionAndInvRadius.W)));

		SetShaderValue(RHICmdList, ShaderRHI, RayStartOffsetDepthScale, LightProxy.GetRayStartOffsetDepthScale());

		const float LightSourceAngle = FMath::Clamp(LightProxy.GetLightSourceAngle(), 0.001f, 5.0f) * PI / 180.0f;
		const FVector TanLightAngleAndNormalThresholdValue(FMath::Tan(LightSourceAngle), FMath::Cos(PI / 2 + LightSourceAngle), LightProxy.GetTraceDistance());
		SetShaderValue(RHICmdList, ShaderRHI, TanLightAngleAndNormalThreshold, TanLightAngleAndNormalThresholdValue);

		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));

		check(TileIntersectionResources || !LightTileIntersectionParameters.IsBound());

		if (TileIntersectionResources)
		{
			LightTileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
		}

		FMatrix WorldToShadowMatrixValue = FTranslationMatrix(ProjectedShadowInfo->PreShadowTranslation) * ProjectedShadowInfo->SubjectAndReceiverMatrix;
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		SetShaderValue(RHICmdList, ShaderRHI, TwoSidedMeshDistanceBias, GTwoSidedMeshDistanceBias);

		if (ProjectedShadowInfo->bDirectionalLight)
		{
			SetShaderValue(RHICmdList, ShaderRHI, MinDepth, ProjectedShadowInfo->CascadeSettings.SplitNear - ProjectedShadowInfo->CascadeSettings.SplitNearFadeRegion);
			SetShaderValue(RHICmdList, ShaderRHI, MaxDepth, ProjectedShadowInfo->CascadeSettings.SplitFar);
		}
		else
		{
			//@todo - set these up for point lights as well
			SetShaderValue(RHICmdList, ShaderRHI, MinDepth, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, MaxDepth, HALF_WORLD_MAX);
		}

		SetShaderValue(RHICmdList, ShaderRHI, DownsampleFactor, GetDFShadowDownsampleFactor());
	}

	template<typename TRHICommandList>
	void UnsetParameters(TRHICommandList& RHICmdList, FSceneRenderTargetItem& ShadowFactorsValue)
	{
		ShadowFactors.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, ShadowFactorsValue.UAV);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowFactors;
		Ar << NumGroups;
		Ar << LightDirection;
		Ar << LightPositionAndInvRadius;
		Ar << LightSourceRadius;
		Ar << RayStartOffsetDepthScale;
		Ar << TanLightAngleAndNormalThreshold;
		Ar << ScissorRectMinAndSize;
		Ar << ObjectParameters;
		Ar << SceneTextureParameters;
		Ar << LightTileIntersectionParameters;
		Ar << WorldToShadow;
		Ar << TwoSidedMeshDistanceBias;
		Ar << MinDepth;
		Ar << MaxDepth;
		Ar << DownsampleFactor;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/DistanceFieldShadowing.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("DistanceFieldShadowingCS");
	}

private:

	FRWShaderParameter ShadowFactors;
	FShaderParameter NumGroups;
	FShaderParameter LightDirection;
	FShaderParameter LightPositionAndInvRadius;
	FShaderParameter LightSourceRadius;
	FShaderParameter RayStartOffsetDepthScale;
	FShaderParameter TanLightAngleAndNormalThreshold;
	FShaderParameter ScissorRectMinAndSize;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FSceneTextureShaderParameters SceneTextureParameters;
	FLightTileIntersectionParameters LightTileIntersectionParameters;
	FShaderParameter WorldToShadow;
	FShaderParameter TwoSidedMeshDistanceBias;
	FShaderParameter MinDepth;
	FShaderParameter MaxDepth;
	FShaderParameter DownsampleFactor;
};

// #define avoids a lot of code duplication
#define VARIATION(A) \
	typedef TDistanceFieldShadowingCS<A, 1> FDistanceFieldShadowingCS##A##1; \
	typedef TDistanceFieldShadowingCS<A, 2> FDistanceFieldShadowingCS##A##2; \
	IMPLEMENT_SHADER_TYPE2(FDistanceFieldShadowingCS##A##1, SF_Compute); \
	IMPLEMENT_SHADER_TYPE2(FDistanceFieldShadowingCS##A##2, SF_Compute);

VARIATION(DFS_DirectionalLightScatterTileCulling)
VARIATION(DFS_DirectionalLightTiledCulling)
VARIATION(DFS_PointLightTiledCulling)

#undef VARIATION

template<bool bUpsampleRequired>
class TDistanceFieldShadowingUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldShadowingUpsamplePS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("UPSAMPLE_REQUIRED"), bUpsampleRequired);
	}

	/** Default constructor. */
	TDistanceFieldShadowingUpsamplePS() {}

	/** Initialization constructor. */
	TDistanceFieldShadowingUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		ShadowFactorsTexture.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsTexture"));
		ShadowFactorsSampler.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsSampler"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap,TEXT("ScissorRectMinAndSize"));
		FadePlaneOffset.Bind(Initializer.ParameterMap,TEXT("FadePlaneOffset"));
		InvFadePlaneLength.Bind(Initializer.ParameterMap,TEXT("InvFadePlaneLength"));
		NearFadePlaneOffset.Bind(Initializer.ParameterMap,TEXT("NearFadePlaneOffset"));
		InvNearFadePlaneLength.Bind(Initializer.ParameterMap,TEXT("InvNearFadePlaneLength"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FIntRect& ScissorRect, TRefCountPtr<IPooledRenderTarget>& ShadowFactorsTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		SetTextureParameter(RHICmdList, ShaderRHI, ShadowFactorsTexture, ShadowFactorsSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), ShadowFactorsTextureValue->GetRenderTargetItem().ShaderResourceTexture);
	
		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));

		if (ShadowInfo->bDirectionalLight && ShadowInfo->CascadeSettings.FadePlaneLength > 0)
		{
			SetShaderValue(RHICmdList, ShaderRHI, FadePlaneOffset, ShadowInfo->CascadeSettings.FadePlaneOffset);
			SetShaderValue(RHICmdList, ShaderRHI, InvFadePlaneLength, 1.0f / FMath::Max(ShadowInfo->CascadeSettings.FadePlaneLength, .00001f));
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, FadePlaneOffset, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, InvFadePlaneLength, 0.0f);
		}

		if (ShadowInfo->bDirectionalLight && ShadowInfo->CascadeSettings.SplitNearFadeRegion > 0)
		{
			SetShaderValue(RHICmdList, ShaderRHI, NearFadePlaneOffset, ShadowInfo->CascadeSettings.SplitNear - ShadowInfo->CascadeSettings.SplitNearFadeRegion);
			SetShaderValue(RHICmdList, ShaderRHI, InvNearFadePlaneLength, 1.0f / FMath::Max(ShadowInfo->CascadeSettings.SplitNearFadeRegion, .00001f));
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, NearFadePlaneOffset, -1.0f);
			SetShaderValue(RHICmdList, ShaderRHI, InvNearFadePlaneLength, 1.0f);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << ShadowFactorsTexture;
		Ar << ShadowFactorsSampler;
		Ar << ScissorRectMinAndSize;
		Ar << FadePlaneOffset;
		Ar << InvFadePlaneLength;
		Ar << NearFadePlaneOffset;
		Ar << InvNearFadePlaneLength;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter ShadowFactorsTexture;
	FShaderResourceParameter ShadowFactorsSampler;
	FShaderParameter ScissorRectMinAndSize;
	FShaderParameter FadePlaneOffset;
	FShaderParameter InvFadePlaneLength;
	FShaderParameter NearFadePlaneOffset;
	FShaderParameter InvNearFadePlaneLength;
};

IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingUpsamplePS<true>,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("DistanceFieldShadowingUpsamplePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingUpsamplePS<false>,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("DistanceFieldShadowingUpsamplePS"),SF_Pixel);

const uint32 ComputeCulledObjectStartOffsetGroupSize = 8;

/**  */
class FComputeCulledObjectStartOffsetCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeCulledObjectStartOffsetCS,Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTE_START_OFFSET_GROUP_SIZE"), ComputeCulledObjectStartOffsetGroupSize);
	}

	FComputeCulledObjectStartOffsetCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TileIntersectionParameters.Bind(Initializer.ParameterMap);
	}

	FComputeCulledObjectStartOffsetCS()
	{
	}
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FLightTileIntersectionResources* TileIntersectionResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		TArray<FUnorderedAccessViewRHIParamRef> UAVs;
		TileIntersectionParameters.GetUAVs(*TileIntersectionResources, UAVs);

		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EComputeToCompute, UAVs.GetData(), UAVs.Num());

		TileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FLightTileIntersectionResources* TileIntersectionResources)
	{
		TileIntersectionParameters.UnsetParameters(RHICmdList, GetComputeShader());

		TArray<FUnorderedAccessViewRHIParamRef> UAVs;
		TileIntersectionParameters.GetUAVs(*TileIntersectionResources, UAVs);

		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, UAVs.GetData(), UAVs.Num());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TileIntersectionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FLightTileIntersectionParameters TileIntersectionParameters;
};

IMPLEMENT_SHADER_TYPE(,FComputeCulledObjectStartOffsetCS,TEXT("/Engine/Private/DistanceFieldShadowing.usf"),TEXT("ComputeCulledTilesStartOffsetCS"),SF_Compute);

template<bool bCountingPass>
void ScatterObjectsToShadowTiles(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View, 
	const FMatrix& WorldToShadowValue, 
	float ShadowBoundingRadius,
	FIntPoint LightTileDimensions, 
	FLightTileIntersectionResources* TileIntersectionResources)
{
	TShaderMapRef<FShadowObjectCullVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FShadowObjectCullPS<bCountingPass>> PixelShader(View.ShaderMap);

	TArray<FUnorderedAccessViewRHIParamRef> UAVs;
	PixelShader->GetUAVs(View, TileIntersectionResources, UAVs);
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, UAVs.GetData(), UAVs.Num());
	if (GRHIRequiresRenderTargetForPixelShaderUAVs)
	{
		TRefCountPtr<IPooledRenderTarget> Dummy;
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(LightTileDimensions, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Dummy, TEXT("Dummy"));
		FRHIRenderTargetView DummyRTView(Dummy->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ENoAction);
		RHICmdList.SetRenderTargets(1, &DummyRTView, NULL, UAVs.Num(), UAVs.GetData());
	}
	else
	{
		RHICmdList.SetRenderTargets(0, (const FRHIRenderTargetView *)NULL, NULL, UAVs.Num(), UAVs.GetData());
	}

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	RHICmdList.SetViewport(0, 0, 0.0f, LightTileDimensions.X, LightTileDimensions.Y, 1.0f);

	// Render backfaces since camera may intersect
	GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(RHICmdList, View, FVector2D(LightTileDimensions.X, LightTileDimensions.Y), WorldToShadowValue, ShadowBoundingRadius);
	PixelShader->SetParameters(RHICmdList, View, TileIntersectionResources);

	RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

	RHICmdList.DrawIndexedPrimitiveIndirect(
		PT_TriangleList,
		GetUnitCubeIndexBuffer(),
		GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.Buffer,
		0);

	SetRenderTarget(RHICmdList, NULL, NULL);
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, UAVs.GetData(), UAVs.Num());
}

void CullDistanceFieldObjectsForLight(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FLightSceneProxy* LightSceneProxy, 
	const FMatrix& WorldToShadowValue, 
	int32 NumPlanes, 
	const FPlane* PlaneData, 
	const FVector4& ShadowBoundingSphereValue,
	float ShadowBoundingRadius,
	TUniquePtr<FLightTileIntersectionResources>& TileIntersectionResources)
{
	const FScene* Scene = (const FScene*)(View.Family->Scene);

	SCOPED_DRAW_EVENT(RHICmdList, CullObjectsForLight);

	{
		if (!GShadowCulledObjectBuffers.IsInitialized()  
			|| GShadowCulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer
			|| GShadowCulledObjectBuffers.Buffers.MaxObjects > 3 * Scene->DistanceFieldSceneData.NumObjectsInBuffer
			|| GFastVRamConfig.bDirty)
		{
			GShadowCulledObjectBuffers.Buffers.bWantBoxBounds = true;
			GShadowCulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
			GShadowCulledObjectBuffers.ReleaseResource();
			GShadowCulledObjectBuffers.InitResource();
		}
		GShadowCulledObjectBuffers.Buffers.AcquireTransientResource();

		{
			SCOPED_DRAW_EVENTF(RHICmdList, CullObjectsToFrustum, TEXT("CullObjectsToFrustum SceneObjects %u"), Scene->DistanceFieldSceneData.NumObjectsInBuffer);

			ClearUAV(RHICmdList, GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments, 0);

			TShaderMapRef<FCullObjectsForShadowCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, Scene, View, WorldToShadowValue, NumPlanes, PlaneData, ShadowBoundingSphereValue);

			DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
			ComputeShader->UnsetParameters(RHICmdList, Scene);
		}
	}

	// Allocate tile resolution based on world space size
	const float LightTiles = FMath::Min(ShadowBoundingRadius / GShadowCullTileWorldSize + 1.0f, 256.0f);
	FIntPoint LightTileDimensions(LightTiles, LightTiles);

	if (LightSceneProxy->GetLightType() == LightType_Directional && GShadowScatterTileCulling)
	{
		const bool b16BitObjectIndices = Scene->DistanceFieldSceneData.CanUse16BitObjectIndices();

		if (!TileIntersectionResources || TileIntersectionResources->TileDimensions != LightTileDimensions || TileIntersectionResources->b16BitIndices != b16BitObjectIndices)
		{
			if (TileIntersectionResources)
			{
				TileIntersectionResources->Release();
			}
			else
			{
				TileIntersectionResources = MakeUnique<FLightTileIntersectionResources>();
			}

			TileIntersectionResources->TileDimensions = LightTileDimensions;
			TileIntersectionResources->b16BitIndices = b16BitObjectIndices;

			TileIntersectionResources->Initialize();
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, ComputeTileStartOffsets);

			// Start at 0 tiles per object
			ClearUAV(RHICmdList, TileIntersectionResources->TileNumCulledObjects, 0);

			// Rasterize object bounding shapes and intersect with shadow tiles to compute how many objects intersect each tile
			ScatterObjectsToShadowTiles<true>(RHICmdList, View, WorldToShadowValue, ShadowBoundingRadius, LightTileDimensions, TileIntersectionResources.Get());

			ClearUAV(RHICmdList, TileIntersectionResources->NextStartOffset, 0);

			uint32 GroupSizeX = FMath::DivideAndRoundUp<int32>(LightTileDimensions.X, ComputeCulledObjectStartOffsetGroupSize);
			uint32 GroupSizeY = FMath::DivideAndRoundUp<int32>(LightTileDimensions.Y, ComputeCulledObjectStartOffsetGroupSize);

			// Compute the start offset for each tile's culled object data
			TShaderMapRef<FComputeCulledObjectStartOffsetCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, TileIntersectionResources.Get());
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, View, TileIntersectionResources.Get());
		}

		{
			SCOPED_DRAW_EVENTF(RHICmdList, CullObjectsToTiles, TEXT("CullObjectsToTiles %ux%u"), LightTileDimensions.X, LightTileDimensions.Y);

			// Start at 0 tiles per object
			ClearUAV(RHICmdList, TileIntersectionResources->TileNumCulledObjects, 0);

			// Rasterize object bounding shapes and intersect with shadow tiles, and write out intersecting tile indices for the cone tracing pass
			ScatterObjectsToShadowTiles<false>(RHICmdList, View, WorldToShadowValue, ShadowBoundingRadius, LightTileDimensions, TileIntersectionResources.Get());
		}
	}
}

int32 GetDFShadowQuality()
{
	return FMath::Clamp(GDFShadowQuality, 0, 2);
}

bool SupportsDistanceFieldShadows(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GDistanceFieldShadowing 
		&& GetDFShadowQuality() > 0
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldShadowing(ShaderPlatform);
}

bool FDeferredShadingSceneRenderer::ShouldPrepareForDistanceFieldShadows() const 
{
	bool bSceneHasRayTracedDFShadows = false;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent())
		{
			const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
			{
				const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

				if (ProjectedShadowInfo->bRayTracedDistanceField)
				{
					bSceneHasRayTracedDFShadows = true;
					break;
				}
			}
		}
	}

	return ViewFamily.EngineShowFlags.DynamicShadows 
		&& bSceneHasRayTracedDFShadows
		&& SupportsDistanceFieldShadows(Scene->GetFeatureLevel(), Scene->GetShaderPlatform());
}

template<typename TRHICommandList, EDistanceFieldShadowingType DFSType, uint32 DFSQuality>
void RayTraceShadowsDispatch(TRHICommandList& RHICmdList, const FViewInfo& View, FProjectedShadowInfo* ProjectedShadowInfo, FLightTileIntersectionResources* TileIntersectionResources)
{
	FIntRect ScissorRect;
	if (!ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetScissorRect(ScissorRect, View, View.ViewRect))
	{
		ScissorRect = View.ViewRect;
	}

	uint32 GroupSizeX = FMath::DivideAndRoundUp(ScissorRect.Size().X / GetDFShadowDownsampleFactor(), GDistanceFieldShadowTileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(ScissorRect.Size().Y / GetDFShadowDownsampleFactor(), GDistanceFieldShadowTileSizeY);

	TShaderMapRef<TDistanceFieldShadowingCS<DFSType, DFSQuality> > ComputeShader(View.ShaderMap);
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
	FSceneRenderTargetItem& RayTracedShadowsRTI = ProjectedShadowInfo->RayTracedShadowsRT->GetRenderTargetItem();
	ComputeShader->SetParameters(RHICmdList, View, ProjectedShadowInfo, RayTracedShadowsRTI, FVector2D(GroupSizeX, GroupSizeY), ScissorRect, TileIntersectionResources);
	DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
	ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
}

template<typename TRHICommandList>
void RayTraceShadows(TRHICommandList& RHICmdList, const FViewInfo& View, FProjectedShadowInfo* ProjectedShadowInfo, FLightTileIntersectionResources* TileIntersectionResources)
{
	int32 const DFShadowQuality = GetDFShadowQuality();
	if (ProjectedShadowInfo->bDirectionalLight && GShadowScatterTileCulling)
	{
		if (DFShadowQuality == 1)
		{
			RayTraceShadowsDispatch<TRHICommandList, DFS_DirectionalLightScatterTileCulling, 1>(RHICmdList, View, ProjectedShadowInfo, TileIntersectionResources);
		}
		else
		{
			RayTraceShadowsDispatch<TRHICommandList, DFS_DirectionalLightScatterTileCulling, 2>(RHICmdList, View, ProjectedShadowInfo, TileIntersectionResources);
		}
	}
	else if (ProjectedShadowInfo->bDirectionalLight)
	{
		if (DFShadowQuality == 1)
		{
			RayTraceShadowsDispatch<TRHICommandList, DFS_DirectionalLightTiledCulling, 1>(RHICmdList, View, ProjectedShadowInfo, TileIntersectionResources);
		}
		else
		{
			RayTraceShadowsDispatch<TRHICommandList, DFS_DirectionalLightTiledCulling, 2>(RHICmdList, View, ProjectedShadowInfo, TileIntersectionResources);
		}
	}
	else
	{
		if (DFShadowQuality == 1)
		{
			RayTraceShadowsDispatch<TRHICommandList, DFS_PointLightTiledCulling, 1>(RHICmdList, View, ProjectedShadowInfo, TileIntersectionResources);
		}
		else
		{
			RayTraceShadowsDispatch<TRHICommandList, DFS_PointLightTiledCulling, 2>(RHICmdList, View, ProjectedShadowInfo, TileIntersectionResources);
		}
	}
}

void FProjectedShadowInfo::BeginRenderRayTracedDistanceFieldProjection(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (SupportsDistanceFieldShadows(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& View.Family->EngineShowFlags.RayTracedDistanceFieldShadows)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_BeginRenderRayTracedDistanceFieldShadows);
		SCOPED_DRAW_EVENT(RHICmdList, BeginRayTracedDistanceFieldShadow);

		const FScene* Scene = (const FScene*)(View.Family->Scene);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());

			SetRenderTarget(RHICmdList, NULL, NULL);

			int32 NumPlanes = 0;
			const FPlane* PlaneData = NULL;
			FVector4 ShadowBoundingSphereValue(0, 0, 0, 0);

			if (bDirectionalLight)
			{
				NumPlanes = CascadeSettings.ShadowBoundsAccurate.Planes.Num();
				PlaneData = CascadeSettings.ShadowBoundsAccurate.Planes.GetData();
			}
			else if (bOnePassPointLightShadow)
			{
				ShadowBoundingSphereValue = FVector4(ShadowBounds.Center.X, ShadowBounds.Center.Y, ShadowBounds.Center.Z, ShadowBounds.W);
			}
			else
			{
				NumPlanes = CasterFrustum.Planes.Num();
				PlaneData = CasterFrustum.Planes.GetData();
				ShadowBoundingSphereValue = FVector4(PreShadowTranslation, 0);
			}

			const FMatrix WorldToShadowValue = FTranslationMatrix(PreShadowTranslation) * SubjectAndReceiverMatrix;

			CullDistanceFieldObjectsForLight(
				RHICmdList,
				View,
				LightSceneInfo->Proxy,
				WorldToShadowValue,
				NumPlanes,
				PlaneData,
				ShadowBoundingSphereValue,
				ShadowBounds.W,
				LightSceneInfo->TileIntersectionResources
				);

			// Note: using the same TileIntersectionResources for multiple views, breaks splitscreen / stereo
			FLightTileIntersectionResources* TileIntersectionResources = LightSceneInfo->TileIntersectionResources.Get();

			View.HeightfieldLightingViewInfo.ComputeRayTracedShadowing(View, RHICmdList, this, TileIntersectionResources, GShadowCulledObjectBuffers);

			{
				const FIntPoint BufferSize = GetBufferSizeForDFShadows();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G16R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				Desc.Flags |= GFastVRamConfig.DistanceFieldShadows;
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayTracedShadowsRT, TEXT("RayTracedShadows"));
			}

			SCOPED_DRAW_EVENT(RHICmdList, RayTraceShadows);
			SetRenderTarget(RHICmdList, NULL, NULL);

			RayTraceShadows(RHICmdList, View, this, TileIntersectionResources);
		}
	}
}

void FProjectedShadowInfo::RenderRayTracedDistanceFieldProjection(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, IPooledRenderTarget* ScreenShadowMaskTexture, bool bProjectingForForwardShading) 
{
	BeginRenderRayTracedDistanceFieldProjection(RHICmdList, View);

	if (RayTracedShadowsRT)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderRayTracedDistanceFieldShadows);
		SCOPED_DRAW_EVENT(RHICmdList, RayTracedDistanceFieldShadow);

		FIntRect ScissorRect;

		if (!LightSceneInfo->Proxy->GetScissorRect(ScissorRect, View, View.ViewRect))
		{
			ScissorRect = View.ViewRect;
		}

		if ( IsTransientResourceBufferAliasingEnabled() )
		{
			GShadowCulledObjectBuffers.Buffers.DiscardTransientResource();
		}

		{
			SetRenderTarget(RHICmdList, ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, FSceneRenderTargets::Get(RHICmdList).GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

			SCOPED_DRAW_EVENT(RHICmdList, Upsample);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(ScissorRect.Min.X, ScissorRect.Min.Y, 0.0f, ScissorRect.Max.X, ScissorRect.Max.Y, 1.0f);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				
			SetBlendStateForProjection(GraphicsPSOInit, bProjectingForForwardShading, false);

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.bDepthBounds = bDirectionalLight;

			if (GFullResolutionDFShadowing)
			{
				TShaderMapRef<TDistanceFieldShadowingUpsamplePS<false> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
				PixelShader->SetParameters(RHICmdList, View, this, ScissorRect, RayTracedShadowsRT);
			}
			else
			{
				TShaderMapRef<TDistanceFieldShadowingUpsamplePS<true> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
				PixelShader->SetParameters(RHICmdList, View, this, ScissorRect, RayTracedShadowsRT);
			}

			//@todo - depth bounds test for local lights
			if (bDirectionalLight)
			{
				SetDepthBoundsTest(RHICmdList, CascadeSettings.SplitNear - CascadeSettings.SplitNearFadeRegion, CascadeSettings.SplitFar, View.ViewMatrices.GetProjectionMatrix());
			}

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				ScissorRect.Width(), ScissorRect.Height(),
				ScissorRect.Min.X / GetDFShadowDownsampleFactor(), ScissorRect.Min.Y / GetDFShadowDownsampleFactor(), 
				ScissorRect.Width() / GetDFShadowDownsampleFactor(), ScissorRect.Height() / GetDFShadowDownsampleFactor(),
				FIntPoint(ScissorRect.Width(), ScissorRect.Height()),
				GetBufferSizeForDFShadows(),
				*VertexShader);
		}

		RayTracedShadowsRT = NULL;
		RayTracedShadowsEndFence = NULL;
	}
}
