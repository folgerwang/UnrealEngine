// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
VolumetricFog.cpp
=============================================================================*/

#include "VolumetricFog.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "GlobalDistanceField.h"
#include "GlobalDistanceFieldParameters.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "DistanceFieldLightingShared.h"
#include "VolumetricFogShared.h"
#include "VolumeRendering.h"
#include "ScreenRendering.h"
#include "VolumeLighting.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"

int32 GVolumetricFog = 1;
FAutoConsoleVariableRef CVarVolumetricFog(
	TEXT("r.VolumetricFog"),
	GVolumetricFog,
	TEXT("Whether to allow the volumetric fog feature."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogInjectShadowedLightsSeparately = 1;
FAutoConsoleVariableRef CVarVolumetricFogInjectShadowedLightsSeparately(
	TEXT("r.VolumetricFog.InjectShadowedLightsSeparately"),
	GVolumetricFogInjectShadowedLightsSeparately,
	TEXT("Whether to allow the volumetric fog feature."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GVolumetricFogDepthDistributionScale = 32.0f;
FAutoConsoleVariableRef CVarVolumetricFogDepthDistributionScale(
	TEXT("r.VolumetricFog.DepthDistributionScale"),
	GVolumetricFogDepthDistributionScale,
	TEXT("Scales the slice depth distribution."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogGridPixelSize = 16;
FAutoConsoleVariableRef CVarVolumetricFogGridPixelSize(
	TEXT("r.VolumetricFog.GridPixelSize"),
	GVolumetricFogGridPixelSize,
	TEXT("XY Size of a cell in the voxel grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogGridSizeZ = 64;
FAutoConsoleVariableRef CVarVolumetricFogGridSizeZ(
	TEXT("r.VolumetricFog.GridSizeZ"),
	GVolumetricFogGridSizeZ,
	TEXT("How many Volumetric Fog cells to use in z."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogTemporalReprojection = 1;
FAutoConsoleVariableRef CVarVolumetricFogTemporalReprojection(
	TEXT("r.VolumetricFog.TemporalReprojection"),
	GVolumetricFogTemporalReprojection,
	TEXT("Whether to use temporal reprojection on volumetric fog."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogJitter = 1;
FAutoConsoleVariableRef CVarVolumetricFogJitter(
	TEXT("r.VolumetricFog.Jitter"),
	GVolumetricFogJitter,
	TEXT("Whether to apply jitter to each frame's volumetric fog computation, achieving temporal super sampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GVolumetricFogHistoryWeight = .9f;
FAutoConsoleVariableRef CVarVolumetricFogHistoryWeight(
	TEXT("r.VolumetricFog.HistoryWeight"),
	GVolumetricFogHistoryWeight,
	TEXT("How much the history value should be weighted each frame.  This is a tradeoff between visible jittering and responsiveness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogHistoryMissSupersampleCount = 4;
FAutoConsoleVariableRef CVarVolumetricFogHistoryMissSupersampleCount(
	TEXT("r.VolumetricFog.HistoryMissSupersampleCount"),
	GVolumetricFogHistoryMissSupersampleCount,
	TEXT("Number of lighting samples to compute for voxels whose history value is not available.\n")
	TEXT("This reduces noise when panning or on camera cuts, but introduces a variable cost to volumetric fog computation.  Valid range [1, 16]."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GInverseSquaredLightDistanceBiasScale = 1.0f;
FAutoConsoleVariableRef CVarInverseSquaredLightDistanceBiasScale(
	TEXT("r.VolumetricFog.InverseSquaredLightDistanceBiasScale"),
	GInverseSquaredLightDistanceBiasScale,
	TEXT("Scales the amount added to the inverse squared falloff denominator.  This effectively removes the spike from inverse squared falloff that causes extreme aliasing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FVolumetricFogGlobalData, "VolumetricFog");

DECLARE_GPU_STAT(VolumetricFog);

FVolumetricFogGlobalData::FVolumetricFogGlobalData()
{}

float TemporalHalton(int32 Index, int32 Base)
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while (Index > 0)
	{
		Result += (Index % Base) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

FVector VolumetricFogTemporalRandom(uint32 FrameNumber)
{
	// Center of the voxel
	FVector RandomOffsetValue(.5f, .5f, .5f);

	if (GVolumetricFogJitter && GVolumetricFogTemporalReprojection)
	{
		RandomOffsetValue = FVector(TemporalHalton(FrameNumber & 1023, 2), TemporalHalton(FrameNumber & 1023, 3), TemporalHalton(FrameNumber & 1023, 5));
	}

	return RandomOffsetValue;
}

uint32 VolumetricFogGridInjectionGroupSize = 4;

class FVolumetricFogMaterialSetupCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogMaterialSetupCS, Global)
	//SHADER_USE_PARAMETER_STRUCT(FVolumetricFogMaterialSetupCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FLinearColor, GlobalAlbedo)
		SHADER_PARAMETER(FLinearColor, GlobalEmissive)
		SHADER_PARAMETER(float, GlobalExtinctionScale)

		SHADER_PARAMETER_STRUCT_REF(FFogUniformParameters, FogUniformParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWVBufferA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWVBufferB)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVolumetricFog(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogGridInjectionGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	FVolumetricFogMaterialSetupCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(this, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());

		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FVolumetricFogMaterialSetupCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FVolumetricFogIntegrationParameterData& IntegrationData)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(, FVolumetricFogMaterialSetupCS, TEXT("/Engine/Private/VolumetricFog.usf"), TEXT("MaterialSetupCS"), SF_Compute);

/** Vertex shader used to write to a range of slices of a 3d volume texture. */
class FWriteToBoundingSphereVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWriteToBoundingSphereVS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVolumetricFog(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_VertexToGeometryShader);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	FWriteToBoundingSphereVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
		ViewSpaceBoundingSphere.Bind(Initializer.ParameterMap, TEXT("ViewSpaceBoundingSphere"));
		ViewToVolumeClip.Bind(Initializer.ParameterMap, TEXT("ViewToVolumeClip"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FWriteToBoundingSphereVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FVolumetricFogIntegrationParameterData& IntegrationData, const FSphere& BoundingSphere, int32 MinZValue)
	{
		SetShaderValue(RHICmdList, GetVertexShader(), MinZ, MinZValue);

		const FVector ViewSpaceBoundingSphereCenter = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center);
		SetShaderValue(RHICmdList, GetVertexShader(), ViewSpaceBoundingSphere, FVector4(ViewSpaceBoundingSphereCenter, BoundingSphere.W));

		const FMatrix ProjectionMatrix = View.ViewMatrices.ComputeProjectionNoAAMatrix();
		SetShaderValue(RHICmdList, GetVertexShader(), ViewToVolumeClip, ProjectionMatrix);

		VolumetricFogParameters.Set(RHICmdList, GetVertexShader(), View, IntegrationData);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MinZ;
		Ar << ViewSpaceBoundingSphere;
		Ar << ViewToVolumeClip;
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter MinZ;
	FShaderParameter ViewSpaceBoundingSphere;
	FShaderParameter ViewToVolumeClip;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(, FWriteToBoundingSphereVS, TEXT("/Engine/Private/VolumetricFog.usf"), TEXT("WriteToBoundingSphereVS"), SF_Vertex);

/** Shader that adds direct lighting contribution from the given light to the current volume lighting cascade. */
class TInjectShadowedLocalLightPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(TInjectShadowedLocalLightPS);

	class FDynamicallyShadowed	: SHADER_PERMUTATION_BOOL("DYNAMICALLY_SHADOWED");
	class FInverseSquared		: SHADER_PERMUTATION_BOOL("INVERSE_SQUARED_FALLOFF");
	class FTemporalReprojection : SHADER_PERMUTATION_BOOL("USE_TEMPORAL_REPROJECTION");

	using FPermutationDomain = TShaderPermutationDomain<
		FDynamicallyShadowed,
		FInverseSquared,
		FTemporalReprojection >;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVolumetricFog(Parameters.Platform);
	}

	TInjectShadowedLocalLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		PhaseG.Bind(Initializer.ParameterMap, TEXT("PhaseG"));
		InverseSquaredLightDistanceBiasScale.Bind(Initializer.ParameterMap, TEXT("InverseSquaredLightDistanceBiasScale"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		VolumeShadowingParameters.Bind(Initializer.ParameterMap);
	}

	TInjectShadowedLocalLightPS() {}

public:
	// @param InnerSplitIndex which CSM shadow map level, INDEX_NONE if no directional light
	// @param VolumeCascadeIndexValue which volume we render to
	void SetParameters(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FVolumetricFogIntegrationParameterData& IntegrationData,
		const FLightSceneInfo* LightSceneInfo,
		const FExponentialHeightFogSceneInfo& FogInfo,
		const FProjectedShadowInfo* ShadowMap,
		bool bDynamicallyShadowed)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);

		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);

		SetShaderValue(RHICmdList, ShaderRHI, PhaseG, FogInfo.VolumetricFogScatteringDistribution);
		SetShaderValue(RHICmdList, ShaderRHI, InverseSquaredLightDistanceBiasScale, GInverseSquaredLightDistanceBiasScale);

		VolumeShadowingParameters.Set(RHICmdList, ShaderRHI, View, LightSceneInfo, ShadowMap, 0, bDynamicallyShadowed);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PhaseG;
		Ar << InverseSquaredLightDistanceBiasScale;
		Ar << VolumetricFogParameters;
		Ar << VolumeShadowingParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter PhaseG;
	FShaderParameter InverseSquaredLightDistanceBiasScale;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FVolumeShadowingParameters VolumeShadowingParameters;
};

IMPLEMENT_GLOBAL_SHADER(TInjectShadowedLocalLightPS, "/Engine/Private/VolumetricFog.usf", "InjectShadowedLocalLightPS", SF_Pixel);

FProjectedShadowInfo* GetShadowForInjectionIntoVolumetricFog(const FLightSceneProxy* LightProxy, FVisibleLightInfo& VisibleLightInfo)
{
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

		if (ProjectedShadowInfo->bAllocated
			&& ProjectedShadowInfo->bWholeSceneShadow
			&& !ProjectedShadowInfo->bRayTracedDistanceField)
		{
			return ProjectedShadowInfo;
		}
	}

	return NULL;
}

bool LightNeedsSeparateInjectionIntoVolumetricFog(const FLightSceneInfo* LightSceneInfo, FVisibleLightInfo& VisibleLightInfo)
{
	const FLightSceneProxy* LightProxy = LightSceneInfo->Proxy;

	if (GVolumetricFogInjectShadowedLightsSeparately
		&& (LightProxy->GetLightType() == LightType_Point || LightProxy->GetLightType() == LightType_Spot)
		&& !LightProxy->HasStaticLighting()
		&& LightProxy->CastsDynamicShadow()
		&& LightProxy->CastsVolumetricShadow())
	{
		const FStaticShadowDepthMap* StaticShadowDepthMap = LightProxy->GetStaticShadowDepthMap();
		const bool bStaticallyShadowed = LightSceneInfo->IsPrecomputedLightingValid() && StaticShadowDepthMap && StaticShadowDepthMap->Data && StaticShadowDepthMap->TextureRHI;

		return GetShadowForInjectionIntoVolumetricFog(LightProxy, VisibleLightInfo) != NULL || bStaticallyShadowed;
	}

	return false;
}

FIntPoint CalculateVolumetricFogBoundsForLight(const FSphere& LightBounds, const FViewInfo& View, FIntVector VolumetricFogGridSize, FVector GridZParams)
{
	FIntPoint VolumeZBounds;

	FVector ViewSpaceLightBoundsOrigin = View.ViewMatrices.GetViewMatrix().TransformPosition(LightBounds.Center);

	int32 FurthestSliceIndexUnclamped = ComputeZSliceFromDepth(ViewSpaceLightBoundsOrigin.Z + LightBounds.W, GridZParams);
	int32 ClosestSliceIndexUnclamped = ComputeZSliceFromDepth(ViewSpaceLightBoundsOrigin.Z - LightBounds.W, GridZParams);

	VolumeZBounds.X = FMath::Clamp(ClosestSliceIndexUnclamped, 0, VolumetricFogGridSize.Z - 1);
	VolumeZBounds.Y = FMath::Clamp(FurthestSliceIndexUnclamped, 0, VolumetricFogGridSize.Z - 1);

	return VolumeZBounds;
}

/**  */
class FCircleRasterizeVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		const int32 NumTriangles = NumVertices - 2;
		const uint32 Size = NumVertices * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		const int32 NumRings = NumVertices;
		const float RadiansPerRingSegment = PI / (float)NumRings;

		// Boost the effective radius so that the edges of the circle approximation lie on the circle, instead of the vertices
		const float RadiusScale = 1.0f / FMath::Cos(RadiansPerRingSegment);

		for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
		{
			float Angle = VertexIndex / (float)(NumVertices - 1) * 2 * PI;
			// WriteToBoundingSphereVS only uses UV
			DestVertex[VertexIndex].Position = FVector2D(0, 0);
			DestVertex[VertexIndex].UV = FVector2D(RadiusScale * FMath::Cos(Angle) * .5f + .5f, RadiusScale * FMath::Sin(Angle) * .5f + .5f);
		}

		RHIUnlockVertexBuffer(VertexBufferRHI);
	}

	static int32 NumVertices;
};

int32 FCircleRasterizeVertexBuffer::NumVertices = 8;

TGlobalResource<FCircleRasterizeVertexBuffer> GCircleRasterizeVertexBuffer;

/**  */
class FCircleRasterizeIndexBuffer : public FIndexBuffer
{
public:

	virtual void InitRHI() override
	{
		const int32 NumTriangles = FCircleRasterizeVertexBuffer::NumVertices - 2;

		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;
		Indices.Empty(NumTriangles * 3);

		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
		{
			int32 LeadingVertexIndex = TriangleIndex + 2;
			Indices.Add(0);
			Indices.Add(LeadingVertexIndex - 1);
			Indices.Add(LeadingVertexIndex);
		}

		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(uint16);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Indices);
		IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}
};

TGlobalResource<FCircleRasterizeIndexBuffer> GCircleRasterizeIndexBuffer;

void FDeferredShadingSceneRenderer::RenderLocalLightsForVolumetricFog(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	bool bUseTemporalReprojection,
	const FVolumetricFogIntegrationParameterData& IntegrationData,
	const FExponentialHeightFogSceneInfo& FogInfo,
	FIntVector VolumetricFogGridSize,
	FVector GridZParams,
	const FPooledRenderTargetDesc& VolumeDesc,
	const FRDGTexture*& OutLocalShadowedLightScattering)
{
	TArray<const FLightSceneInfo*, SceneRenderingAllocator> LightsToInject;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent()
			&& LightSceneInfo->ShouldRenderLight(View)
			&& LightNeedsSeparateInjectionIntoVolumetricFog(LightSceneInfo, VisibleLightInfos[LightSceneInfo->Id])
			&& LightSceneInfo->Proxy->GetVolumetricScatteringIntensity() > 0)
		{
			const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

			if ((View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < (FogInfo.VolumetricFogDistance + LightBounds.W) * (FogInfo.VolumetricFogDistance + LightBounds.W))
			{
				LightsToInject.Add(LightSceneInfo);
			}
		}
	}

	if (LightsToInject.Num() > 0)
	{
		OutLocalShadowedLightScattering = GraphBuilder.CreateTexture(VolumeDesc, TEXT("LocalShadowedLightScattering"));

		FRenderTargetParameters* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutLocalShadowedLightScattering, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShadowedLights"),
			PassParameters,
			ERenderGraphPassFlags::None,
			[PassParameters, &View, this, LightsToInject, VolumetricFogGridSize, GridZParams, bUseTemporalReprojection, IntegrationData, FogInfo](FRHICommandListImmediate& RHICmdList)
		{
			for (int32 LightIndex = 0; LightIndex < LightsToInject.Num(); LightIndex++)
			{
				const FLightSceneInfo* LightSceneInfo = LightsToInject[LightIndex];
				FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(LightSceneInfo->Proxy, VisibleLightInfos[LightSceneInfo->Id]);

				const bool bInverseSquared = LightSceneInfo->Proxy->IsInverseSquared();
				const bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
				const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
				const FIntPoint VolumeZBounds = CalculateVolumetricFogBoundsForLight(LightBounds, View, VolumetricFogGridSize, GridZParams);

				if (VolumeZBounds.X < VolumeZBounds.Y)
				{
					TInjectShadowedLocalLightPS::FPermutationDomain PermutationVector;
					PermutationVector.Set< TInjectShadowedLocalLightPS::FDynamicallyShadowed >(bDynamicallyShadowed);
					PermutationVector.Set< TInjectShadowedLocalLightPS::FInverseSquared >(bInverseSquared);
					PermutationVector.Set< TInjectShadowedLocalLightPS::FTemporalReprojection >(bUseTemporalReprojection);

					auto VertexShader = View.ShaderMap->GetShader< FWriteToBoundingSphereVS >();
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
					auto PixelShader = View.ShaderMap->GetShader< TInjectShadowedLocalLightPS >(PermutationVector);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					// Accumulate the contribution of multiple lights
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
					GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, View, IntegrationData, LightSceneInfo, FogInfo, ProjectedShadowInfo, bDynamicallyShadowed);
					VertexShader->SetParameters(RHICmdList, View, IntegrationData, LightBounds, VolumeZBounds.X);

					if (GeometryShader.IsValid())
					{
						GeometryShader->SetParameters(RHICmdList, VolumeZBounds.X);
					}

					RHICmdList.SetStreamSource(0, GCircleRasterizeVertexBuffer.VertexBufferRHI, 0);
					const int32 NumInstances = VolumeZBounds.Y - VolumeZBounds.X;
					const int32 NumTriangles = FCircleRasterizeVertexBuffer::NumVertices - 2;
					RHICmdList.DrawIndexedPrimitive(GCircleRasterizeIndexBuffer.IndexBufferRHI, 0, 0, FCircleRasterizeVertexBuffer::NumVertices, 0, NumTriangles, NumInstances);
				}
			}
		});
	}
}

class TVolumetricFogLightScatteringCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(TVolumetricFogLightScatteringCS)

	class FTemporalReprojection			: SHADER_PERMUTATION_BOOL("USE_TEMPORAL_REPROJECTION");
	class FDistanceFieldSkyOcclusion	: SHADER_PERMUTATION_BOOL("DISTANCE_FIELD_SKY_OCCLUSION");

	using FPermutationDomain = TShaderPermutationDomain<
		FTemporalReprojection,
		FDistanceFieldSkyOcclusion >;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VBufferA)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VBufferB)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LocalShadowedLightScattering)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LightFunctionTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWLightScattering)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVolumetricFog(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogGridInjectionGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	TVolumetricFogLightScatteringCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(this, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());

		LocalShadowedLightScattering.Bind(Initializer.ParameterMap, TEXT("LocalShadowedLightScattering"));
		LightScatteringHistory.Bind(Initializer.ParameterMap, TEXT("LightScatteringHistory"));
		LightScatteringHistorySampler.Bind(Initializer.ParameterMap, TEXT("LightScatteringHistorySampler"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		DirectionalLightFunctionWorldToShadow.Bind(Initializer.ParameterMap, TEXT("DirectionalLightFunctionWorldToShadow"));
		LightFunctionTexture.Bind(Initializer.ParameterMap, TEXT("LightFunctionTexture"));
		LightFunctionSampler.Bind(Initializer.ParameterMap, TEXT("LightFunctionSampler"));
		StaticLightingScatteringIntensity.Bind(Initializer.ParameterMap, TEXT("StaticLightingScatteringIntensity"));
		SkyLightUseStaticShadowing.Bind(Initializer.ParameterMap, TEXT("SkyLightUseStaticShadowing"));
		SkyLightVolumetricScatteringIntensity.Bind(Initializer.ParameterMap, TEXT("SkyLightVolumetricScatteringIntensity"));
		SkySH.Bind(Initializer.ParameterMap, TEXT("SkySH"));
		PhaseG.Bind(Initializer.ParameterMap, TEXT("PhaseG"));
		InverseSquaredLightDistanceBiasScale.Bind(Initializer.ParameterMap, TEXT("InverseSquaredLightDistanceBiasScale"));
		UseHeightFogColors.Bind(Initializer.ParameterMap, TEXT("UseHeightFogColors"));
		UseDirectionalLightShadowing.Bind(Initializer.ParameterMap, TEXT("UseDirectionalLightShadowing"));
		AOParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
	}

	TVolumetricFogLightScatteringCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FVolumetricFogIntegrationParameterData& IntegrationData,
		const FExponentialHeightFogSceneInfo& FogInfo,
		FTextureRHIParamRef LightScatteringHistoryTexture,
		bool bUseDirectionalLightShadowing,
		const FMatrix& DirectionalLightFunctionWorldToShadowValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		if (!LightScatteringHistoryTexture)
		{
			LightScatteringHistoryTexture = GBlackVolumeTexture->TextureRHI;
		}

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			LightScatteringHistory,
			LightScatteringHistorySampler,
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			LightScatteringHistoryTexture);

		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);
		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FForwardLightData>(), View.ForwardLightingResources->ForwardLightDataUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, DirectionalLightFunctionWorldToShadow, DirectionalLightFunctionWorldToShadowValue);

		SetSamplerParameter(RHICmdList, ShaderRHI, LightFunctionSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		FScene* Scene = (FScene*)View.Family->Scene;
		FDistanceFieldAOParameters AOParameterData(Scene->DefaultMaxDistanceFieldOcclusionDistance);
		FSkyLightSceneProxy* SkyLight = Scene->SkyLight;

		if (SkyLight
			// Skylights with static lighting had their diffuse contribution baked into lightmaps
			&& !SkyLight->bHasStaticLighting
			&& View.Family->EngineShowFlags.SkyLighting)
		{
			const float LocalSkyLightUseStaticShadowing = SkyLight->bWantsStaticShadowing && SkyLight->bCastShadows ? 1.0f : 0.0f;
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightUseStaticShadowing, LocalSkyLightUseStaticShadowing);
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightVolumetricScatteringIntensity, SkyLight->VolumetricScatteringIntensity);

			const FSHVectorRGB3& SkyIrradiance = SkyLight->IrradianceEnvironmentMap;
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.R.V, 0);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.G.V, 1);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.B.V, 2);

			AOParameterData = FDistanceFieldAOParameters(SkyLight->OcclusionMaxDistance, SkyLight->Contrast);
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightUseStaticShadowing, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightVolumetricScatteringIntensity, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 0);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 1);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 2);
		}

		float StaticLightingScatteringIntensityValue = 0;

		if (View.Family->EngineShowFlags.GlobalIllumination && View.Family->EngineShowFlags.VolumetricLightmap)
		{
			StaticLightingScatteringIntensityValue = FogInfo.VolumetricFogStaticLightingScatteringIntensity;
		}

		SetShaderValue(RHICmdList, ShaderRHI, StaticLightingScatteringIntensity, StaticLightingScatteringIntensityValue);

		SetShaderValue(RHICmdList, ShaderRHI, PhaseG, FogInfo.VolumetricFogScatteringDistribution);
		SetShaderValue(RHICmdList, ShaderRHI, InverseSquaredLightDistanceBiasScale, GInverseSquaredLightDistanceBiasScale);
		SetShaderValue(RHICmdList, ShaderRHI, UseHeightFogColors, FogInfo.bOverrideLightColorsWithFogInscatteringColors ? 1.0f : 0.0f);
		SetShaderValue(RHICmdList, ShaderRHI, UseDirectionalLightShadowing, bUseDirectionalLightShadowing ? 1.0f : 0.0f);

		AOParameters.Set(RHICmdList, ShaderRHI, AOParameterData);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, View.GlobalDistanceFieldInfo.ParameterData);

		FFogUniformParameters FogUniformParameters;
		SetupFogUniformParameters(View, FogUniformParameters);
		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FFogUniformParameters>(), FogUniformParameters);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LocalShadowedLightScattering;
		Ar << LightScatteringHistory;
		Ar << LightScatteringHistorySampler;
		Ar << VolumetricFogParameters;
		Ar << DirectionalLightFunctionWorldToShadow;
		Ar << LightFunctionTexture;
		Ar << LightFunctionSampler;
		Ar << StaticLightingScatteringIntensity;
		Ar << SkyLightUseStaticShadowing;
		Ar << SkyLightVolumetricScatteringIntensity;
		Ar << SkySH;
		Ar << PhaseG;
		Ar << InverseSquaredLightDistanceBiasScale;
		Ar << UseHeightFogColors;
		Ar << UseDirectionalLightShadowing;
		Ar << AOParameters;
		Ar << GlobalDistanceFieldParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter LocalShadowedLightScattering;
	FShaderResourceParameter LightScatteringHistory;
	FShaderResourceParameter LightScatteringHistorySampler;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FShaderParameter DirectionalLightFunctionWorldToShadow;
	FShaderResourceParameter LightFunctionTexture;
	FShaderResourceParameter LightFunctionSampler;
	FShaderParameter StaticLightingScatteringIntensity;
	FShaderParameter SkyLightUseStaticShadowing;
	FShaderParameter SkyLightVolumetricScatteringIntensity;
	FShaderParameter SkySH;
	FShaderParameter PhaseG;
	FShaderParameter InverseSquaredLightDistanceBiasScale;
	FShaderParameter UseHeightFogColors;
	FShaderParameter UseDirectionalLightShadowing;
	FAOParameters AOParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
};

IMPLEMENT_GLOBAL_SHADER(TVolumetricFogLightScatteringCS, "/Engine/Private/VolumetricFog.usf", "LightScatteringCS", SF_Compute);

uint32 VolumetricFogIntegrationGroupSize = 8;

class FVolumetricFogFinalIntegrationCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogFinalIntegrationCS, Global)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, LightScattering)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWIntegratedLightScattering)
	END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportVolumetricFog(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogIntegrationGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	FVolumetricFogFinalIntegrationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(this, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());

		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FVolumetricFogFinalIntegrationCS()
	{
	}

public:
	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FVolumetricFogIntegrationParameterData& IntegrationData)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, IntegrationData);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(, FVolumetricFogFinalIntegrationCS, TEXT("/Engine/Private/VolumetricFog.usf"), TEXT("FinalIntegrationCS"), SF_Compute);

bool ShouldRenderVolumetricFog(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return ShouldRenderFog(ViewFamily)
		&& Scene
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportVolumetricFog(Scene->GetShaderPlatform())
		&& GVolumetricFog
		&& ViewFamily.EngineShowFlags.VolumetricFog
		&& Scene->ExponentialFogs.Num() > 0
		&& Scene->ExponentialFogs[0].bEnableVolumetricFog
		&& Scene->ExponentialFogs[0].VolumetricFogDistance > 0;
}

FVector GetVolumetricFogGridZParams(float NearPlane, float FarPlane, int32 GridSizeZ)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = GVolumetricFogDepthDistributionScale;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * FMath::Exp2((GridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	double O2 = (FMath::Exp2((GridSizeZ - 1) / S) - F / N) / (-F / N + 1);

	float FloatN = (float)N;
	float FloatF = (float)F;
	float FloatB = (float)B;
	float FloatO = (float)O;
	float FloatS = (float)S;

	float NSlice = FMath::Log2(FloatN*FloatB + FloatO) * FloatS;
	float NearPlaneSlice = FMath::Log2(NearPlane*FloatB + FloatO) * FloatS;
	float FSlice = FMath::Log2(FloatF*FloatB + FloatO) * FloatS;
	// y = log2(z*B + O) * S
	// f(N) = 0 = log2(N*B + O) * S
	// 1 = N*B + O
	// O = 1 - N*B
	// B = (1 - O) / N

	// f(F) = GLightGridSizeZ - 1 = log2(F*B + O) * S
	// exp2((GLightGridSizeZ - 1) / S) = F*B + O
	// exp2((GLightGridSizeZ - 1) / S) = F * (1 - O) / N + O
	// exp2((GLightGridSizeZ - 1) / S) = F / N - F / N * O + O
	// exp2((GLightGridSizeZ - 1) / S) = F / N + (-F / N + 1) * O
	// O = (exp2((GLightGridSizeZ - 1) / S) - F / N) / (-F / N + 1)

	return FVector(B, O, S);
}

FIntVector GetVolumetricFogGridSize(FIntPoint ViewRectSize)
{
	extern int32 GLightGridSizeZ;
	const FIntPoint VolumetricFogGridSizeXY = FIntPoint::DivideAndRoundUp(ViewRectSize, GVolumetricFogGridPixelSize);
	return FIntVector(VolumetricFogGridSizeXY.X, VolumetricFogGridSizeXY.Y, GVolumetricFogGridSizeZ);
}

void SetupVolumetricFogGlobalData(const FViewInfo& View, FVolumetricFogGlobalData& Parameters)
{
	const FScene* Scene = (FScene*)View.Family->Scene;
	const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

	const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());

	Parameters.GridSizeInt = VolumetricFogGridSize;
	Parameters.GridSize = FVector(VolumetricFogGridSize);

	FVector ZParams = GetVolumetricFogGridZParams(View.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);
	Parameters.GridZParams = ZParams;

	Parameters.SVPosToVolumeUV = FVector2D(1.0f, 1.0f) / (FVector2D(Parameters.GridSize) * GVolumetricFogGridPixelSize);
	Parameters.FogGridToPixelXY = FIntPoint(GVolumetricFogGridPixelSize, GVolumetricFogGridPixelSize);
	Parameters.MaxDistance = FogInfo.VolumetricFogDistance;

	Parameters.HeightFogInscatteringColor = View.ExponentialFogColor;

	Parameters.HeightFogDirectionalLightInscatteringColor = FVector::ZeroVector;

	if (View.bUseDirectionalInscattering && !View.FogInscatteringColorCubemap)
	{
		Parameters.HeightFogDirectionalLightInscatteringColor = FVector(View.DirectionalInscatteringColor);
	}
}

void FViewInfo::SetupVolumetricFogUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	const FScene* Scene = (const FScene*)Family->Scene;

	if (ShouldRenderVolumetricFog(Scene, *Family))
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(ViewRect.Size());

		ViewUniformShaderParameters.VolumetricFogInvGridSize = FVector(1.0f / VolumetricFogGridSize.X, 1.0f / VolumetricFogGridSize.Y, 1.0f / VolumetricFogGridSize.Z);

		const FVector ZParams = GetVolumetricFogGridZParams(NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);
		ViewUniformShaderParameters.VolumetricFogGridZParams = ZParams;

		ViewUniformShaderParameters.VolumetricFogSVPosToVolumeUV = FVector2D(1.0f, 1.0f) / (FVector2D(VolumetricFogGridSize.X, VolumetricFogGridSize.Y) * GVolumetricFogGridPixelSize);
		ViewUniformShaderParameters.VolumetricFogMaxDistance = FogInfo.VolumetricFogDistance;
	}
	else
	{
		ViewUniformShaderParameters.VolumetricFogInvGridSize = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricFogGridZParams = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricFogSVPosToVolumeUV = FVector2D(0, 0);
		ViewUniformShaderParameters.VolumetricFogMaxDistance = 0;
	}
}

bool FDeferredShadingSceneRenderer::ShouldRenderVolumetricFog() const
{
	return ::ShouldRenderVolumetricFog(Scene, ViewFamily);
}

void FDeferredShadingSceneRenderer::SetupVolumetricFog()
{
	if (ShouldRenderVolumetricFog())
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());

			FVolumetricFogGlobalData GlobalData;
			SetupVolumetricFogGlobalData(View, GlobalData);
			View.VolumetricFogResources.VolumetricFogGlobalData = TUniformBufferRef<FVolumetricFogGlobalData>::CreateUniformBufferImmediate(GlobalData, UniformBuffer_SingleFrame);
		}
	}
	else
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			if (View.ViewState)
			{
				View.ViewState->LightScatteringHistory = NULL;
			}
		}
	}
}

void FDeferredShadingSceneRenderer::ComputeVolumetricFog(FRHICommandListImmediate& RHICmdListImmediate)
{
	check(RHICmdListImmediate.IsOutsideRenderPass());

	if (ShouldRenderVolumetricFog())
	{		
		QUICK_SCOPE_CYCLE_COUNTER(STAT_VolumetricFog);
		SCOPED_GPU_STAT(RHICmdListImmediate, VolumetricFog);

		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());
			const FVector GridZParams = GetVolumetricFogGridZParams(View.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);

			//@DW - graph todo
			//SCOPED_DRAW_EVENT(RHICmdList, VolumetricFog);

			const FVector FrameJitterOffsetValue = VolumetricFogTemporalRandom(View.Family->FrameNumber);

			FVolumetricFogIntegrationParameterData IntegrationData;
			IntegrationData.FrameJitterOffsetValues.Empty(16);
			IntegrationData.FrameJitterOffsetValues.AddZeroed(16);
			IntegrationData.FrameJitterOffsetValues[0] = VolumetricFogTemporalRandom(View.Family->FrameNumber);

			for (int32 FrameOffsetIndex = 1; FrameOffsetIndex < GVolumetricFogHistoryMissSupersampleCount; FrameOffsetIndex++)
			{
				IntegrationData.FrameJitterOffsetValues[FrameOffsetIndex] = VolumetricFogTemporalRandom(View.Family->FrameNumber - FrameOffsetIndex);
			}

			const bool bUseTemporalReprojection =
				GVolumetricFogTemporalReprojection
				&& View.ViewState;

			IntegrationData.bTemporalHistoryIsValid =
				bUseTemporalReprojection
				&& !View.bCameraCut
				&& !View.bPrevTransformsReset
				&& ViewFamily.bRealtimeUpdate
				&& View.ViewState->LightScatteringHistory;

			FMatrix LightFunctionWorldToShadow;

			FRDGBuilder GraphBuilder(RHICmdListImmediate);

			//@DW - register GWhiteTexture as a graph external for when there's no light function - later a shader is going to bind it whether we rendered to it or not
			const FRDGTexture* LightFunctionTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy);
			bool bUseDirectionalLightShadowing;

			RenderLightFunctionForVolumetricFog(
				GraphBuilder,
				View,
				VolumetricFogGridSize,
				FogInfo.VolumetricFogDistance,
				LightFunctionWorldToShadow,
				LightFunctionTexture,
				bUseDirectionalLightShadowing);

			const uint32 Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV | TexCreate_ReduceMemoryWithTilingMode;
			FPooledRenderTargetDesc VolumeDesc(FPooledRenderTargetDesc::CreateVolumeDesc(VolumetricFogGridSize.X, VolumetricFogGridSize.Y, VolumetricFogGridSize.Z, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_None, Flags, false));
			FPooledRenderTargetDesc VolumeDescFastVRAM = VolumeDesc;
			VolumeDescFastVRAM.Flags |= GFastVRamConfig.VolumetricFog;

			//@DW - Explicit creation of graph resource handles
			//@DW - Passing these around in a struct to ease manual wiring
			IntegrationData.VBufferA = GraphBuilder.CreateTexture(VolumeDescFastVRAM, TEXT("VBufferA"));
			IntegrationData.VBufferB = GraphBuilder.CreateTexture(VolumeDescFastVRAM, TEXT("VBufferB"));
			IntegrationData.VBufferA_UAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(IntegrationData.VBufferA));
			IntegrationData.VBufferB_UAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(IntegrationData.VBufferB));

			{
				FVolumetricFogMaterialSetupCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumetricFogMaterialSetupCS::FParameters>();
				PassParameters->GlobalAlbedo = FogInfo.VolumetricFogAlbedo;
				PassParameters->GlobalEmissive = FogInfo.VolumetricFogEmissive;
				PassParameters->GlobalExtinctionScale = FogInfo.VolumetricFogExtinctionScale;

				PassParameters->RWVBufferA = IntegrationData.VBufferA_UAV;
				PassParameters->RWVBufferB = IntegrationData.VBufferB_UAV;

				FFogUniformParameters FogUniformParameters;
				SetupFogUniformParameters(View, FogUniformParameters);
				PassParameters->FogUniformParameters = CreateUniformBufferImmediate(FogUniformParameters, UniformBuffer_SingleDraw);
				PassParameters->View = View.ViewUniformBuffer;

				auto ComputeShader = View.ShaderMap->GetShader< FVolumetricFogMaterialSetupCS >();
				ClearUnusedGraphResources(ComputeShader, PassParameters);

				//@DW - this pass only reads external textures, we don't have any graph inputs
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("InitializeVolumeAttributes"),
					PassParameters,
					ERenderGraphPassFlags::Compute,
					[PassParameters, &View, VolumetricFogGridSize, IntegrationData, ComputeShader](FRHICommandListImmediate& RHICmdList)
				{
					const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogGridInjectionGroupSize);

					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

					ComputeShader->SetParameters(RHICmdList, View, IntegrationData);

					SetShaderParameters(RHICmdList, ComputeShader, ComputeShader->GetComputeShader(), *PassParameters);
					DispatchComputeShader(RHICmdList, ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShader->GetComputeShader());
				});

				VoxelizeFogVolumePrimitives(
					GraphBuilder,
					View,
					IntegrationData,
					VolumetricFogGridSize,
					GridZParams,
					FogInfo.VolumetricFogDistance);
			}

			const FRDGTexture* LocalShadowedLightScattering = GraphBuilder.RegisterExternalTexture(GSystemTextures.VolumetricBlackDummy);
			RenderLocalLightsForVolumetricFog(GraphBuilder, View, bUseTemporalReprojection, IntegrationData, FogInfo, VolumetricFogGridSize, GridZParams, VolumeDescFastVRAM, LocalShadowedLightScattering);

			IntegrationData.LightScattering = GraphBuilder.CreateTexture(VolumeDesc, TEXT("LightScattering"));
			IntegrationData.LightScatteringUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(IntegrationData.LightScattering));

			{
				TVolumetricFogLightScatteringCS::FParameters* PassParameters = GraphBuilder.AllocParameters<TVolumetricFogLightScatteringCS::FParameters>();

				PassParameters->VBufferA = IntegrationData.VBufferA;
				PassParameters->VBufferB = IntegrationData.VBufferB;
				PassParameters->LocalShadowedLightScattering = LocalShadowedLightScattering;
				PassParameters->LightFunctionTexture = LightFunctionTexture;
				PassParameters->RWLightScattering = IntegrationData.LightScatteringUAV;

				const bool bUseGlobalDistanceField = UseGlobalDistanceField() && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0;

				const bool bUseDistanceFieldSkyOcclusion =
					ViewFamily.EngineShowFlags.AmbientOcclusion
					&& Scene->SkyLight
					&& Scene->SkyLight->bCastShadows
					&& Scene->SkyLight->bCastVolumetricShadow
					&& ShouldRenderDistanceFieldAO()
					&& SupportsDistanceFieldAO(View.GetFeatureLevel(), View.GetShaderPlatform())
					&& bUseGlobalDistanceField
					&& Views.Num() == 1
					&& View.IsPerspectiveProjection();

				TVolumetricFogLightScatteringCS::FPermutationDomain PermutationVector;
				PermutationVector.Set< TVolumetricFogLightScatteringCS::FTemporalReprojection >(bUseTemporalReprojection);
				PermutationVector.Set< TVolumetricFogLightScatteringCS::FDistanceFieldSkyOcclusion >(bUseDistanceFieldSkyOcclusion);

				auto ComputeShader = View.ShaderMap->GetShader< TVolumetricFogLightScatteringCS >(PermutationVector);
				ClearUnusedGraphResources(ComputeShader, PassParameters);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("LightScattering %dx%dx%d %s %s",
						VolumetricFogGridSize.X,
						VolumetricFogGridSize.Y,
						VolumetricFogGridSize.Z,
						bUseDistanceFieldSkyOcclusion ? TEXT("DFAO") : TEXT(""),
						PassParameters->LightFunctionTexture ? TEXT("LF") : TEXT("")),
					PassParameters,
					ERenderGraphPassFlags::Compute,
					[PassParameters, ComputeShader, &View, this, FogInfo, bUseTemporalReprojection, VolumetricFogGridSize, IntegrationData, bUseDirectionalLightShadowing, bUseDistanceFieldSkyOcclusion, LightFunctionWorldToShadow](FRHICommandListImmediate& RHICmdList)
				{
					UnbindRenderTargets(RHICmdList);
					const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogGridInjectionGroupSize);

					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

					FTextureRHIParamRef LightScatteringHistoryTexture = bUseTemporalReprojection && View.ViewState->LightScatteringHistory.IsValid()
						? View.ViewState->LightScatteringHistory->GetRenderTargetItem().ShaderResourceTexture
						: GBlackVolumeTexture->TextureRHI;

					ComputeShader->SetParameters(RHICmdList, View, IntegrationData, FogInfo, LightScatteringHistoryTexture, bUseDirectionalLightShadowing, LightFunctionWorldToShadow);

					SetShaderParameters(RHICmdList, ComputeShader, ComputeShader->GetComputeShader(), *PassParameters);
					DispatchComputeShader(RHICmdList, ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShader->GetComputeShader());
				});
			}

			const FRDGTexture* IntegratedLightScattering = GraphBuilder.CreateTexture(VolumeDesc, TEXT("IntegratedLightScattering"));
			const FRDGTextureUAV* IntegratedLightScatteringUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(IntegratedLightScattering));

			{
				FVolumetricFogFinalIntegrationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVolumetricFogFinalIntegrationCS::FParameters>();
				PassParameters->LightScattering = IntegrationData.LightScattering;
				PassParameters->RWIntegratedLightScattering = IntegratedLightScatteringUAV;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("FinalIntegration"),
					PassParameters,
					ERenderGraphPassFlags::Compute,
					[PassParameters, &View, VolumetricFogGridSize, IntegrationData, this](FRHICommandListImmediate& RHICmdList)
				{
					const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogIntegrationGroupSize);

					auto ComputeShader = View.ShaderMap->GetShader< FVolumetricFogFinalIntegrationCS >();
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, IntegrationData);

					SetShaderParameters(RHICmdList, ComputeShader, ComputeShader->GetComputeShader(), *PassParameters);
					DispatchComputeShader(RHICmdList, ComputeShader, NumGroups.X, NumGroups.Y, 1);
					UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShader->GetComputeShader());
				});
			}

			GraphBuilder.QueueTextureExtraction(IntegratedLightScattering, &View.VolumetricFogResources.IntegratedLightScattering);

			if (bUseTemporalReprojection)
			{
				GraphBuilder.QueueTextureExtraction(IntegrationData.LightScattering, &View.ViewState->LightScatteringHistory);
			}
			else if (View.ViewState)
			{
				View.ViewState->LightScatteringHistory = NULL;
			}

			GraphBuilder.Execute();
		}
	}
}