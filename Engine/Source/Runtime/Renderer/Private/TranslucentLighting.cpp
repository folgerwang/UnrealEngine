// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentLighting.cpp: Translucent lighting implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "EngineDefines.h"
#include "RHI.h"
#include "RenderResource.h"
#include "HitProxies.h"
#include "FinalPostProcessSettings.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/LightComponent.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "TranslucentRendering.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "LightRendering.h"
#include "ScreenRendering.h"
#include "AmbientCubemapParameters.h"
#include "VolumeRendering.h"
#include "VolumeLighting.h"
#include "PipelineStateCache.h"
#include "VisualizeTexture.h"
#include "MeshPassProcessor.inl"

class FMaterial;

/** Whether to allow rendering translucency shadow depths. */
bool GUseTranslucencyShadowDepths = true;

DECLARE_GPU_STAT_NAMED(TranslucentLighting, TEXT("Translucent Lighting"));
 
int32 GUseTranslucentLightingVolumes = 1;
FAutoConsoleVariableRef CVarUseTranslucentLightingVolumes(
	TEXT("r.TranslucentLightingVolume"),
	GUseTranslucentLightingVolumes,
	TEXT("Whether to allow updating the translucent lighting volumes.\n")
	TEXT("0:off, otherwise on, default is 1"),
	ECVF_RenderThreadSafe
	);

float GTranslucentVolumeMinFOV = 45;
static FAutoConsoleVariableRef CVarTranslucentVolumeMinFOV(
	TEXT("r.TranslucentVolumeMinFOV"),
	GTranslucentVolumeMinFOV,
	TEXT("Minimum FOV for translucent lighting volume.  Prevents popping in lighting when zooming in."),
	ECVF_RenderThreadSafe
	);

float GTranslucentVolumeFOVSnapFactor = 10;
static FAutoConsoleVariableRef CTranslucentVolumeFOVSnapFactor(
	TEXT("r.TranslucentVolumeFOVSnapFactor"),
	GTranslucentVolumeFOVSnapFactor,
	TEXT("FOV will be snapped to a factor of this before computing volume bounds."),
	ECVF_RenderThreadSafe
	);

int32 GUseTranslucencyVolumeBlur = 1;
FAutoConsoleVariableRef CVarUseTranslucentLightingVolumeBlur(
	TEXT("r.TranslucencyVolumeBlur"),
	GUseTranslucencyVolumeBlur,
	TEXT("Whether to blur the translucent lighting volumes.\n")
	TEXT("0:off, otherwise on, default is 1"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyLightingVolumeDim = 64;
FAutoConsoleVariableRef CVarTranslucencyLightingVolumeDim(
	TEXT("r.TranslucencyLightingVolumeDim"),
	GTranslucencyLightingVolumeDim,
	TEXT("Dimensions of the volume textures used for translucency lighting.  Larger textures result in higher resolution but lower performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeInnerDistance(
	TEXT("r.TranslucencyLightingVolumeInnerDistance"),
	1500.0f,
	TEXT("Distance from the camera that the first volume cascade should end"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeOuterDistance(
	TEXT("r.TranslucencyLightingVolumeOuterDistance"),
	5000.0f,
	TEXT("Distance from the camera that the second volume cascade should end"),
	ECVF_RenderThreadSafe);

void FViewInfo::CalcTranslucencyLightingVolumeBounds(FBox* InOutCascadeBoundsArray, int32 NumCascades) const
{
	for (int32 CascadeIndex = 0; CascadeIndex < NumCascades; CascadeIndex++)
	{
		float InnerDistance = CVarTranslucencyLightingVolumeInnerDistance.GetValueOnRenderThread();
		float OuterDistance = CVarTranslucencyLightingVolumeOuterDistance.GetValueOnRenderThread();

		const float FrustumStartDistance = CascadeIndex == 0 ? 0 : InnerDistance;
		const float FrustumEndDistance = CascadeIndex == 0 ? InnerDistance : OuterDistance;

		float FieldOfView = PI / 4.0f;
		float AspectRatio = 1.0f;

		if (IsPerspectiveProjection())
		{
			// Derive FOV and aspect ratio from the perspective projection matrix
			FieldOfView = FMath::Atan(1.0f / ShadowViewMatrices.GetProjectionMatrix().M[0][0]);
			// Clamp to prevent shimmering when zooming in
			FieldOfView = FMath::Max(FieldOfView, GTranslucentVolumeMinFOV * (float)PI / 180.0f);
			const float RoundFactorRadians = GTranslucentVolumeFOVSnapFactor * (float)PI / 180.0f;
			// Round up to a fixed factor
			// This causes the volume lighting to make discreet jumps as the FOV animates, instead of slowly crawling over a long period
			FieldOfView = FieldOfView + RoundFactorRadians - FMath::Fmod(FieldOfView, RoundFactorRadians);
			AspectRatio = ShadowViewMatrices.GetProjectionMatrix().M[1][1] / ShadowViewMatrices.GetProjectionMatrix().M[0][0];
		}

		const float StartHorizontalLength = FrustumStartDistance * FMath::Tan(FieldOfView);
		const FVector StartCameraRightOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(0) * StartHorizontalLength;
		const float StartVerticalLength = StartHorizontalLength / AspectRatio;
		const FVector StartCameraUpOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(1) * StartVerticalLength;

		const float EndHorizontalLength = FrustumEndDistance * FMath::Tan(FieldOfView);
		const FVector EndCameraRightOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(0) * EndHorizontalLength;
		const float EndVerticalLength = EndHorizontalLength / AspectRatio;
		const FVector EndCameraUpOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(1) * EndVerticalLength;

		FVector SplitVertices[8];
		const FVector ShadowViewOrigin = ShadowViewMatrices.GetViewOrigin();

		SplitVertices[0] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance + StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[1] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance + StartCameraRightOffset - StartCameraUpOffset;
		SplitVertices[2] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance - StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[3] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance - StartCameraRightOffset - StartCameraUpOffset;

		SplitVertices[4] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance + EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[5] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance + EndCameraRightOffset - EndCameraUpOffset;
		SplitVertices[6] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance - EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[7] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance - EndCameraRightOffset - EndCameraUpOffset;

		FVector Center(0,0,0);
		// Weight the far vertices more so that the bounding sphere will be further from the camera
		// This minimizes wasted shadowmap space behind the viewer
		const float FarVertexWeightScale = 10.0f;
		for (int32 VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			const float Weight = VertexIndex > 3 ? 1 / (4.0f + 4.0f / FarVertexWeightScale) : 1 / (4.0f + 4.0f * FarVertexWeightScale);
			Center += SplitVertices[VertexIndex] * Weight;
		}

		float RadiusSquared = 0;
		for (int32 VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			RadiusSquared = FMath::Max(RadiusSquared, (Center - SplitVertices[VertexIndex]).SizeSquared());
		}

		FSphere SphereBounds(Center, FMath::Sqrt(RadiusSquared));

		// Snap the center to a multiple of the volume dimension for stability
		const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
		SphereBounds.Center.X = SphereBounds.Center.X - FMath::Fmod(SphereBounds.Center.X, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);
		SphereBounds.Center.Y = SphereBounds.Center.Y - FMath::Fmod(SphereBounds.Center.Y, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);
		SphereBounds.Center.Z = SphereBounds.Center.Z - FMath::Fmod(SphereBounds.Center.Z, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);

		InOutCascadeBoundsArray[CascadeIndex] = FBox(SphereBounds.Center - SphereBounds.W, SphereBounds.Center + SphereBounds.W);
	}
}

/** Shader parameters for rendering the depth of a mesh for shadowing. */
class FShadowDepthShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ProjectionMatrix.Bind(ParameterMap,TEXT("ProjectionMatrix"));
		ShadowParams.Bind(ParameterMap,TEXT("ShadowParams"));
		ClampToNearPlane.Bind(ParameterMap,TEXT("bClampToNearPlane"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, ShaderRHIParamRef ShaderRHI, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		SetShaderValue(
			RHICmdList, 
			ShaderRHI,
			ProjectionMatrix,
			FTranslationMatrix(ShadowInfo->PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation()) * ShadowInfo->SubjectAndReceiverMatrix
			);

		SetShaderValue(RHICmdList, ShaderRHI, ShadowParams, FVector2D(ShadowInfo->GetShaderDepthBias(), ShadowInfo->InvMaxSubjectDepth));
		// Only clamp vertices to the near plane when rendering whole scene directional light shadow depths or preshadows from directional lights
		const bool bClampToNearPlaneValue = ShadowInfo->IsWholeSceneDirectionalShadow() || (ShadowInfo->bPreShadow && ShadowInfo->bDirectionalLight);
		SetShaderValue(RHICmdList, ShaderRHI,ClampToNearPlane,bClampToNearPlaneValue ? 1.0f : 0.0f);
	}

	/** Set the vertex shader parameter values. */
	void SetVertexShader(FRHICommandList& RHICmdList, FShader* VertexShader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		Set(RHICmdList, VertexShader->GetVertexShader(), View, ShadowInfo, MaterialRenderProxy);
	}

	/** Set the domain shader parameter values. */
	void SetDomainShader(FRHICommandList& RHICmdList, FShader* DomainShader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		Set(RHICmdList, DomainShader->GetDomainShader(), View, ShadowInfo, MaterialRenderProxy);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FShadowDepthShaderParameters& P)
	{
		Ar << P.ProjectionMatrix;
		Ar << P.ShadowParams;
		Ar << P.ClampToNearPlane;
		return Ar;
	}

private:
	FShaderParameter ProjectionMatrix;
	FShaderParameter ShadowParams;
	FShaderParameter ClampToNearPlane;
};

class FTranslucencyDepthShaderElementData : public FMeshMaterialShaderElementData
{
public:

	float TranslucentShadowStartOffset;
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucencyDepthPassUniformParameters,)
	SHADER_PARAMETER_STRUCT(FSceneTexturesUniformParameters, SceneTextures)
	SHADER_PARAMETER(FMatrix, ProjectionMatrix)
	SHADER_PARAMETER(float, bClampToNearPlane)
	SHADER_PARAMETER(float, InvMaxSubjectDepth)
	SHADER_PARAMETER_STRUCT(FTranslucentSelfShadowUniformParameters, TranslucentSelfShadow)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucencyDepthPassUniformParameters, "TranslucentDepthPass");

void SetupTranslucencyDepthPassUniformBuffer(
	const FProjectedShadowInfo* ShadowInfo,
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	FTranslucencyDepthPassUniformParameters& TranslucencyDepthPassParameters)
{
	// Note - scene depth can be bound by the material for use in depth fades
	// This is incorrect when rendering a shadowmap as it's not from the camera's POV
	// Set the scene depth texture to something safe when rendering shadow depths
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
	SetupSceneTextureUniformParameters(SceneRenderTargets, View.FeatureLevel, ESceneTextureSetupMode::None, TranslucencyDepthPassParameters.SceneTextures);

	TranslucencyDepthPassParameters.ProjectionMatrix = FTranslationMatrix(ShadowInfo->PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation()) * ShadowInfo->SubjectAndReceiverMatrix;

	// Only clamp vertices to the near plane when rendering whole scene directional light shadow depths or preshadows from directional lights
	const bool bClampToNearPlaneValue = ShadowInfo->IsWholeSceneDirectionalShadow() || (ShadowInfo->bPreShadow && ShadowInfo->bDirectionalLight);
	TranslucencyDepthPassParameters.bClampToNearPlane = bClampToNearPlaneValue ? 1.0f : 0.0f;

	TranslucencyDepthPassParameters.InvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;

	SetupTranslucentSelfShadowUniformParameters(ShadowInfo, TranslucencyDepthPassParameters.TranslucentSelfShadow);
}

/**
* Vertex shader used to render shadow maps for translucency.
*/
class FTranslucencyShadowDepthVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FTranslucencyShadowDepthVS, MeshMaterial);
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return IsTranslucentBlendMode(Material->GetBlendMode()) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FTranslucencyShadowDepthVS() {}
	FTranslucencyShadowDepthVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FTranslucencyDepthPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}
};

enum ETranslucencyShadowDepthShaderMode
{
	TranslucencyShadowDepth_PerspectiveCorrect,
	TranslucencyShadowDepth_Standard,
};

template <ETranslucencyShadowDepthShaderMode ShaderMode>
class TTranslucencyShadowDepthVS : public FTranslucencyShadowDepthVS
{
	DECLARE_SHADER_TYPE(TTranslucencyShadowDepthVS, MeshMaterial);
public:

	TTranslucencyShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FTranslucencyShadowDepthVS(Initializer)
	{
	}

	TTranslucencyShadowDepthVS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTranslucencyShadowDepthVS::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == TranslucencyShadowDepth_PerspectiveCorrect ? 1 : 0));
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthVS<TranslucencyShadowDepth_PerspectiveCorrect>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainVS"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthVS<TranslucencyShadowDepth_Standard>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainVS"),SF_Vertex);

/**
 * Pixel shader used for accumulating translucency layer densities
 */
class FTranslucencyShadowDepthPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FTranslucencyShadowDepthPS,MeshMaterial);
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsTranslucentBlendMode(Material->GetBlendMode()) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FTranslucencyShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FTranslucencyDepthPassUniformParameters::StaticStructMetadata.GetShaderVariableName());

		TranslucentShadowStartOffset.Bind(Initializer.ParameterMap, TEXT("TranslucentShadowStartOffset"));
	}

	FTranslucencyShadowDepthPS() {}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FTranslucencyDepthShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(TranslucentShadowStartOffset, ShaderElementData.TranslucentShadowStartOffset);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << TranslucentShadowStartOffset;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter TranslucentShadowStartOffset;
};

template <ETranslucencyShadowDepthShaderMode ShaderMode>
class TTranslucencyShadowDepthPS : public FTranslucencyShadowDepthPS
{
	DECLARE_SHADER_TYPE(TTranslucencyShadowDepthPS,MeshMaterial);
public:

	TTranslucencyShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FTranslucencyShadowDepthPS(Initializer)
	{
	}

	TTranslucencyShadowDepthPS() {}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FTranslucencyShadowDepthPS::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == TranslucencyShadowDepth_PerspectiveCorrect ? 1 : 0));
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthPS<TranslucencyShadowDepth_PerspectiveCorrect>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainOpacityPS"),SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthPS<TranslucencyShadowDepth_Standard>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainOpacityPS"),SF_Pixel);

class FTranslucencyDepthPassMeshProcessor : public FMeshPassProcessor
{
public:
	FTranslucencyDepthPassMeshProcessor(const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		const FProjectedShadowInfo* InShadowInfo,
		FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:

	template<ETranslucencyShadowDepthShaderMode ShaderMode>
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		float MaterialTranslucentShadowStartOffset,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
	const FProjectedShadowInfo* ShadowInfo;
	FShadowDepthType ShadowDepthType;
	const bool bDirectionalLight;
};

FTranslucencyDepthPassMeshProcessor::FTranslucencyDepthPassMeshProcessor(const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	const FProjectedShadowInfo* InShadowInfo,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
	, ShadowInfo(InShadowInfo)
	, ShadowDepthType(InShadowInfo->GetShadowDepthType())
	, bDirectionalLight(InShadowInfo->bDirectionalLight)
{
}

template<ETranslucencyShadowDepthShaderMode ShaderMode>
void FTranslucencyDepthPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	float MaterialTranslucentShadowStartOffset,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		TTranslucencyShadowDepthVS<ShaderMode>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		TTranslucencyShadowDepthPS<ShaderMode>> PassShaders;

	PassShaders.VertexShader = MaterialResource.GetShader<TTranslucencyShadowDepthVS<ShaderMode> >(VertexFactory->GetType());
	PassShaders.PixelShader = MaterialResource.GetShader<TTranslucencyShadowDepthPS<ShaderMode> >(VertexFactory->GetType());

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	FTranslucencyDepthShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const float LocalToWorldScale = ShadowInfo->GetParentSceneInfo()->Proxy->GetLocalToWorld().GetScaleVector().GetMax();
	const float TranslucentShadowStartOffsetValue = MaterialTranslucentShadowStartOffset * LocalToWorldScale;
	ShaderElementData.TranslucentShadowStartOffset = TranslucentShadowStartOffsetValue / (ShadowInfo->MaxSubjectZ - ShadowInfo->MinSubjectZ);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

void FTranslucencyDepthPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.CastShadow)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);
		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const float MaterialTranslucentShadowStartOffset = Material.GetTranslucentShadowStartOffset();
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

		// Only render translucent meshes into the Fourier opacity maps
		if (bIsTranslucent && ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
		{
			if (bDirectionalLight)
			{
				Process<TranslucencyShadowDepth_Standard>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MaterialTranslucentShadowStartOffset, MeshFillMode, MeshCullMode);
			}
			else
			{
				Process<TranslucencyShadowDepth_PerspectiveCorrect>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MaterialTranslucentShadowStartOffset, MeshFillMode, MeshCullMode);
			}
		}
	}
}

void FProjectedShadowInfo::RenderTranslucencyDepths(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer)
{
	check(RHICmdList.IsInsideRenderPass());
	check(IsInRenderingThread());
	checkSlow(!bWholeSceneShadow);
	SCOPE_CYCLE_COUNTER(STAT_RenderPerObjectShadowDepthsTime);

	FTranslucencyDepthPassUniformParameters TranslucencyDepthPassParameters;
	SetupTranslucencyDepthPassUniformBuffer(this, RHICmdList, *ShadowDepthView, TranslucencyDepthPassParameters);
	TUniformBufferRef<FTranslucencyDepthPassUniformParameters> PassUniformBuffer = TUniformBufferRef<FTranslucencyDepthPassUniformParameters>::CreateUniformBufferImmediate(TranslucencyDepthPassParameters, UniformBuffer_SingleFrame, EUniformBufferValidation::None);

	FMeshPassProcessorRenderState DrawRenderState(*ShadowDepthView, PassUniformBuffer);
	{
#if WANTS_DRAW_MESH_EVENTS
		FString EventName;
		if (GetEmitDrawEvents())
		{
			GetShadowTypeNameForDrawEvent(EventName);
		}
		SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepthActor, *EventName);
#endif
		// Clear the shadow and its border
		RHICmdList.SetViewport(
			X,
			Y,
			0.0f,
			(X + BorderSize * 2 + ResolutionX),
			(Y + BorderSize * 2 + ResolutionY),
			1.0f
			);

		FLinearColor ClearColors[2] = {FLinearColor(0,0,0,0), FLinearColor(0,0,0,0)};
		DrawClearQuadMRT(RHICmdList, true, ARRAY_COUNT(ClearColors), ClearColors, false, 1.0f, false, 0);

		// Set the viewport for the shadow.
		RHICmdList.SetViewport(
			(X + BorderSize),
			(Y + BorderSize),
			0.0f,
			(X + BorderSize + ResolutionX),
			(Y + BorderSize + ResolutionY),
			1.0f
			);

		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		DrawRenderState.SetBlendState(TStaticBlendState<
			CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
			CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());


		FMeshCommandOneFrameArray VisibleMeshDrawCommands;
		FDynamicPassMeshDrawListContext TranslucencyDepthContext(DynamicMeshDrawCommandStorage, VisibleMeshDrawCommands);

		FTranslucencyDepthPassMeshProcessor TranslucencyDepthPassMeshProcessor(
			SceneRenderer->Scene,
			ShadowDepthView,
			DrawRenderState,
			this,
			&TranslucencyDepthContext);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectTranslucentMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshAndRelevance = DynamicSubjectTranslucentMeshElements[MeshBatchIndex];
			check(!MeshAndRelevance.Mesh->bRequiresPerElementVisibility);
			const uint64 BatchElementMask = ~0ull;
			TranslucencyDepthPassMeshProcessor.AddMeshBatch(*MeshAndRelevance.Mesh, BatchElementMask, MeshAndRelevance.PrimitiveSceneProxy);
		}

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < SubjectTranslucentPrimitives.Num(); PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = SubjectTranslucentPrimitives[PrimitiveIndex];
			int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
			FPrimitiveViewRelevance ViewRelevance = ShadowDepthView->PrimitiveViewRelevanceMap[PrimitiveId];

			if (!ViewRelevance.bInitializedThisFrame)
			{
				// Compute the subject primitive's view relevance since it wasn't cached
				ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(ShadowDepthView);
			}

			if (ViewRelevance.bDrawRelevance && ViewRelevance.bStaticRelevance)
			{
				for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
				{
					const FStaticMeshBatch& StaticMeshBatch = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
					const uint64 BatchElementMask = StaticMeshBatch.bRequiresPerElementVisibility ? ShadowDepthView->StaticMeshBatchVisibility[StaticMeshBatch.BatchVisibilityId] : ~0ull;
					TranslucencyDepthPassMeshProcessor.AddMeshBatch(StaticMeshBatch, BatchElementMask, StaticMeshBatch.PrimitiveSceneInfo->Proxy, StaticMeshBatch.Id);
				}
			}
		}

		if (VisibleMeshDrawCommands.Num() > 0)
		{
			const bool bDynamicInstancing = IsDynamicInstancingEnabled(ShadowDepthView->FeatureLevel);

			FVertexBufferRHIParamRef PrimitiveIdVertexBuffer = nullptr;
			ApplyViewOverridesToMeshDrawCommands(*ShadowDepthView, VisibleMeshDrawCommands);
			SortAndMergeDynamicPassMeshDrawCommands(SceneRenderer->FeatureLevel, VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage, PrimitiveIdVertexBuffer, 1);
			SubmitMeshDrawCommands(VisibleMeshDrawCommands, PrimitiveIdVertexBuffer, 0, bDynamicInstancing, 1, RHICmdList);
		}
	}
}

/** Pixel shader used to filter a single volume lighting cascade. */
class FFilterTranslucentVolumePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFilterTranslucentVolumePS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}

	FFilterTranslucentVolumePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		TexelSize.Bind(Initializer.ParameterMap, TEXT("TexelSize"));
		TranslucencyLightingVolumeAmbient.Bind(Initializer.ParameterMap, TEXT("TranslucencyLightingVolumeAmbient"));
		TranslucencyLightingVolumeAmbientSampler.Bind(Initializer.ParameterMap, TEXT("TranslucencyLightingVolumeAmbientSampler"));
		TranslucencyLightingVolumeDirectional.Bind(Initializer.ParameterMap, TEXT("TranslucencyLightingVolumeDirectional"));
		TranslucencyLightingVolumeDirectionalSampler.Bind(Initializer.ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalSampler"));
	}
	FFilterTranslucentVolumePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, int32 VolumeCascadeIndex, const int32 ViewIndex)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
		SetShaderValue(RHICmdList, ShaderRHI, TexelSize, 1.0f / TranslucencyLightingVolumeDim);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI, 
			TranslucencyLightingVolumeAmbient, 
			TranslucencyLightingVolumeAmbientSampler, 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			SceneContext.TranslucencyLightingVolumeAmbient[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex]->GetRenderTargetItem().ShaderResourceTexture);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI, 
			TranslucencyLightingVolumeDirectional, 
			TranslucencyLightingVolumeDirectionalSampler, 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			SceneContext.TranslucencyLightingVolumeDirectional[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex]->GetRenderTargetItem().ShaderResourceTexture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TexelSize;
		Ar << TranslucencyLightingVolumeAmbient;
		Ar << TranslucencyLightingVolumeAmbientSampler;
		Ar << TranslucencyLightingVolumeDirectional;
		Ar << TranslucencyLightingVolumeDirectionalSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter TexelSize;
	FShaderResourceParameter TranslucencyLightingVolumeAmbient;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientSampler;
	FShaderResourceParameter TranslucencyLightingVolumeDirectional;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalSampler;
};

IMPLEMENT_SHADER_TYPE(,FFilterTranslucentVolumePS,TEXT("/Engine/Private/TranslucentLightingShaders.usf"),TEXT("FilterMainPS"),SF_Pixel);

/** Shader parameters needed to inject direct lighting into a volume. */
class FTranslucentInjectParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		WorldToShadowMatrix.Bind(ParameterMap,TEXT("WorldToShadowMatrix"));
		ShadowmapMinMax.Bind(ParameterMap,TEXT("ShadowmapMinMax"));
		VolumeCascadeIndex.Bind(ParameterMap,TEXT("VolumeCascadeIndex"));
	}

	template<typename ShaderRHIParamRef>
	void Set(
		FRHICommandList& RHICmdList, 
		const ShaderRHIParamRef ShaderRHI, 
		FShader* Shader, 
		const FViewInfo& View, 
		const FLightSceneInfo* LightSceneInfo, 
		const FProjectedShadowInfo* ShadowMap, 
		uint32 VolumeCascadeIndexValue,
		bool bDynamicallyShadowed) const
	{
		SetDeferredLightParameters(RHICmdList, ShaderRHI, Shader->GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);

		if (bDynamicallyShadowed)
		{
			FVector4 ShadowmapMinMaxValue;
			FMatrix WorldToShadowMatrixValue = ShadowMap->GetWorldToShadowMatrix(ShadowmapMinMaxValue);

			SetShaderValue(RHICmdList, ShaderRHI, WorldToShadowMatrix, WorldToShadowMatrixValue);
			SetShaderValue(RHICmdList, ShaderRHI, ShadowmapMinMax, ShadowmapMinMaxValue);
		}

		SetShaderValue(RHICmdList, ShaderRHI, VolumeCascadeIndex, VolumeCascadeIndexValue);
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FTranslucentInjectParameters& P)
	{
		Ar << P.WorldToShadowMatrix;
		Ar << P.ShadowmapMinMax;
		Ar << P.VolumeCascadeIndex;
		return Ar;
	}

private:

	FShaderParameter WorldToShadowMatrix;
	FShaderParameter ShadowmapMinMax;
	FShaderParameter VolumeCascadeIndex;
};

/** Pixel shader used to accumulate per-object translucent shadows into a volume texture. */
class FTranslucentObjectShadowingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTranslucentObjectShadowingPS,Global);
public:

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INJECTION_PIXEL_SHADER"), 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}

	FTranslucentObjectShadowingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		TranslucentInjectParameters.Bind(Initializer.ParameterMap);
	}
	FTranslucentObjectShadowingPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowMap, uint32 VolumeCascadeIndex)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		TranslucentInjectParameters.Set(RHICmdList, GetPixelShader(), this, View, LightSceneInfo, ShadowMap, VolumeCascadeIndex, true);

		FTranslucentSelfShadowUniformParameters TranslucentSelfShadowUniformParameters;
		SetupTranslucentSelfShadowUniformParameters(ShadowMap, TranslucentSelfShadowUniformParameters);
		SetUniformBufferParameterImmediate(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FTranslucentSelfShadowUniformParameters>(), TranslucentSelfShadowUniformParameters);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TranslucentInjectParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FTranslucentInjectParameters TranslucentInjectParameters;
};

IMPLEMENT_SHADER_TYPE(,FTranslucentObjectShadowingPS,TEXT("/Engine/Private/TranslucentLightingShaders.usf"),TEXT("PerObjectShadowingMainPS"),SF_Pixel);

/** Shader that adds direct lighting contribution from the given light to the current volume lighting cascade. */
template<ELightComponentType InjectionType, bool bDynamicallyShadowed, bool bApplyLightFunction, bool bInverseSquared>
class TTranslucentLightingInjectPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(TTranslucentLightingInjectPS,Material);
public:

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RADIAL_ATTENUATION"), (uint32)(InjectionType != LightType_Directional));
		OutEnvironment.SetDefine(TEXT("INJECTION_PIXEL_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("DYNAMICALLY_SHADOWED"), (uint32)bDynamicallyShadowed);
		OutEnvironment.SetDefine(TEXT("APPLY_LIGHT_FUNCTION"), (uint32)bApplyLightFunction);
		OutEnvironment.SetDefine(TEXT("INVERSE_SQUARED_FALLOFF"), (uint32)bInverseSquared);
	}

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material)
	{
		return (Material->IsLightFunction() || Material->IsSpecialEngineMaterial()) && (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform)));
	}

	TTranslucentLightingInjectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		VolumeShadowingParameters.Bind(Initializer.ParameterMap);
		SpotlightMask.Bind(Initializer.ParameterMap, TEXT("SpotlightMask"));
		LightFunctionParameters.Bind(Initializer.ParameterMap);
		TranslucentInjectParameters.Bind(Initializer.ParameterMap);
		LightFunctionWorldToLight.Bind(Initializer.ParameterMap, TEXT("LightFunctionWorldToLight"));
	}
	TTranslucentLightingInjectPS() {}

	// @param InnerSplitIndex which CSM shadow map level, INDEX_NONE if no directional light
	// @param VolumeCascadeIndexValue which volume we render to
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FLightSceneInfo* LightSceneInfo, 
		const FMaterialRenderProxy* MaterialProxy, 
		const FProjectedShadowInfo* ShadowMap, 
		int32 InnerSplitIndex, 
		int32 VolumeCascadeIndexValue)
	{
		check(ShadowMap || !bDynamicallyShadowed);
		
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneTextureSetupMode::All);
		
		VolumeShadowingParameters.Set(RHICmdList, ShaderRHI, View, LightSceneInfo, ShadowMap, InnerSplitIndex, bDynamicallyShadowed);

		bool bIsSpotlight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
		//@todo - needs to be a permutation to reduce shadow filtering work
		SetShaderValue(RHICmdList, ShaderRHI, SpotlightMask, (bIsSpotlight ? 1.0f : 0.0f));

		LightFunctionParameters.Set(RHICmdList, ShaderRHI, LightSceneInfo, 1);
		TranslucentInjectParameters.Set(RHICmdList, ShaderRHI, this, View, LightSceneInfo, ShadowMap, VolumeCascadeIndexValue, bDynamicallyShadowed);

		if (LightFunctionWorldToLight.IsBound())
		{
			const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
			// Switch x and z so that z of the user specified scale affects the distance along the light direction
			const FVector InverseScale = FVector( 1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X );
			const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));	

			SetShaderValue(RHICmdList, ShaderRHI, LightFunctionWorldToLight, WorldToLight);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << VolumeShadowingParameters;
		Ar << SpotlightMask;
		Ar << LightFunctionParameters;
		Ar << TranslucentInjectParameters;
		Ar << LightFunctionWorldToLight;
		return bShaderHasOutdatedParameters;
	}

private:
	FVolumeShadowingParameters VolumeShadowingParameters;
	FShaderParameter SpotlightMask;
	FLightFunctionSharedParameters LightFunctionParameters;
	FTranslucentInjectParameters TranslucentInjectParameters;
	FShaderParameter LightFunctionWorldToLight;
};

#define IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType,bDynamicallyShadowed,bApplyLightFunction,bInverseSquared) \
	typedef TTranslucentLightingInjectPS<LightType,bDynamicallyShadowed,bApplyLightFunction,bInverseSquared> TTranslucentLightingInjectPS##LightType##bDynamicallyShadowed##bApplyLightFunction##bInverseSquared; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucentLightingInjectPS##LightType##bDynamicallyShadowed##bApplyLightFunction##bInverseSquared,TEXT("/Engine/Private/TranslucentLightInjectionShaders.usf"),TEXT("InjectMainPS"),SF_Pixel);

/** Versions with a light function. */
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Directional,true,true,false); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Directional,false,true,false); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,true,true,true); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,false,true,true); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,true,true,false); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,false,true,false); 

/** Versions without a light function. */
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Directional,true,false,false); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Directional,false,false,false); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,true,false,true); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,false,false,true); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,true,false,false); 
IMPLEMENT_INJECTION_PIXELSHADER_TYPE(LightType_Point,false,false,false); 

void FDeferredShadingSceneRenderer::ClearTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList, int32 ViewIndex)
{
	if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ClearTranslucentVolumeLighting);
		SCOPED_GPU_STAT(RHICmdList, TranslucentLighting);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SceneContext.ClearTranslucentVolumeLighting(RHICmdList, ViewIndex);		
	}
}

class FClearTranslucentLightingVolumeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FClearTranslucentLightingVolumeCS, Global)
public:

	static const int32 CLEAR_BLOCK_SIZE = 4;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CLEAR_COMPUTE_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("CLEAR_BLOCK_SIZE"), CLEAR_BLOCK_SIZE);
	}

	FClearTranslucentLightingVolumeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Ambient0.Bind(Initializer.ParameterMap, TEXT("Ambient0"));
		Directional0.Bind(Initializer.ParameterMap, TEXT("Directional0"));
		Ambient1.Bind(Initializer.ParameterMap, TEXT("Ambient1"));
		Directional1.Bind(Initializer.ParameterMap, TEXT("Directional1"));
	}

	FClearTranslucentLightingVolumeCS()
	{
	}

	void SetParameters(
		FRHIAsyncComputeCommandListImmediate& RHICmdList,
		FUnorderedAccessViewRHIParamRef* VolumeUAVs,
		int32 NumUAVs
	)
	{
		check(NumUAVs == 4);
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		Ambient0.SetTexture(RHICmdList, ShaderRHI, NULL, VolumeUAVs[0]);
		Directional0.SetTexture(RHICmdList, ShaderRHI, NULL, VolumeUAVs[1]);
		Ambient1.SetTexture(RHICmdList, ShaderRHI, NULL, VolumeUAVs[2]);
		Directional1.SetTexture(RHICmdList, ShaderRHI, NULL, VolumeUAVs[3]);
	}

	void UnsetParameters(FRHIAsyncComputeCommandListImmediate& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		Ambient0.UnsetUAV(RHICmdList, ShaderRHI);
		Directional0.UnsetUAV(RHICmdList, ShaderRHI);
		Ambient1.UnsetUAV(RHICmdList, ShaderRHI);
		Directional1.UnsetUAV(RHICmdList, ShaderRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Ambient0;
		Ar << Directional0;
		Ar << Ambient1;
		Ar << Directional1;
		return bShaderHasOutdatedParameters;
	}

private:
	FRWShaderParameter Ambient0;
	FRWShaderParameter Directional0;
	FRWShaderParameter Ambient1;
	FRWShaderParameter Directional1;
};

IMPLEMENT_SHADER_TYPE(, FClearTranslucentLightingVolumeCS, TEXT("/Engine/Private/TranslucentLightInjectionShaders.usf"), TEXT("ClearTranslucentLightingVolumeCS"), SF_Compute)

void FDeferredShadingSceneRenderer::ClearTranslucentVolumeLightingAsyncCompute(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const int32 NumUAVs = 4;

	for(int i = 0; i < Views.Num(); ++i)
	{
		FUnorderedAccessViewRHIParamRef VolumeUAVs[4] = {
			SceneContext.TranslucencyLightingVolumeAmbient[(i * NumTranslucentVolumeRenderTargetSets)]->GetRenderTargetItem().UAV,
			SceneContext.TranslucencyLightingVolumeDirectional[(i * NumTranslucentVolumeRenderTargetSets)]->GetRenderTargetItem().UAV,
			SceneContext.TranslucencyLightingVolumeAmbient[(i * NumTranslucentVolumeRenderTargetSets) + 1]->GetRenderTargetItem().UAV,
			SceneContext.TranslucencyLightingVolumeDirectional[(i * NumTranslucentVolumeRenderTargetSets) + 1]->GetRenderTargetItem().UAV
		};

		FClearTranslucentLightingVolumeCS* ComputeShader = *TShaderMapRef<FClearTranslucentLightingVolumeCS>(GetGlobalShaderMap(FeatureLevel));
		static const FName EndComputeFenceName(TEXT("TranslucencyLightingVolumeClearEndComputeFence"));
		TranslucencyLightingVolumeClearEndFence = RHICmdList.CreateComputeFence(EndComputeFenceName);

		static const FName BeginComputeFenceName(TEXT("TranslucencyLightingVolumeClearBeginComputeFence"));
		FComputeFenceRHIRef ClearBeginFence = RHICmdList.CreateComputeFence(BeginComputeFenceName);

		//write fence on the Gfx pipe so the async clear compute shader won't clear until the Gfx pipe is caught up.
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, VolumeUAVs, NumUAVs, ClearBeginFence);

		const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

		//Grab the async compute commandlist.
		FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
		{
			SCOPED_COMPUTE_EVENTF(RHICmdListComputeImmediate, ClearTranslucencyLightingVolume, TEXT("ClearTranslucencyLightingVolumeCompute %d"), TranslucencyLightingVolumeDim);

			//we must wait on the fence written from the Gfx pipe to let us know all our dependencies are ready.
			RHICmdListComputeImmediate.WaitComputeFence(ClearBeginFence);

			//standard compute setup, but on the async commandlist.
			RHICmdListComputeImmediate.SetComputeShader(ComputeShader->GetComputeShader());

			ComputeShader->SetParameters(RHICmdListComputeImmediate, VolumeUAVs, NumUAVs);
		
			int32 GroupsPerDim = TranslucencyLightingVolumeDim / FClearTranslucentLightingVolumeCS::CLEAR_BLOCK_SIZE;
			DispatchComputeShader(RHICmdListComputeImmediate, ComputeShader, GroupsPerDim, GroupsPerDim, GroupsPerDim);

			ComputeShader->UnsetParameters(RHICmdListComputeImmediate);

			//transition the output to readable and write the fence to allow the Gfx pipe to carry on.
			RHICmdListComputeImmediate.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, VolumeUAVs, NumUAVs, TranslucencyLightingVolumeClearEndFence);
		}

		//immediately dispatch our async compute commands to the RHI thread to be submitted to the GPU as soon as possible.
		//dispatch after the scope so the drawevent pop is inside the dispatch
		FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);

	}
}

/** Encapsulates a pixel shader that is adding ambient cubemap to the volume. */
class FInjectAmbientCubemapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FInjectAmbientCubemapPS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FInjectAmbientCubemapPS() {}

public:
	FCubemapShaderParameters CubemapShaderParameters;

	/** Initialization constructor. */
	FInjectAmbientCubemapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CubemapShaderParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CubemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FFinalPostProcessSettings::FCubemapEntry& CubemapEntry)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		CubemapShaderParameters.SetParameters(RHICmdList, ShaderRHI, CubemapEntry);
	}
};

IMPLEMENT_SHADER_TYPE(,FInjectAmbientCubemapPS,TEXT("/Engine/Private/TranslucentLightingShaders.usf"),TEXT("InjectAmbientCubemapMainPS"),SF_Pixel);

void FDeferredShadingSceneRenderer::InjectAmbientCubemapTranslucentVolumeLighting(FRHICommandList& RHICmdList, const FViewInfo& View, int32 ViewIndex)
{
	if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering && View.FinalPostProcessSettings.ContributingCubemaps.Num())
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SCOPED_DRAW_EVENT(RHICmdList, InjectAmbientCubemapTranslucentVolumeLighting);
		SCOPED_GPU_STAT(RHICmdList, TranslucentLighting);

		const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

		const FVolumeBounds VolumeBounds(TranslucencyLightingVolumeDim);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();

		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			//Checks to detect/prevent UE-31578
			const IPooledRenderTarget* RT0 = SceneContext.TranslucencyLightingVolumeAmbient[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];

			// we don't update the directional volume (could be a HQ option)
			FRHIRenderPassInfo RPInfo(RT0->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_Store);
			TransitionRenderPassTargets(RHICmdList, RPInfo);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("InjectAmbientCubemapTranslucentVolumeLighting"));
			{
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				TShaderMapRef<FWriteToSliceVS> VertexShader(ShaderMap);
				TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(ShaderMap);
				TShaderMapRef<FInjectAmbientCubemapPS> PixelShader(ShaderMap);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
									
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
				if(GeometryShader.IsValid())
				{
					GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
				}

				uint32 Count = View.FinalPostProcessSettings.ContributingCubemaps.Num();
				for(uint32 i = 0; i < Count; ++i)
				{
					const FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = View.FinalPostProcessSettings.ContributingCubemaps[i];

					PixelShader->SetParameters(RHICmdList, View, CubemapEntry);

					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				}
			}
			RHICmdList.EndRenderPass();
			RHICmdList.CopyToResolveTarget(RT0->GetRenderTargetItem().TargetableTexture, RT0->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
		}
	}
}

void FDeferredShadingSceneRenderer::ClearTranslucentVolumePerObjectShadowing(FRHICommandList& RHICmdList, const int32 ViewIndex)
{
	if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SCOPED_DRAW_EVENT(RHICmdList, ClearTranslucentVolumePerLightShadowing);
		SCOPED_GPU_STAT(RHICmdList, TranslucentLighting);

		static_assert(TVC_MAX == 2, "Only expecting two translucency lighting cascades.");
		FTextureRHIParamRef RenderTargets[2];
		RenderTargets[0] = SceneContext.GetTranslucencyVolumeAmbient(TVC_Inner, ViewIndex)->GetRenderTargetItem().TargetableTexture;
		RenderTargets[1] = SceneContext.GetTranslucencyVolumeDirectional(TVC_Inner, ViewIndex)->GetRenderTargetItem().TargetableTexture;

		FLinearColor ClearColors[2];
		ClearColors[0] = FLinearColor(1, 1, 1, 1);
		ClearColors[1] = FLinearColor(1, 1, 1, 1);

		FSceneRenderTargets::ClearVolumeTextures<ARRAY_COUNT(RenderTargets)>(RHICmdList, FeatureLevel, RenderTargets, ClearColors);
	}
}

/** Calculates volume texture bounds for the given light in the given translucent lighting volume cascade. */
FVolumeBounds CalculateLightVolumeBounds(const FSphere& LightBounds, const FViewInfo& View, uint32 VolumeCascadeIndex, bool bDirectionalLight)
{
	const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

	FVolumeBounds VolumeBounds;

	if (bDirectionalLight)
	{
		VolumeBounds = FVolumeBounds(TranslucencyLightingVolumeDim);
	}
	else
	{
		// Determine extents in the volume texture
		const FVector MinPosition = (LightBounds.Center - LightBounds.W - View.TranslucencyLightingVolumeMin[VolumeCascadeIndex]) / View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];
		const FVector MaxPosition = (LightBounds.Center + LightBounds.W - View.TranslucencyLightingVolumeMin[VolumeCascadeIndex]) / View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];

		VolumeBounds.MinX = FMath::Max(FMath::TruncToInt(MinPosition.X), 0);
		VolumeBounds.MinY = FMath::Max(FMath::TruncToInt(MinPosition.Y), 0);
		VolumeBounds.MinZ = FMath::Max(FMath::TruncToInt(MinPosition.Z), 0);

		VolumeBounds.MaxX = FMath::Min(FMath::TruncToInt(MaxPosition.X) + 1, TranslucencyLightingVolumeDim);
		VolumeBounds.MaxY = FMath::Min(FMath::TruncToInt(MaxPosition.Y) + 1, TranslucencyLightingVolumeDim);
		VolumeBounds.MaxZ = FMath::Min(FMath::TruncToInt(MaxPosition.Z) + 1, TranslucencyLightingVolumeDim);
	}

	return VolumeBounds;
}

void FDeferredShadingSceneRenderer::AccumulateTranslucentVolumeObjectShadowing(FRHICommandList& RHICmdList, const FProjectedShadowInfo* InProjectedShadowInfo, bool bClearVolume, const FViewInfo& View, const int32 ViewIndex)
{
	const FLightSceneInfo* LightSceneInfo = &InProjectedShadowInfo->GetLightSceneInfo();

	if (bClearVolume)
	{
		ClearTranslucentVolumePerObjectShadowing(RHICmdList, ViewIndex);
	}

	if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
	{
		SCOPED_DRAW_EVENT(RHICmdList, AccumulateTranslucentVolumeShadowing);
		SCOPED_GPU_STAT(RHICmdList, TranslucentLighting);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		// Inject into each volume cascade
		for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			const bool bDirectionalLight = LightSceneInfo->Proxy->GetLightType() == LightType_Directional;
			const FVolumeBounds VolumeBounds = CalculateLightVolumeBounds(LightSceneInfo->Proxy->GetBoundingSphere(), View, VolumeCascadeIndex, bDirectionalLight);

			if (VolumeBounds.IsValid())
			{
				FTextureRHIParamRef RenderTarget;

				if (VolumeCascadeIndex == 0)
				{
					RenderTarget = SceneContext.GetTranslucencyVolumeAmbient(TVC_Inner, ViewIndex)->GetRenderTargetItem().TargetableTexture;
				}
				else
				{
					RenderTarget = SceneContext.GetTranslucencyVolumeDirectional(TVC_Inner, ViewIndex)->GetRenderTargetItem().TargetableTexture;
				}

				FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("AccumulateVolumeObjectShadowing"));
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

					// Modulate the contribution of multiple object shadows in rgb
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI();

					TShaderMapRef<FWriteToSliceVS> VertexShader(ShaderMap);
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(ShaderMap);
					TShaderMapRef<FTranslucentObjectShadowingPS> PixelShader(ShaderMap);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

					VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
					if(GeometryShader.IsValid())
					{
						GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
					}
					PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, InProjectedShadowInfo, VolumeCascadeIndex);
				
					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				}
				RHICmdList.EndRenderPass();

				RHICmdList.CopyToResolveTarget(SceneContext.GetTranslucencyVolumeAmbient((ETranslucencyVolumeCascade)VolumeCascadeIndex, ViewIndex)->GetRenderTargetItem().TargetableTexture,
					SceneContext.GetTranslucencyVolumeAmbient((ETranslucencyVolumeCascade)VolumeCascadeIndex, ViewIndex)->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
			}
		}
	}
}

/**
 * Helper function for finding and setting the right version of TTranslucentLightingInjectPS given template parameters.
 * @param MaterialProxy must not be 0
 * @param InnerSplitIndex todo: get from ShadowMap, INDEX_NONE if no directional light
 */
template<ELightComponentType InjectionType, bool bDynamicallyShadowed>
void SetInjectionShader(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	const FViewInfo& View, 
	const FMaterialRenderProxy* MaterialProxy,
	const FLightSceneInfo* LightSceneInfo, 
	const FProjectedShadowInfo* ShadowMap, 
	int32 InnerSplitIndex, 
	int32 VolumeCascadeIndexValue,
	FWriteToSliceVS* VertexShader,
	FWriteToSliceGS* GeometryShader,
	bool bApplyLightFunction,
	bool bInverseSquared)
{
	check(ShadowMap || !bDynamicallyShadowed);

	const FMaterialShaderMap* MaterialShaderMap = MaterialProxy->GetMaterial(View.GetFeatureLevel())->GetRenderingThreadShaderMap();
	FMaterialShader* PixelShader = NULL;

	const bool Directional = InjectionType == LightType_Directional;

	if (bApplyLightFunction)
	{
		if( bInverseSquared )
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, true, true && !Directional> >();

			check(InjectionPixelShader);
			PixelShader = InjectionPixelShader;
		}
		else
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, true, false> >();

			check(InjectionPixelShader);
			PixelShader = InjectionPixelShader;
		}
	}
	else
	{
		if( bInverseSquared )
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, false, true && !Directional> >();

			check(InjectionPixelShader);
			PixelShader = InjectionPixelShader;
		}
		else
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, false, false> >();

			check(InjectionPixelShader);
			PixelShader = InjectionPixelShader;
		}
	}
	
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
	GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(GeometryShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Now shader is set, bind parameters
	if (bApplyLightFunction)
	{
		if( bInverseSquared )
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, true, true && !Directional> >();
			check(InjectionPixelShader);
			InjectionPixelShader->SetParameters(RHICmdList, View, LightSceneInfo, MaterialProxy, ShadowMap, InnerSplitIndex, VolumeCascadeIndexValue);
		}
		else
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, true, false> >();
			check(InjectionPixelShader);
			InjectionPixelShader->SetParameters(RHICmdList, View, LightSceneInfo, MaterialProxy, ShadowMap, InnerSplitIndex, VolumeCascadeIndexValue);
		}
	}
	else
	{
		if( bInverseSquared )
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, false, true && !Directional> >();
			check(InjectionPixelShader);
			InjectionPixelShader->SetParameters(RHICmdList, View, LightSceneInfo, MaterialProxy, ShadowMap, InnerSplitIndex, VolumeCascadeIndexValue);
		}
		else
		{
			auto InjectionPixelShader = MaterialShaderMap->GetShader< TTranslucentLightingInjectPS<InjectionType, bDynamicallyShadowed, false, false> >();
			check(InjectionPixelShader);
			InjectionPixelShader->SetParameters(RHICmdList, View, LightSceneInfo, MaterialProxy, ShadowMap, InnerSplitIndex, VolumeCascadeIndexValue);
		}
	}
}

/** 
 * Information about a light to be injected.
 * Cached in this struct to avoid recomputing multiple times (multiple cascades).
 */
struct FTranslucentLightInjectionData
{
	// must not be 0
	const FLightSceneInfo* LightSceneInfo;
	// can be 0
	const FProjectedShadowInfo* ProjectedShadowInfo;
	//
	bool bApplyLightFunction;
	// must not be 0
	const FMaterialRenderProxy* LightFunctionMaterialProxy;
};

/**
 * Adds a light to LightInjectionData if it should be injected into the translucent volume, and caches relevant information in a FTranslucentLightInjectionData.
 * @param InProjectedShadowInfo is 0 for unshadowed lights
 */
static void AddLightForInjection(
	FDeferredShadingSceneRenderer& SceneRenderer,
	const FLightSceneInfo& LightSceneInfo, 
	const FProjectedShadowInfo* InProjectedShadowInfo,
	TArray<FTranslucentLightInjectionData, SceneRenderingAllocator>& LightInjectionData)
{
	if (LightSceneInfo.Proxy->AffectsTranslucentLighting())
	{
		const FVisibleLightInfo& VisibleLightInfo = SceneRenderer.VisibleLightInfos[LightSceneInfo.Id];

		const ERHIFeatureLevel::Type FeatureLevel = SceneRenderer.Scene->GetFeatureLevel();

		const bool bApplyLightFunction = (SceneRenderer.ViewFamily.EngineShowFlags.LightFunctions &&
			LightSceneInfo.Proxy->GetLightFunctionMaterial() && 
			LightSceneInfo.Proxy->GetLightFunctionMaterial()->GetMaterial(FeatureLevel)->IsLightFunction());

		const FMaterialRenderProxy* MaterialProxy = bApplyLightFunction ? 
			LightSceneInfo.Proxy->GetLightFunctionMaterial() : 
			UMaterial::GetDefaultMaterial(MD_LightFunction)->GetRenderProxy();

		// Skip rendering if the DefaultLightFunctionMaterial isn't compiled yet
		if (MaterialProxy->GetMaterial(FeatureLevel)->IsLightFunction())
		{
			FTranslucentLightInjectionData InjectionData;
			InjectionData.LightSceneInfo = &LightSceneInfo;
			InjectionData.ProjectedShadowInfo = InProjectedShadowInfo;
			InjectionData.bApplyLightFunction = bApplyLightFunction;
			InjectionData.LightFunctionMaterialProxy = MaterialProxy;
			LightInjectionData.Add(InjectionData);
		}
	}
}

/** Injects all the lights in LightInjectionData into the translucent lighting volume textures. */
static void InjectTranslucentLightArray(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const TArray<FTranslucentLightInjectionData, SceneRenderingAllocator>& LightInjectionData, int32 ViewIndex)
{
	check(RHICmdList.IsOutsideRenderPass());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucency, LightInjectionData.Num());

	// Inject into each volume cascade
	// Operate on one cascade at a time to reduce render target switches
	for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
	{
		const IPooledRenderTarget* RT0 = SceneContext.TranslucencyLightingVolumeAmbient[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];
		const IPooledRenderTarget* RT1 = SceneContext.TranslucencyLightingVolumeDirectional[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];

		GVisualizeTexture.SetCheckPoint(RHICmdList, RT0);
		GVisualizeTexture.SetCheckPoint(RHICmdList, RT1);

		FTextureRHIParamRef RenderTargets[2];
		RenderTargets[0] = RT0->GetRenderTargetItem().TargetableTexture;
		RenderTargets[1] = RT1->GetRenderTargetItem().TargetableTexture;

		FRHIRenderPassInfo RPInfo(ARRAY_COUNT(RenderTargets), RenderTargets, ERenderTargetActions::Load_Store);
		TransitionRenderPassTargets(RHICmdList, RPInfo);

		RHICmdList.BeginRenderPass(RPInfo, TEXT("InjectTranslucentLightArray"));
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			for (int32 LightIndex = 0; LightIndex < LightInjectionData.Num(); LightIndex++)
			{
				const FTranslucentLightInjectionData& InjectionData = LightInjectionData[LightIndex];
				const FLightSceneInfo* const LightSceneInfo = InjectionData.LightSceneInfo;
				const bool bInverseSquared = LightSceneInfo->Proxy->IsInverseSquared();
				const bool bDirectionalLight = LightSceneInfo->Proxy->GetLightType() == LightType_Directional;
				const FVolumeBounds VolumeBounds = CalculateLightVolumeBounds(LightSceneInfo->Proxy->GetBoundingSphere(), View, VolumeCascadeIndex, bDirectionalLight);

				if (VolumeBounds.IsValid())
				{
					TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);

					if (bDirectionalLight)
					{
						// Accumulate the contribution of multiple lights
						// Directional lights write their shadowing into alpha of the ambient texture
						GraphicsPSOInit.BlendState = TStaticBlendState<
							CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
							CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();

						if (InjectionData.ProjectedShadowInfo)
						{
							// shadows, restricting light contribution to the cascade bounds (except last cascade far to get light functions and no shadows there)
							SetInjectionShader<LightType_Directional, true>(RHICmdList, GraphicsPSOInit, View, InjectionData.LightFunctionMaterialProxy, LightSceneInfo,
								InjectionData.ProjectedShadowInfo, InjectionData.ProjectedShadowInfo->CascadeSettings.ShadowSplitIndex, VolumeCascadeIndex,
								*VertexShader, *GeometryShader, InjectionData.bApplyLightFunction, false);
						}
						else
						{
							// no shadows
							SetInjectionShader<LightType_Directional, false>(RHICmdList, GraphicsPSOInit, View, InjectionData.LightFunctionMaterialProxy, LightSceneInfo,
								InjectionData.ProjectedShadowInfo, -1, VolumeCascadeIndex,
								*VertexShader, *GeometryShader, InjectionData.bApplyLightFunction, false);
						}
					}
					else
					{
						// Accumulate the contribution of multiple lights
						GraphicsPSOInit.BlendState = TStaticBlendState<
							CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One,
							CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();

						if (InjectionData.ProjectedShadowInfo)
						{
							SetInjectionShader<LightType_Point, true>(RHICmdList, GraphicsPSOInit, View, InjectionData.LightFunctionMaterialProxy, LightSceneInfo,
								InjectionData.ProjectedShadowInfo, -1, VolumeCascadeIndex,
								*VertexShader, *GeometryShader, InjectionData.bApplyLightFunction, bInverseSquared);
						}
						else
						{
							SetInjectionShader<LightType_Point, false>(RHICmdList, GraphicsPSOInit, View, InjectionData.LightFunctionMaterialProxy, LightSceneInfo,
								InjectionData.ProjectedShadowInfo, -1, VolumeCascadeIndex,
								*VertexShader, *GeometryShader, InjectionData.bApplyLightFunction, bInverseSquared);
						}
					}

					const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

					VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
					if(GeometryShader.IsValid())
					{
						GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
					}
					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				}
			}
		}
		RHICmdList.EndRenderPass();
		RHICmdList.CopyToResolveTarget(RT0->GetRenderTargetItem().TargetableTexture, RT0->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
		RHICmdList.CopyToResolveTarget(RT1->GetRenderTargetItem().TargetableTexture, RT1->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	}
}

void FDeferredShadingSceneRenderer::InjectTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo, const FProjectedShadowInfo* InProjectedShadowInfo, const FViewInfo& View, int32 ViewIndex)
{
	if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
	{
		SCOPE_CYCLE_COUNTER(STAT_TranslucentInjectTime);

		TArray<FTranslucentLightInjectionData, SceneRenderingAllocator> LightInjectionData;

		AddLightForInjection(*this, LightSceneInfo, InProjectedShadowInfo, LightInjectionData);

		// shadowed or unshadowed (InProjectedShadowInfo==0)
		InjectTranslucentLightArray(RHICmdList, View, LightInjectionData, ViewIndex);
	}
}

void FDeferredShadingSceneRenderer::InjectTranslucentVolumeLightingArray(FRHICommandListImmediate& RHICmdList, const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, int32 NumLights)
{
	SCOPE_CYCLE_COUNTER(STAT_TranslucentInjectTime);

	TArray<FTranslucentLightInjectionData, SceneRenderingAllocator> *LightInjectionData = new TArray<FTranslucentLightInjectionData, SceneRenderingAllocator>[Views.Num()];
	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		LightInjectionData[ViewIndex].Empty(NumLights);
	}
	

	for (int32 LightIndex = 0; LightIndex < NumLights; LightIndex++)
	{
		const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
		const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if(LightSceneInfo->ShouldRenderLight(Views[ViewIndex]))
			{
				AddLightForInjection(*this, *LightSceneInfo, NULL, LightInjectionData[ViewIndex]);
			}
		}
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		// non-shadowed, non-light function lights
		InjectTranslucentLightArray(RHICmdList, Views[ViewIndex], LightInjectionData[ViewIndex], ViewIndex);
	}

	delete[] LightInjectionData;
}

/** Pixel shader used to inject simple lights into the translucent lighting volume */
class FSimpleLightTranslucentLightingInjectPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleLightTranslucentLightingInjectPS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}

	FSimpleLightTranslucentLightingInjectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		VolumeCascadeIndex.Bind(Initializer.ParameterMap, TEXT("VolumeCascadeIndex"));
		SimpleLightPositionAndRadius.Bind(Initializer.ParameterMap, TEXT("SimpleLightPositionAndRadius"));
		SimpleLightColorAndExponent.Bind(Initializer.ParameterMap, TEXT("SimpleLightColorAndExponent"));
	}
	FSimpleLightTranslucentLightingInjectPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FSimpleLightEntry& SimpleLight, const FSimpleLightPerViewEntry& SimpleLightPerViewData, int32 VolumeCascadeIndexValue)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

		FVector4 PositionAndRadius(SimpleLightPerViewData.Position, SimpleLight.Radius);
		SetShaderValue(RHICmdList, GetPixelShader(), VolumeCascadeIndex, VolumeCascadeIndexValue);
		SetShaderValue(RHICmdList, GetPixelShader(), SimpleLightPositionAndRadius, PositionAndRadius);

		FVector4 LightColorAndExponent(SimpleLight.Color, SimpleLight.Exponent);

		SetShaderValue(RHICmdList, GetPixelShader(), SimpleLightColorAndExponent, LightColorAndExponent);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumeCascadeIndex;
		Ar << SimpleLightPositionAndRadius;
		Ar << SimpleLightColorAndExponent;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter VolumeCascadeIndex;
	FShaderParameter SimpleLightPositionAndRadius;
	FShaderParameter SimpleLightColorAndExponent;
};

IMPLEMENT_SHADER_TYPE(,FSimpleLightTranslucentLightingInjectPS,TEXT("/Engine/Private/TranslucentLightInjectionShaders.usf"),TEXT("SimpleLightInjectMainPS"),SF_Pixel);

void FDeferredShadingSceneRenderer::InjectSimpleTranslucentVolumeLightingArray(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights, const FViewInfo& View, const int32 ViewIndex)
{
	check(RHICmdList.IsOutsideRenderPass());
	SCOPE_CYCLE_COUNTER(STAT_TranslucentInjectTime);

	int32 NumLightsToInject = 0;

	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
	{
		if (SimpleLights.InstanceData[LightIndex].bAffectTranslucency)
		{
			NumLightsToInject++;
		}
	}

	if (NumLightsToInject > 0)
	{
		INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucency, NumLightsToInject);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// Inject into each volume cascade
		// Operate on one cascade at a time to reduce render target switches
		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			const IPooledRenderTarget* RT0 = SceneContext.TranslucencyLightingVolumeAmbient[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];
			const IPooledRenderTarget* RT1 = SceneContext.TranslucencyLightingVolumeDirectional[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];

			GVisualizeTexture.SetCheckPoint(RHICmdList, RT0);
			GVisualizeTexture.SetCheckPoint(RHICmdList, RT1);

			FTextureRHIParamRef RenderTargets[2];
			RenderTargets[0] = RT0->GetRenderTargetItem().TargetableTexture;
			RenderTargets[1] = RT1->GetRenderTargetItem().TargetableTexture;

			FRHIRenderPassInfo RPInfo(ARRAY_COUNT(RenderTargets), RenderTargets, ERenderTargetActions::Load_Store);
			TransitionRenderPassTargets(RHICmdList, RPInfo);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("InjectSimpleTranslucentVolumeLightingArray"));
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				// Accumulate the contribution of multiple lights
				GraphicsPSOInit.BlendState = TStaticBlendState<
					CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
				GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		
				for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
				{
					const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[LightIndex];
					const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, Views.Num());

					if (SimpleLight.bAffectTranslucency)
					{
						const FSphere LightBounds(SimpleLightPerViewData.Position, SimpleLight.Radius);
						const FVolumeBounds VolumeBounds = CalculateLightVolumeBounds(LightBounds, View, VolumeCascadeIndex, false);

						if (VolumeBounds.IsValid())
						{
							TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
							TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
							TShaderMapRef<FSimpleLightTranslucentLightingInjectPS> PixelShader(View.ShaderMap);

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
							GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

							const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

							VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
							if(GeometryShader.IsValid())
							{
								GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
							}
							PixelShader->SetParameters(RHICmdList, View, SimpleLight, SimpleLightPerViewData, VolumeCascadeIndex);



							RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
						}
					}
				}
			}
			RHICmdList.EndRenderPass();
			RHICmdList.CopyToResolveTarget(RT0->GetRenderTargetItem().TargetableTexture, RT0->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
			RHICmdList.CopyToResolveTarget(RT1->GetRenderTargetItem().TargetableTexture, RT1->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
		}
	}
}

void FDeferredShadingSceneRenderer::FilterTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const int32 ViewIndex)
{
	if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
#if 0
		// textures have to be finalized before reading.
		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			const IPooledRenderTarget* RT0 = SceneContext.TranslucencyLightingVolumeAmbient[VolumeCascadeIndex];
			const IPooledRenderTarget* RT1 = SceneContext.TranslucencyLightingVolumeDirectional[VolumeCascamdeIndex];
			FTextureRHIRef TargetTexture0 = RT0->GetRenderTargetItem().TargetableTexture;
			FTextureRHIRef TargetTexture1 = RT1->GetRenderTargetItem().TargetableTexture;
			RHICmdList.CopyToResolveTarget(TargetTexture0, TargetTexture0, FResolveParams());
			RHICmdList.CopyToResolveTarget(TargetTexture1, TargetTexture1, FResolveParams());
		}
#endif

		if (GUseTranslucencyVolumeBlur)
		{
			const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
			SCOPED_DRAW_EVENTF(RHICmdList, FilterTranslucentVolume, TEXT("FilterTranslucentVolume %dx%dx%d Cascades:%d"),
				TranslucencyLightingVolumeDim, TranslucencyLightingVolumeDim, TranslucencyLightingVolumeDim, TVC_MAX);

			SCOPED_GPU_STAT(RHICmdList, TranslucentLighting);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			bool bTransitionedToWriteable = (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering && View.FinalPostProcessSettings.ContributingCubemaps.Num());

			// Filter each cascade
			for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
			{
				const IPooledRenderTarget* RT0 = SceneContext.GetTranslucencyVolumeAmbient((ETranslucencyVolumeCascade)VolumeCascadeIndex, ViewIndex);
				const IPooledRenderTarget* RT1 = SceneContext.GetTranslucencyVolumeDirectional((ETranslucencyVolumeCascade)VolumeCascadeIndex, ViewIndex);

				const IPooledRenderTarget* Input0 = SceneContext.TranslucencyLightingVolumeAmbient[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];
				const IPooledRenderTarget* Input1 = SceneContext.TranslucencyLightingVolumeDirectional[VolumeCascadeIndex + NumTranslucentVolumeRenderTargetSets * ViewIndex];

				GVisualizeTexture.SetCheckPoint(RHICmdList, RT0);
				GVisualizeTexture.SetCheckPoint(RHICmdList, RT1);

				FTextureRHIParamRef RenderTargets[2];
				RenderTargets[0] = RT0->GetRenderTargetItem().TargetableTexture;
				RenderTargets[1] = RT1->GetRenderTargetItem().TargetableTexture;

				FTextureRHIParamRef Inputs[2];
				Inputs[0] = Input0->GetRenderTargetItem().TargetableTexture;
				Inputs[1] = Input1->GetRenderTargetItem().TargetableTexture;

				static_assert(TVC_MAX == 2, "Final transition logic should change");

				//the volume textures should still be writable from the injection phase on the first loop.
				if (!bTransitionedToWriteable || VolumeCascadeIndex > 0)
				{
					RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, RenderTargets, 2);
				}
				RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Inputs, 2);

				FRHIRenderPassInfo RPInfo(ARRAY_COUNT(RenderTargets), RenderTargets, ERenderTargetActions::Load_Store);
				TransitionRenderPassTargets(RHICmdList, RPInfo);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("FilterTranslucentVolumeLighting"));
				{
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					const FVolumeBounds VolumeBounds(TranslucencyLightingVolumeDim);
					TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
					TShaderMapRef<FFilterTranslucentVolumePS> PixelShader(View.ShaderMap);
				
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
					if(GeometryShader.IsValid())
					{
						GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
					}
					PixelShader->SetParameters(RHICmdList, View, VolumeCascadeIndex, ViewIndex);

					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				}
				RHICmdList.EndRenderPass();

				//only do readable transition on the final loop since the other ones will do this up front.
				//if (VolumeCascadeIndex == TVC_MAX - 1)
				{					
					RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, RenderTargets, 2);
				}
			}
		}
	}
}
