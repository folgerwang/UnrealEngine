// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "BasePassRendering.h"
#include "StaticMeshDrawList.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarSelectiveBasePassOutputs(
	TEXT("r.SelectiveBasePassOutputs"),
	0,
	TEXT("Enables shaders to only export to relevant rendertargets.\n") \
	TEXT(" 0: Export in all rendertargets.\n") \
	TEXT(" 1: Export only into relevant rendertarget.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarGlobalClipPlane(
	TEXT("r.AllowGlobalClipPlane"),
	0,
	TEXT("Enables mesh shaders to support a global clip plane, needed for planar reflections, which adds about 15% BasePass GPU cost on PS4."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarVertexFoggingForOpaque(
	TEXT("r.VertexFoggingForOpaque"),
	1,
	TEXT("Causes opaque materials to use per-vertex fogging, which costs less and integrates properly with MSAA.  Only supported with forward shading."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarParallelBasePass(
	TEXT("r.ParallelBasePass"),
	1,
	TEXT("Toggles parallel base pass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarRHICmdBasePassDeferredContexts(
	TEXT("r.RHICmdBasePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize base pass command list execution."));

FAutoConsoleTaskPriority CPrio_FSortFrontToBackTask(
	TEXT("TaskGraph.TaskPriorities.SortFrontToBackTask"),
	TEXT("Task and thread priority for FSortFrontToBackTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksBasePass(
	TEXT("r.RHICmdFlushRenderThreadTasksBasePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the base pass. A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksBasePass is > 0 we will flush."));

bool UseSelectiveBasePassOutputs()
{
	return CVarSelectiveBasePassOutputs.GetValueOnAnyThread() == 1;
}

static TAutoConsoleVariable<int32> CVarSupportStationarySkylight(
	TEXT("r.SupportStationarySkylight"),
	1,
	TEXT("Enables Stationary and Dynamic Skylight shader permutations."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSupportAtmosphericFog(
	TEXT("r.SupportAtmosphericFog"),
	1,
	TEXT("Enables AtmosphericFog shader permutations."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSupportLowQualityLightmaps(
	TEXT("r.SupportLowQualityLightmaps"),
	1,
	TEXT("Support low quality lightmap shader permutations"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSupportAllShaderPermutations(
	TEXT("r.SupportAllShaderPermutations"),
	0,
	TEXT("Local user config override to force all shader permutation features on."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

/** Whether to replace lightmap textures with solid colors to visualize the mip-levels. */
bool GVisualizeMipLevels = false;

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FSharedBasePassUniformParameters, TEXT("BasePass"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FOpaqueBasePassUniformParameters, TEXT("OpaqueBasePass"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FTranslucentBasePassUniformParameters, TEXT("TranslucentBasePass"));

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
// BasePass Vertex Shader needs to include hull and domain shaders for tessellation, these only compile for D3D11
#define IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TBasePassVS< LightMapPolicyType, false > TBasePassVS##LightMapPolicyName ; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVS##LightMapPolicyName,TEXT("/Engine/Private/BasePassVertexShader.usf"),TEXT("Main"),SF_Vertex); \
	typedef TBasePassHS< LightMapPolicyType, false > TBasePassHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassHS##LightMapPolicyName,TEXT("/Engine/Private/BasePassTessellationShaders.usf"),TEXT("MainHull"),SF_Hull); \
	typedef TBasePassDS< LightMapPolicyType > TBasePassDS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassDS##LightMapPolicyName,TEXT("/Engine/Private/BasePassTessellationShaders.usf"),TEXT("MainDomain"),SF_Domain); 

#define IMPLEMENT_BASEPASS_VERTEXSHADER_ONLY_TYPE(LightMapPolicyType,LightMapPolicyName,AtmosphericFogShaderName) \
	typedef TBasePassVS<LightMapPolicyType,true> TBasePassVS##LightMapPolicyName##AtmosphericFogShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVS##LightMapPolicyName##AtmosphericFogShaderName,TEXT("/Engine/Private/BasePassVertexShader.usf"),TEXT("Main"),SF_Vertex)	\
	typedef TBasePassHS< LightMapPolicyType, true> TBasePassHS##LightMapPolicyName##AtmosphericFogShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassHS##LightMapPolicyName##AtmosphericFogShaderName,TEXT("/Engine/Private/BasePassTessellationShaders.usf"),TEXT("MainHull"),SF_Hull);

#define IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,bEnableSkyLight,SkyLightName) \
	typedef TBasePassPS<LightMapPolicyType, bEnableSkyLight> TBasePassPS##LightMapPolicyName##SkyLightName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassPS##LightMapPolicyName##SkyLightName,TEXT("/Engine/Private/BasePassPixelShader.usf"),TEXT("MainPS"),SF_Pixel);

// Implement a pixel shader type for skylights and one without, and one vertex shader that will be shared between them
#define IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_BASEPASS_VERTEXSHADER_ONLY_TYPE(LightMapPolicyType,LightMapPolicyName,AtmosphericFog) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,true,Skylight) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,false,);

// Implement shader types per lightmap policy
// If renaming or refactoring these, remember to update FMaterialResource::GetRepresentativeInstructionCounts and FPreviewMaterial::ShouldCache().
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedTranslucencyPolicy, FSelfShadowedTranslucencyPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedCachedPointIndirectLightingPolicy, FSelfShadowedCachedPointIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedVolumetricLightmapPolicy, FSelfShadowedVolumetricLightmapPolicy );

IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>, FPrecomputedVolumetricLightmapLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>, FCachedVolumeIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_CACHED_POINT_INDIRECT_LIGHTING>, FCachedPointIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_SIMPLE_NO_LIGHTMAP>, FSimpleNoLightmapLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING>, FSimpleLightmapOnlyLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING>, FSimpleDirectionalLightLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING>, FSimpleStationaryLightPrecomputedShadowsLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING>, FSimpleStationaryLightSingleSampleShadowsLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING>, FSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>, TLightMapPolicyHQ );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyHQ  );

DECLARE_GPU_STAT(Basepass);

void FBasePassReflectionParameters::SetMesh(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef PixelShaderRHI, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, ERHIFeatureLevel::Type FeatureLevel)
{
	const FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy ? Proxy->GetPrimitiveSceneInfo() : NULL;
	const FPlanarReflectionSceneProxy* PlanarReflectionProxy = NULL;

	if (PrimitiveSceneInfo && PrimitiveSceneInfo->CachedPlanarReflectionProxy)
	{
		PlanarReflectionProxy = PrimitiveSceneInfo->CachedPlanarReflectionProxy;
	}

	PlanarReflectionParameters.SetParameters(RHICmdList, PixelShaderRHI, View, PlanarReflectionProxy);

	int32 ArrayIndex = 0;
	float AverageBrightness = 1.0f;
	const FReflectionCaptureProxy* ReflectionProxy = PrimitiveSceneInfo ? PrimitiveSceneInfo->CachedReflectionCaptureProxy : nullptr;
	FVector4 CaptureOffsetAndAverageBrightnessValue(0, 0, 0, 1);
	FVector4 PositionAndRadius = FVector4(0, 0, 0, 1);
	float Brightness = 1.0f;

	if (PrimitiveSceneInfo && ReflectionProxy && View.Family->EngineShowFlags.ReflectionEnvironment)
	{
		PrimitiveSceneInfo->Scene->GetCaptureParameters(ReflectionProxy, ArrayIndex, AverageBrightness);
		CaptureOffsetAndAverageBrightnessValue = FVector4(ReflectionProxy->CaptureOffset, AverageBrightness);
		PositionAndRadius = FVector4(ReflectionProxy->Position, ReflectionProxy->InfluenceRadius);
		Brightness = ReflectionProxy->Brightness;
	}

	SetShaderValue(RHICmdList, PixelShaderRHI, SingleCubemapArrayIndex, ArrayIndex);
	SetShaderValue(RHICmdList, PixelShaderRHI, SingleCaptureOffsetAndAverageBrightness, CaptureOffsetAndAverageBrightnessValue);
	SetShaderValue(RHICmdList, PixelShaderRHI, SingleCapturePositionAndRadius, PositionAndRadius);
	SetShaderValue(RHICmdList, PixelShaderRHI, SingleCaptureBrightness, Brightness);
}

static void SetDepthStencilStateForBasePass(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View, const FMeshBatch& Mesh, const FPrimitiveSceneProxy* PrimitiveSceneProxy, bool bEnableReceiveDecalOutput, bool bUseDebugViewPS, FDepthStencilStateRHIParamRef LodFadeOverrideDepthStencilState)
{
	static IConsoleVariable* EarlyZPassOnlyMaterialMaskingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPassOnlyMaterialMasking"));
	bool bMaskInEarlyPass = (EarlyZPassOnlyMaterialMaskingCVar && Mesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel())->IsMasked() && EarlyZPassOnlyMaterialMaskingCVar->GetInt());

	if (bEnableReceiveDecalOutput && !bUseDebugViewPS)
	{
		// Set stencil value for this draw call
		// This is effectively extending the GBuffer using the stencil bits
		const uint8 StencilValue = GET_STENCIL_BIT_MASK(RECEIVE_DECAL, PrimitiveSceneProxy ? !!PrimitiveSceneProxy->ReceivesDecals() : 0x00)
			| STENCIL_LIGHTING_CHANNELS_MASK(PrimitiveSceneProxy ? PrimitiveSceneProxy->GetLightingChannelStencilValue() : 0x00);

		if (LodFadeOverrideDepthStencilState != nullptr)
		{
			//@TODO: Handle bMaskInEarlyPass in this case (used when a LODTransition is specified)
			DrawRenderState.SetDepthStencilState(LodFadeOverrideDepthStencilState);
			DrawRenderState.SetStencilRef(StencilValue);
		}
		else if (bMaskInEarlyPass)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				false, CF_Equal,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
			>::GetRHI());
			DrawRenderState.SetStencilRef(StencilValue);
		}
		else if (DrawRenderState.GetDepthStencilAccess() & FExclusiveDepthStencil::DepthWrite)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				true, CF_GreaterEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
			>::GetRHI());
			DrawRenderState.SetStencilRef(StencilValue);
		}
		else
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				false, CF_GreaterEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
			>::GetRHI());
			DrawRenderState.SetStencilRef(StencilValue);
		}
	}
	else if (bMaskInEarlyPass)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
	}
}

void FBasePassDrawingPolicy::ApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
{
	FDepthStencilStateRHIParamRef DepthStencilState = nullptr;
	DrawRenderState.SetDitheredLODTransitionAlpha(0.0f);

	if (Mesh.bDitheredLODTransition)
	{
		if (ViewInfo.StaticMeshFadeOutDitheredLODMap[Mesh.Id])
		{
			if (InAllowStencilDither)
			{
				DepthStencilState = TStaticDepthStencilState<
					false, CF_Equal,
					true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
				>::GetRHI();
			}
			else
			{
				DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition());
			}
		}
		else if (ViewInfo.StaticMeshFadeInDitheredLODMap[Mesh.Id])
		{
			if (InAllowStencilDither)
			{
				DepthStencilState = TStaticDepthStencilState<
					false, CF_Equal,
					true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
				>::GetRHI();
			}
			else
			{
				DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition() - 1.0f);
			}
		}
	}

	SetDepthStencilStateForBasePass(DrawRenderState, ViewInfo, Mesh, Mesh.PrimitiveSceneInfo->Proxy, bEnableReceiveDecalOutput, UseDebugViewPS(), DepthStencilState);
}

/** The action used to draw a base pass static mesh element. */
class FDrawBasePassStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawBasePassStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	bool UseTranslucentSelfShadowing() const { return false; }
	const FProjectedShadowInfo* GetTranslucentSelfShadow() const { return NULL; }

	bool AllowIndirectLightingCache() const 
	{ 
		// Note: can't disallow based on presence of PrecomputedLightVolumes in the scene as this is registration time
		// Unless extra handling is added to recreate static draw lists when new volumes are added
		return true; 
	}

	bool AllowIndirectLightingCacheVolumeTexture() const
	{
		return true;
	}
	
	bool UseVolumetricLightmap() const
	{
		return Scene->VolumetricLightmapSceneData.HasData();
	}

	/** Draws the mesh with a specific light-map type */
	template<typename LightMapPolicyType>
	void Process(
		FRHICommandList& RHICmdList,
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
		EBasePassDrawListType DrawType = EBasePass_Default;

		if (StaticMesh->IsMasked(Parameters.FeatureLevel))
		{
			DrawType = EBasePass_Masked;
		}

		if (Scene)
		{
			// Find the appropriate draw list for the static mesh based on the light-map policy type.
			TStaticMeshDrawList<TBasePassDrawingPolicy<LightMapPolicyType> >& DrawList =
				Scene->GetBasePassDrawList<LightMapPolicyType>(DrawType);

			const bool bRenderSkylight = Scene->ShouldRenderSkylightInBasePass(Parameters.BlendMode) && Parameters.ShadingModel != MSM_Unlit;
			const bool bRenderAtmosphericFog = IsTranslucentBlendMode(Parameters.BlendMode) && Scene->HasAtmosphericFog() && Scene->ReadOnlyCVARCache.bEnableAtmosphericFog;

			// Add the static mesh to the draw list.
			DrawList.AddMesh(
				StaticMesh,
				typename TBasePassDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
				TBasePassDrawingPolicy<LightMapPolicyType>(
					StaticMesh->VertexFactory,
					StaticMesh->MaterialRenderProxy,
					*Parameters.Material,
					Parameters.FeatureLevel,
					LightMapPolicy,
					Parameters.BlendMode,
					bRenderSkylight,
					bRenderAtmosphericFog,
					ComputeMeshOverrideSettings(*StaticMesh),
					DVSM_None,
					/* bInEnableReceiveDecalOutput = */ true
				),
				Scene->GetFeatureLevel()
				);
		}
	}
};

void FBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
{
	// Determine the mesh's material and blend mode.
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if (!IsTranslucentBlendMode(BlendMode) && ShouldIncludeDomainInMeshPass(Material->GetMaterialDomain()))
	{
		ProcessBasePassMesh(
			RHICmdList, 
			FProcessBasePassMeshParameters(
				*StaticMesh,
				Material,
				StaticMesh->PrimitiveSceneInfo->Proxy,
				false,
				Scene->GetFeatureLevel()),
			FDrawBasePassStaticMeshAction(Scene,StaticMesh)
			);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawBasePassDynamicMeshAction
{
public:

	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawBasePassDynamicMeshAction(
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		float InDitheredLODTransitionAlpha,
		const FDrawingPolicyRenderState& InDrawRenderState,
		const FHitProxyId InHitProxyId
		)
		: View(InView)
		, DrawRenderState(InDrawRenderState)
		, HitProxyId(InHitProxyId)
	{
		DrawRenderState.SetDitheredLODTransitionAlpha(InDitheredLODTransitionAlpha);
	}

	bool UseTranslucentSelfShadowing() const { return false; }
	const FProjectedShadowInfo* GetTranslucentSelfShadow() const { return NULL; }

	bool AllowIndirectLightingCache() const 
	{ 
		const FScene* Scene = (const FScene*)View.Family->Scene;
		return View.Family->EngineShowFlags.IndirectLightingCache && Scene && Scene->PrecomputedLightVolumes.Num() > 0;
	}

	bool AllowIndirectLightingCacheVolumeTexture() const
	{
		return true;
	}

	bool UseVolumetricLightmap() const
	{
		const FScene* Scene = (const FScene*)View.Family->Scene;
		return View.Family->EngineShowFlags.VolumetricLightmap 
			&& Scene 
			&& Scene->VolumetricLightmapSceneData.HasData();
	}

	/** Draws the translucent mesh with a specific light-map type, and shader complexity predicate. */
	template<typename LightMapPolicyType>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//TODO this codepath is probably disabled by SetDepthStencilStateForBasePass
		if(View.Family->EngineShowFlags.ShaderComplexity)
		{
			// When rendering masked materials in the shader complexity viewmode, 
			// We want to overwrite complexity for the pixels which get depths written,
			// And accumulate complexity for pixels which get killed due to the opacity mask being below the clip value.
			// This is accomplished by forcing the masked materials to render depths in the depth only pass, 
			// Then rendering in the base pass with additive complexity blending, depth tests on, and depth writes off.
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
		}
		else if (View.Family->UseDebugViewPS() && View.Family->GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
		{
			if (Parameters.PrimitiveSceneProxy && Parameters.PrimitiveSceneProxy->IsSelected())
			{
				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
			}
			else // If not selected, use depth equal to make alpha test stand out (goes with EarlyZPassMode = DDM_AllOpaque) 
			{
				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
			}
		}
#endif
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		const bool bRenderSkylight = Scene && Scene->ShouldRenderSkylightInBasePass(Parameters.BlendMode) && Parameters.ShadingModel != MSM_Unlit;
		const bool bRenderAtmosphericFog = IsTranslucentBlendMode(Parameters.BlendMode) && (Scene && Scene->HasAtmosphericFog() && Scene->ReadOnlyCVARCache.bEnableAtmosphericFog) && View.Family->EngineShowFlags.AtmosphericFog;

		bool bEnableReceiveDecalOutput = Scene != nullptr;
		TBasePassDrawingPolicy<LightMapPolicyType> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			Parameters.FeatureLevel,
			LightMapPolicy,
			Parameters.BlendMode,
			bRenderSkylight,
			bRenderAtmosphericFog,
			ComputeMeshOverrideSettings(Parameters.Mesh),
			View.Family->GetDebugViewShaderMode(),
			bEnableReceiveDecalOutput
			);

		SetDepthStencilStateForBasePass(DrawRenderState, View, Parameters.Mesh, Parameters.PrimitiveSceneProxy, bEnableReceiveDecalOutput, DrawingPolicy.UseDebugViewPS(), nullptr);
		DrawingPolicy.SetupPipelineState(DrawRenderState, View);
		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderState, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()), DrawingPolicy.GetMaterialRenderProxy());
		DrawingPolicy.SetSharedState(RHICmdList, DrawRenderState, &View, typename TBasePassDrawingPolicy<LightMapPolicyType>::ContextDataType(Parameters.bIsInstancedStereo));

		for( int32 BatchElementIndex = 0, Num = Parameters.Mesh.Elements.Num(); BatchElementIndex < Num; BatchElementIndex++ )
		{
			// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
			const bool bIsInstancedMesh = Parameters.Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
			const uint32 InstancedStereoDrawCount = (Parameters.bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
			for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
			{
				DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

				TDrawEvent<FRHICommandList> MeshEvent;
				BeginMeshDrawEvent(RHICmdList, Parameters.PrimitiveSceneProxy, Parameters.Mesh, MeshEvent, EnumHasAnyFlags(EShowMaterialDrawEventTypes(GShowMaterialDrawEventTypes), EShowMaterialDrawEventTypes::BasePass));

				DrawingPolicy.SetMeshRenderState(
					RHICmdList,
					View,
					Parameters.PrimitiveSceneProxy,
					Parameters.Mesh,
					BatchElementIndex,
					DrawRenderState,
					typename TBasePassDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
					typename TBasePassDrawingPolicy<LightMapPolicyType>::ContextDataType()
					);
				DrawingPolicy.DrawMesh(RHICmdList, View, Parameters.Mesh, BatchElementIndex, Parameters.bIsInstancedStereo);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//TODO this codepath is probably disabled by SetDepthStencilStateForBasePass
		if(View.Family->EngineShowFlags.ShaderComplexity)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
		}
#endif
	}
};

bool FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bIsInstancedStereo
	)
{
	// Determine the mesh's material and blend mode.
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if (!IsTranslucentBlendMode(BlendMode) && ShouldIncludeDomainInMeshPass(Material->GetMaterialDomain()))
	{
		ProcessBasePassMesh(
			RHICmdList, 
			FProcessBasePassMeshParameters(
				Mesh,
				Material,
				PrimitiveSceneProxy,
				!bPreFog,
				View.GetFeatureLevel(), 
				bIsInstancedStereo
				),
			FDrawBasePassDynamicMeshAction(
				RHICmdList,
				View,
				Mesh.DitheredLODTransitionAlpha,
				DrawRenderState,
				HitProxyId
				)
			);
		return true;
	}
	else
	{
		return false;
	}
}

void FSelfShadowedCachedPointIndirectLightingPolicy::SetMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	if (PixelShaderParameters)
	{
		FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
		
		if (View.Family->EngineShowFlags.GlobalIllumination && PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveSceneInfo())
		{
			PrecomputedLightingBuffer = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheUniformBuffer;
		}
		
		if (!PrecomputedLightingBuffer)
		{
			PrecomputedLightingBuffer = GEmptyPrecomputedLightingUniformBuffer.GetUniformBufferRHI();
		}

		if (PixelShaderParameters->BufferParameter.IsBound())
		{
			SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShaderParameters->BufferParameter, PrecomputedLightingBuffer);
		}
	}

	FSelfShadowedTranslucencyPolicy::SetMesh(
		RHICmdList, 
		View, 
		PrimitiveSceneProxy, 
		VertexShaderParameters,
		PixelShaderParameters,
		VertexShader,
		PixelShader,
		VertexFactory,
		MaterialRenderProxy,
		ElementData);
}

void FSelfShadowedVolumetricLightmapPolicy::SetMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	if (PixelShaderParameters)
	{
		FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
		
		if (View.Family->EngineShowFlags.GlobalIllumination && PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveSceneInfo())
		{
			PrecomputedLightingBuffer = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheUniformBuffer;
		}
		
		if (!PrecomputedLightingBuffer)
		{
			PrecomputedLightingBuffer = GEmptyPrecomputedLightingUniformBuffer.GetUniformBufferRHI();
		}

		if (PixelShaderParameters->BufferParameter.IsBound())
		{
			SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShaderParameters->BufferParameter, PrecomputedLightingBuffer);
		}
	}

	FSelfShadowedTranslucencyPolicy::SetMesh(
		RHICmdList, 
		View, 
		PrimitiveSceneProxy, 
		VertexShaderParameters,
		PixelShaderParameters,
		VertexShader,
		PixelShader,
		VertexFactory,
		MaterialRenderProxy,
		ElementData);
}

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <ELightMapPolicyType Policy>
void GetUniformBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bNeedsHSDS,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader
	)
{
	if (bNeedsHSDS)
	{
		DomainShader = Material.GetShader<TBasePassDS<TUniformLightMapPolicy<Policy> > >(VertexFactoryType);
		
		// Metal requires matching permutations, but no other platform should worry about this complication.
		if (bEnableAtmosphericFog && DomainShader && IsMetalPlatform(EShaderPlatform(DomainShader->GetTarget().Platform)))
		{
			HullShader = Material.GetShader<TBasePassHS<TUniformLightMapPolicy<Policy>, true > >(VertexFactoryType);
		}
		else
		{
			HullShader = Material.GetShader<TBasePassHS<TUniformLightMapPolicy<Policy>, false > >(VertexFactoryType);
		}
	}

	if (bEnableAtmosphericFog)
	{
		VertexShader = Material.GetShader<TBasePassVS<TUniformLightMapPolicy<Policy>, true> >(VertexFactoryType);
	}
	else
	{
		VertexShader = Material.GetShader<TBasePassVS<TUniformLightMapPolicy<Policy>, false> >(VertexFactoryType);
	}
	if (bEnableSkyLight)
	{
		PixelShader = Material.GetShader<TBasePassPS<TUniformLightMapPolicy<Policy>, true> >(VertexFactoryType);
	}
	else
	{
		PixelShader = Material.GetShader<TBasePassPS<TUniformLightMapPolicy<Policy>, false> >(VertexFactoryType);
	}
}

template <>
void GetBasePassShaders<FUniformLightMapPolicy>(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	bool bNeedsHSDS,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
		GetUniformBasePassShaders<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
		GetUniformBasePassShaders<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_CACHED_POINT_INDIRECT_LIGHTING:
		GetUniformBasePassShaders<LMP_CACHED_POINT_INDIRECT_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_NO_LIGHTMAP:
		GetUniformBasePassShaders<LMP_SIMPLE_NO_LIGHTMAP>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_LQ_LIGHTMAP:
		GetUniformBasePassShaders<LMP_LQ_LIGHTMAP>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_HQ_LIGHTMAP:
		GetUniformBasePassShaders<LMP_HQ_LIGHTMAP>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
		GetUniformBasePassShaders<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	default:										
		check(false);
	case LMP_NO_LIGHTMAP:
		GetUniformBasePassShaders<LMP_NO_LIGHTMAP>(Material, VertexFactoryType, bNeedsHSDS, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	}
}

FTextureRHIRef& GetEyeAdaptation(const FViewInfo& View)
{
	if (View.HasValidEyeAdaptation())
	{
		IPooledRenderTarget* EyeAdaptationRT = View.GetEyeAdaptation();
		if (EyeAdaptationRT)
		{
			return EyeAdaptationRT->GetRenderTargetItem().TargetableTexture;
		}
	}

	return GWhiteTexture->TextureRHI;
}

void SetupSharedBasePassParameters(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	FSceneRenderTargets& SceneRenderTargets,
	FSharedBasePassUniformParameters& SharedParameters)
{
	SharedParameters.Forward = View.ForwardLightingResources->ForwardLightData;

	if (View.bIsInstancedStereoEnabled && View.StereoPass == EStereoscopicPass::eSSP_LEFT_EYE)
	{
		const FSceneView& RightEye = *View.Family->Views[1];
		SharedParameters.ForwardISR = RightEye.ForwardLightingResources->ForwardLightData;
	}
	else
	{
		SharedParameters.ForwardISR = View.ForwardLightingResources->ForwardLightData;
	}

	SetupReflectionUniformParameters(View, SharedParameters.Reflection);
	SetupFogUniformParameters(View, SharedParameters.Fog);

	const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT(RHICmdList);

	if (!PooledRT)
	{
		// no subsurface profile was used yet
		PooledRT = GSystemTextures.BlackDummy;
	}

	const FSceneRenderTargetItem& Item = PooledRT->GetRenderTargetItem();
	SharedParameters.SSProfilesTexture = Item.ShaderResourceTexture;
}

void CreateOpaqueBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	IPooledRenderTarget* ForwardScreenSpaceShadowMask, 
	TUniformBufferRef<FOpaqueBasePassUniformParameters>& BasePassUniformBuffer)
{
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

	FOpaqueBasePassUniformParameters BasePassParameters;
	SetupSharedBasePassParameters(RHICmdList, View, SceneRenderTargets, BasePassParameters.Shared);

	// Forward shading
	{
		if (!ForwardScreenSpaceShadowMask)
		{
			ForwardScreenSpaceShadowMask = GSystemTextures.WhiteDummy.GetReference();
		}
		BasePassParameters.ForwardScreenSpaceShadowMaskTexture = ForwardScreenSpaceShadowMask->GetRenderTargetItem().ShaderResourceTexture;

		IPooledRenderTarget* IndirectOcclusion = SceneRenderTargets.ScreenSpaceAO;

		if (!SceneRenderTargets.bScreenSpaceAOIsValid)
		{
			IndirectOcclusion = GSystemTextures.WhiteDummy;
		}

		BasePassParameters.IndirectOcclusionTexture = IndirectOcclusion->GetRenderTargetItem().ShaderResourceTexture;

		FTextureRHIParamRef ResolvedSceneDepthTextureValue = GSystemTextures.WhiteDummy->GetRenderTargetItem().ShaderResourceTexture;

		if (SceneRenderTargets.GetMSAACount() > 1)
		{
			ResolvedSceneDepthTextureValue = SceneRenderTargets.SceneDepthZ->GetRenderTargetItem().ShaderResourceTexture;
		}

		BasePassParameters.ResolvedSceneDepthTexture = ResolvedSceneDepthTextureValue;
	}

	// DBuffer Decals
	{
		const bool bIsDBufferEnabled = IsUsingDBuffers(View.GetShaderPlatform());
		IPooledRenderTarget* DBufferA = bIsDBufferEnabled && SceneRenderTargets.DBufferA ? SceneRenderTargets.DBufferA : GSystemTextures.BlackAlphaOneDummy;
		IPooledRenderTarget* DBufferB = bIsDBufferEnabled && SceneRenderTargets.DBufferB ? SceneRenderTargets.DBufferB : GSystemTextures.DefaultNormal8Bit;
		IPooledRenderTarget* DBufferC = bIsDBufferEnabled && SceneRenderTargets.DBufferC ? SceneRenderTargets.DBufferC : GSystemTextures.BlackAlphaOneDummy;

		BasePassParameters.DBufferATexture = DBufferA->GetRenderTargetItem().ShaderResourceTexture;
		BasePassParameters.DBufferBTexture = DBufferB->GetRenderTargetItem().ShaderResourceTexture;
		BasePassParameters.DBufferCTexture = DBufferC->GetRenderTargetItem().ShaderResourceTexture;
		BasePassParameters.DBufferATextureSampler = TStaticSamplerState<>::GetRHI();
		BasePassParameters.DBufferBTextureSampler = TStaticSamplerState<>::GetRHI();
		BasePassParameters.DBufferCTextureSampler = TStaticSamplerState<>::GetRHI();

		if ((GSupportsRenderTargetWriteMask || IsUsingPerPixelDBufferMask(View.GetShaderPlatform())) && SceneRenderTargets.DBufferMask)
		{
			BasePassParameters.DBufferRenderMask = SceneRenderTargets.DBufferMask->GetRenderTargetItem().TargetableTexture;
		}
		else
		{
			BasePassParameters.DBufferRenderMask = GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture;
		}
	}

	// Misc
	BasePassParameters.EyeAdaptation = GetEyeAdaptation(View);

	BasePassUniformBuffer = TUniformBufferRef<FOpaqueBasePassUniformParameters>::CreateUniformBufferImmediate(BasePassParameters, UniformBuffer_SingleFrame);
}

/**
 * Renders the scene's base pass 
 * @return true if anything was rendered
 */
bool FDeferredShadingSceneRenderer::RenderBasePass(FRHICommandListImmediate& RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, IPooledRenderTarget* ForwardScreenSpaceShadowMask)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderBasePass, FColor::Emerald);

	bool bDirty = false;

	RHICmdList.AutomaticCacheFlushAfterComputeShader(false);

	if (ViewFamily.EngineShowFlags.LightMapDensity && AllowDebugViewmodes())
	{
		// Override the base pass with the lightmap density pass if the viewmode is enabled.
		bDirty = RenderLightMapDensities(RHICmdList);
	}
	else
	{
		SCOPED_DRAW_EVENT(RHICmdList, BasePass);
		SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);
		SCOPED_GPU_STAT(RHICmdList, Basepass);

		if (GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePass.GetValueOnRenderThread())
		{
			FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

				TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;
				CreateOpaqueBasePassUniformBuffer(RHICmdList, View, ForwardScreenSpaceShadowMask, BasePassUniformBuffer);

				FDrawingPolicyRenderState DrawRenderState(View, BasePassUniformBuffer);

				if (View.ShouldRenderView())
				{
					RenderBasePassViewParallel(View, RHICmdList, BasePassDepthStencilAccess, DrawRenderState);
				}

				RenderEditorPrimitives(RHICmdList, View, BasePassDepthStencilAccess, DrawRenderState, bDirty);
			}

			bDirty = true; // assume dirty since we are not going to wait
		}
		else
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

				TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;
				CreateOpaqueBasePassUniformBuffer(RHICmdList, View, ForwardScreenSpaceShadowMask, BasePassUniformBuffer);

				FDrawingPolicyRenderState DrawRenderState(View, BasePassUniformBuffer);

				if (View.ShouldRenderView())
				{
					bDirty |= RenderBasePassView(RHICmdList, View, BasePassDepthStencilAccess, DrawRenderState);
				}

				RenderEditorPrimitives(RHICmdList, View, BasePassDepthStencilAccess, DrawRenderState, bDirty);
			}
		}	
	}

	RHICmdList.AutomaticCacheFlushAfterComputeShader(true);
	RHICmdList.FlushComputeShaderCache();

	return bDirty;
}

bool FDeferredShadingSceneRenderer::RenderBasePassStaticDataType(FRHICommandList& RHICmdList, FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const EBasePassDrawListType DrawType)
{
	SCOPED_DRAW_EVENTF(RHICmdList, StaticType, TEXT("Static EBasePassDrawListType=%d"), DrawType);

	return Scene->BasePassUniformLightMapPolicyDrawList[DrawType].DrawVisible(RHICmdList, View, DrawRenderState, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataTypeParallel(FParallelCommandListSet& ParallelCommandListSet, const EBasePassDrawListType DrawType)
{
	Scene->BasePassUniformLightMapPolicyDrawList[DrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
}

template<typename StaticMeshDrawList>
class FSortFrontToBackTask
{
private:
	StaticMeshDrawList * const StaticMeshDrawListToSort;
	const FVector ViewPosition;

public:
	FSortFrontToBackTask(StaticMeshDrawList * const InStaticMeshDrawListToSort, const FVector InViewPosition)
		: StaticMeshDrawListToSort(InStaticMeshDrawListToSort)
		, ViewPosition(InViewPosition)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSortFrontToBackTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FSortFrontToBackTask.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		StaticMeshDrawListToSort->SortFrontToBack(ViewPosition);
	}
};

void FDeferredShadingSceneRenderer::AsyncSortBasePassStaticData(const FVector InViewPosition, FGraphEventArray &OutSortEvents)
{
	// If we're not using a depth only pass, sort the static draw list buckets roughly front to back, to maximize HiZ culling
	// Note that this is only a very rough sort, since it does not interfere with state sorting, and each list is sorted separately
	if (EarlyZPassMode != DDM_None)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AsyncSortBasePassStaticData);

	for (int32 DrawType = 0; DrawType < EBasePass_MAX; ++DrawType)
	{
		OutSortEvents.Add(TGraphTask<FSortFrontToBackTask<TStaticMeshDrawList<TBasePassDrawingPolicy<FUniformLightMapPolicy> > > >::CreateTask(
			nullptr, ENamedThreads::GetRenderThread()).ConstructAndDispatchWhenReady(&(Scene->BasePassUniformLightMapPolicyDrawList[DrawType]), InViewPosition));
	}
}

void FDeferredShadingSceneRenderer::SortBasePassStaticData(FVector ViewPosition)
{
	// If we're not using a depth only pass, sort the static draw list buckets roughly front to back, to maximize HiZ culling
	// Note that this is only a very rough sort, since it does not interfere with state sorting, and each list is sorted separately
	if (EarlyZPassMode == DDM_None)
	{
		SCOPE_CYCLE_COUNTER(STAT_SortStaticDrawLists);

		for (int32 DrawType = 0; DrawType < EBasePass_MAX; DrawType++)
		{
			Scene->BasePassUniformLightMapPolicyDrawList[DrawType].SortFrontToBack(ViewPosition);
		}
	}
}

/**
* Renders the basepass for the static data of a given View.
*
* @return true if anything was rendered to scene color
*/
bool FDeferredShadingSceneRenderer::RenderBasePassStaticData(FRHICommandList& RHICmdList, FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState)
{
	bool bDirty = false;

	SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);

	// When using a depth-only pass, the default opaque geometry's depths are already
	// in the depth buffer at this point, so rendering masked next will already cull
	// as efficiently as it can, while also increasing the ZCull efficiency when
	// rendering the default opaque geometry afterward.
	if (EarlyZPassMode != DDM_None)
	{
		bDirty |= RenderBasePassStaticDataType(RHICmdList, View, DrawRenderState, EBasePass_Masked);
		bDirty |= RenderBasePassStaticDataType(RHICmdList, View, DrawRenderState, EBasePass_Default);
	}
	else
	{
		// Otherwise, in the case where we're not using a depth-only pre-pass, there
		// is an advantage to rendering default opaque first to help cull the more
		// expensive masked geometry.
		bDirty |= RenderBasePassStaticDataType(RHICmdList, View, DrawRenderState, EBasePass_Default);
		bDirty |= RenderBasePassStaticDataType(RHICmdList, View, DrawRenderState, EBasePass_Masked);
	}
	return bDirty;
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);

	// When using a depth-only pass, the default opaque geometry's depths are already
	// in the depth buffer at this point, so rendering masked next will already cull
	// as efficiently as it can, while also increasing the ZCull efficiency when
	// rendering the default opaque geometry afterward.
	if (EarlyZPassMode != DDM_None)
	{
		RenderBasePassStaticDataTypeParallel(ParallelCommandListSet, EBasePass_Masked);
		RenderBasePassStaticDataTypeParallel(ParallelCommandListSet, EBasePass_Default);
	}
	else
	{
		// Otherwise, in the case where we're not using a depth-only pre-pass, there
		// is an advantage to rendering default opaque first to help cull the more
		// expensive masked geometry.
		RenderBasePassStaticDataTypeParallel(ParallelCommandListSet, EBasePass_Default);
		RenderBasePassStaticDataTypeParallel(ParallelCommandListSet, EBasePass_Masked);
	}
}

/**
* Renders the basepass for the dynamic data of a given DPG and View.
*
* @return true if anything was rendered to scene color
*/

void FDeferredShadingSceneRenderer::RenderBasePassDynamicData(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, bool& bOutDirty)
{
	bool bDirty = false;

	SCOPE_CYCLE_COUNTER(STAT_DynamicPrimitiveDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

	FBasePassOpaqueDrawingPolicyFactory::ContextType Context;

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if ((MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() || ViewFamily.EngineShowFlags.Wireframe)
			&& MeshBatchAndRelevance.GetRenderInMainPass())
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
		}
	}

	if (bDirty)
	{
		bOutDirty = true;
	}
}

class FRenderBasePassDynamicDataThreadTask : public FRenderTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;

public:

	FRenderBasePassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderBasePassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		bool OutDirty = false;
		ThisRenderer.RenderBasePassDynamicData(RHICmdList, View, DrawRenderState, OutDirty);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

void FDeferredShadingSceneRenderer::RenderBasePassDynamicDataParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();
	FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderBasePassDynamicDataThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::GetRenderThread())
		.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, ParallelCommandListSet.DrawRenderState);

	ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
}

static void SetupBasePassView(FRHICommandList& RHICmdList, const FViewInfo& View, const FSceneRenderer* SceneRenderer, FDrawingPolicyRenderState& DrawRenderState, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const bool bShaderComplexity, const bool bIsEditorPrimitivePass = false)
{
	DrawRenderState.SetDepthStencilAccess(BasePassDepthStencilAccess);

	if (bShaderComplexity)
	{
		// Additive blending when shader complexity viewmode is enabled.
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
		// Disable depth writes as we have a full depth prepass.
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	}
	else
	{
		// Opaque blending for all G buffer targets, depth tests and writes.
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BasePassOutputsVelocityDebug"));
		if (CVar && CVar->GetValueOnRenderThread() == 2)
		{
			DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_NONE>::GetRHI());
		}
		else
		{
			DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
		}

		if (DrawRenderState.GetDepthStencilAccess() & FExclusiveDepthStencil::DepthWrite)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		}
		else
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}
	}

	if (!View.IsInstancedStereoPass() || bIsEditorPrimitivePass)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	}
	else
	{
		if (View.bIsMultiViewEnabled)
		{
			const uint32 LeftMinX = SceneRenderer->Views[0].ViewRect.Min.X;
			const uint32 LeftMaxX = SceneRenderer->Views[0].ViewRect.Max.X;
			const uint32 RightMinX = SceneRenderer->Views[1].ViewRect.Min.X;
			const uint32 RightMaxX = SceneRenderer->Views[1].ViewRect.Max.X;
			
			const uint32 LeftMaxY = SceneRenderer->Views[0].ViewRect.Max.Y;
			const uint32 RightMaxY = SceneRenderer->Views[1].ViewRect.Max.Y;
			
			RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0, 0.0f, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, 1.0f);
		}
		else
		{
			RHICmdList.SetViewport(0, 0, 0, SceneRenderer->InstancedStereoWidth, View.ViewRect.Max.Y, 1);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Basepass"), STAT_CLP_Basepass, STATGROUP_ParallelCommandListMarkers);

class FBasePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess;

	FBasePassParallelCommandListSet(
		const FViewInfo& InView,
		FRHICommandListImmediate& InParentCmdList,
		bool bInParallelExecute,
		bool bInCreateSceneContext,
		const FSceneRenderer* InSceneRenderer,
		FExclusiveDepthStencil::Type InBasePassDepthStencilAccess,
		const FDrawingPolicyRenderState& InDrawRenderState)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Basepass), InView, InSceneRenderer, InParentCmdList, bInParallelExecute, bInCreateSceneContext, InDrawRenderState)
		, BasePassDepthStencilAccess(InBasePassDepthStencilAccess)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FBasePassParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		FSceneRenderTargets::Get(CmdList).BeginRenderingGBuffer(CmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, BasePassDepthStencilAccess, SceneRenderer->ViewFamily.EngineShowFlags.ShaderComplexity);
		SetupBasePassView(CmdList, View, SceneRenderer, DrawRenderState, BasePassDepthStencilAccess, !!SceneRenderer->ViewFamily.EngineShowFlags.ShaderComplexity);
	}
};

void FDeferredShadingSceneRenderer::RenderBasePassViewParallel(FViewInfo& View, FRHICommandListImmediate& ParentCmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FDrawingPolicyRenderState& InDrawRenderState)
{
	FBasePassParallelCommandListSet ParallelSet(View, ParentCmdList, 
		CVarRHICmdBasePassDeferredContexts.GetValueOnRenderThread() > 0, 
		CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
		this,
		BasePassDepthStencilAccess,
		InDrawRenderState);

	RenderBasePassStaticDataParallel(ParallelSet);
	RenderBasePassDynamicDataParallel(ParallelSet);
}

void FDeferredShadingSceneRenderer::RenderEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FDrawingPolicyRenderState& InDrawRenderState, bool& bOutDirty) 
{
	FDrawingPolicyRenderState DrawRenderState(InDrawRenderState);
	SetupBasePassView(RHICmdList, View, this, DrawRenderState, BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity, true);

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked);

	bool bDirty = false;
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(ShaderPlatform);

		// Draw the base pass for the view's batched mesh elements.
		bDirty |= DrawViewElements<FBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, FBasePassOpaqueDrawingPolicyFactory::ContextType(), SDPG_World, true) || bDirty;

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false) || bDirty;

		// Draw foreground objects last
		bDirty |= DrawViewElements<FBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, FBasePassOpaqueDrawingPolicyFactory::ContextType(), SDPG_Foreground, true) || bDirty;

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false) || bDirty;

	}

	if (bDirty)
	{
		bOutDirty = true;
	}
}

bool FDeferredShadingSceneRenderer::RenderBasePassView(FRHICommandListImmediate& RHICmdList, FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FDrawingPolicyRenderState& InDrawRenderState)
{
	bool bDirty = false; 
	FDrawingPolicyRenderState DrawRenderState(InDrawRenderState);
	SetupBasePassView(RHICmdList, View, this, DrawRenderState, BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity);
	bDirty |= RenderBasePassStaticData(RHICmdList, View, DrawRenderState);
	RenderBasePassDynamicData(RHICmdList, View, DrawRenderState, bDirty);

	return bDirty;
}
