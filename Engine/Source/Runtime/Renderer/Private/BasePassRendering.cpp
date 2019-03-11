// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "BasePassRendering.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"
#include "EditorPrimitivesRendering.h"
#include "TranslucentRendering.h"

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

static TAutoConsoleVariable<int32> CVarRHICmdBasePassDeferredContexts(
	TEXT("r.RHICmdBasePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize base pass command list execution."));

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

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSharedBasePassUniformParameters, "BasePass");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FOpaqueBasePassUniformParameters, "OpaqueBasePass");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucentBasePassUniformParameters, "TranslucentBasePass");

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

void SetBasePassDitheredLODTransitionState(const FSceneView* SceneView, const FMeshBatch& RESTRICT Mesh, int32 StaticMeshId, FMeshPassProcessorRenderState& DrawRenderState)
{
	if (SceneView && StaticMeshId >= 0 && Mesh.bDitheredLODTransition)
	{
		checkSlow(SceneView->bIsViewInfo);
		const FViewInfo* ViewInfo = (FViewInfo*)SceneView;

		if (ViewInfo->bAllowStencilDither)
		{
			if (ViewInfo->StaticMeshFadeOutDitheredLODMap[StaticMeshId])
			{
				DrawRenderState.SetDepthStencilState(
					TStaticDepthStencilState<
					false, CF_Equal,
					true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
					>::GetRHI());
			}
			else if (ViewInfo->StaticMeshFadeInDitheredLODMap[StaticMeshId])
			{
				DrawRenderState.SetDepthStencilState(
					TStaticDepthStencilState<
					false, CF_Equal,
					true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)
					>::GetRHI());
			}
		}
	}
}

void SetTranslucentRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FMaterial& Material)
{
	switch (Material.GetBlendMode())
	{
	default:
	case BLEND_Opaque:
		// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
		break;
	case BLEND_Masked:
		// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
		break;
	case BLEND_Translucent:
		// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
		// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
		break;
	case BLEND_Additive:
		// Add to the existing scene color
		// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
		// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
		break;
	case BLEND_Modulate:
		// Modulate with the existing scene color, preserve destination alpha.
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
		break;
	case BLEND_AlphaComposite:
		// Blend with existing scene color. New color is already pre-multiplied by alpha.
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
		break;
	};

	const bool bDisableDepthTest = Material.ShouldDisableDepthTest();
	const bool bEnableResponsiveAA = Material.ShouldEnableResponsiveAA();

	if (bEnableResponsiveAA)
	{
		if (bDisableDepthTest)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				STENCIL_TEMPORAL_RESPONSIVE_AA_MASK, STENCIL_TEMPORAL_RESPONSIVE_AA_MASK
			>::GetRHI());
			DrawRenderState.SetStencilRef(STENCIL_TEMPORAL_RESPONSIVE_AA_MASK);
		}
		else
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				false, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				STENCIL_TEMPORAL_RESPONSIVE_AA_MASK, STENCIL_TEMPORAL_RESPONSIVE_AA_MASK
			>::GetRHI());
			DrawRenderState.SetStencilRef(STENCIL_TEMPORAL_RESPONSIVE_AA_MASK);
		}
	}
	else if (bDisableDepthTest)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	}
}

FMeshDrawCommandSortKey CalculateTranslucentMeshStaticSortKey(const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, uint16 MeshIdInPrimitive)
{
	uint16 SortKeyPriority = 0;

	if (PrimitiveSceneProxy)
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
		SortKeyPriority = (uint16)((int32)PrimitiveSceneInfo->Proxy->GetTranslucencySortPriority() - (int32)SHRT_MIN);
	}

	FMeshDrawCommandSortKey SortKey;
	SortKey.Translucent.MeshIdInPrimitive = MeshIdInPrimitive;
	SortKey.Translucent.Priority = SortKeyPriority;
	SortKey.Translucent.Distance = 0; // View specific, so will be filled later inside VisibleMeshCommands.

	return SortKey;
}

FMeshDrawCommandSortKey CalculateBasePassMeshStaticSortKey(EDepthDrawingMode EarlyZPassMode, EBlendMode BlendMode, const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.BasePass.VertexShaderHash = PointerHash(VertexShader) & 0xFFFF;
	SortKey.BasePass.PixelShaderHash = PointerHash(PixelShader);
	if (EarlyZPassMode != DDM_None)
	{
		SortKey.BasePass.Masked = BlendMode == EBlendMode::BLEND_Masked ? 0 : 1;
	}
	else
	{
		SortKey.BasePass.Masked = BlendMode == EBlendMode::BLEND_Masked ? 1 : 0;
	}

	return SortKey;
}

void SetDepthStencilStateForBasePass(FMeshPassProcessorRenderState& DrawRenderState, ERHIFeatureLevel::Type FeatureLevel, const FMeshBatch& Mesh, const FPrimitiveSceneProxy* PrimitiveSceneProxy, bool bEnableReceiveDecalOutput, bool bUseDebugViewPS, FDepthStencilStateRHIParamRef LodFadeOverrideDepthStencilState)
{
	static IConsoleVariable* EarlyZPassOnlyMaterialMaskingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPassOnlyMaterialMasking"));
	bool bMaskInEarlyPass = (EarlyZPassOnlyMaterialMaskingCVar && Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->IsMasked() && EarlyZPassOnlyMaterialMaskingCVar->GetInt());

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

void SetupBasePassState(FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const bool bShaderComplexity, FMeshPassProcessorRenderState& DrawRenderState)
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
}

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <ELightMapPolicyType Policy>
void GetUniformBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	ERHIFeatureLevel::Type FeatureLevel,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>*& PixelShader
	)
{
	const EMaterialTessellationMode MaterialTessellationMode = Material.GetTessellationMode();

	const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[FeatureLevel])
		&& VertexFactoryType->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation;

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
		VertexShader = (TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader<TBasePassVS<TUniformLightMapPolicy<Policy>, true> >(VertexFactoryType);
	}
	else
	{
		VertexShader = (TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader<TBasePassVS<TUniformLightMapPolicy<Policy>, false> >(VertexFactoryType);
	}
	if (bEnableSkyLight)
	{
		PixelShader = (TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader<TBasePassPS<TUniformLightMapPolicy<Policy>, true> >(VertexFactoryType);
	}
	else
	{
		PixelShader = (TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader<TBasePassPS<TUniformLightMapPolicy<Policy>, false> >(VertexFactoryType);
	}
}

template <>
void GetBasePassShaders<FUniformLightMapPolicy>(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	ERHIFeatureLevel::Type FeatureLevel,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>*& PixelShader
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
		GetUniformBasePassShaders<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
		GetUniformBasePassShaders<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_CACHED_POINT_INDIRECT_LIGHTING:
		GetUniformBasePassShaders<LMP_CACHED_POINT_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_NO_LIGHTMAP:
		GetUniformBasePassShaders<LMP_SIMPLE_NO_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING:
		GetUniformBasePassShaders<LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_LQ_LIGHTMAP:
		GetUniformBasePassShaders<LMP_LQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_HQ_LIGHTMAP:
		GetUniformBasePassShaders<LMP_HQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
		GetUniformBasePassShaders<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
		break;
	default:										
		check(false);
	case LMP_NO_LIGHTMAP:
		GetUniformBasePassShaders<LMP_NO_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableAtmosphericFog, bEnableSkyLight, HullShader, DomainShader, VertexShader, PixelShader);
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

	const FScene* Scene = View.Family->Scene ? View.Family->Scene->GetRenderScene() : nullptr;
	const FPlanarReflectionSceneProxy* ReflectionSceneProxy = Scene ? Scene->GetForwardPassGlobalPlanarReflection() : nullptr;

	SetupReflectionUniformParameters(View, SharedParameters.Reflection);
	SetupFogUniformParameters(View, SharedParameters.Fog);
	SetupPlanarReflectionUniformParameters(View, ReflectionSceneProxy, SharedParameters.PlanarReflection);

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
		if (ForwardScreenSpaceShadowMask)
		{
			BasePassParameters.UseForwardScreenSpaceShadowMask = 1;
			BasePassParameters.ForwardScreenSpaceShadowMaskTexture = ForwardScreenSpaceShadowMask->GetRenderTargetItem().ShaderResourceTexture;
		}
		else
		{
			BasePassParameters.UseForwardScreenSpaceShadowMask = 0;
			BasePassParameters.ForwardScreenSpaceShadowMaskTexture = GSystemTextures.WhiteDummy.GetReference()->GetRenderTargetItem().ShaderResourceTexture;
		}

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

	FScene* Scene = View.Family->Scene ? View.Family->Scene->GetRenderScene() : nullptr;

	if (Scene)
	{
		Scene->UniformBuffers.OpaqueBasePassUniformBuffer.UpdateUniformBufferImmediate(BasePassParameters);
		BasePassUniformBuffer = Scene->UniformBuffers.OpaqueBasePassUniformBuffer;
	}
	else
	{
		BasePassUniformBuffer = TUniformBufferRef<FOpaqueBasePassUniformParameters>::CreateUniformBufferImmediate(BasePassParameters, UniformBuffer_SingleFrame);
	}
}

/**
 * Renders the scene's base pass. This assumes there is a current renderpass active. 
 * @return true if anything was rendered
 */
bool FDeferredShadingSceneRenderer::RenderBasePass(FRHICommandListImmediate& RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, IPooledRenderTarget* ForwardScreenSpaceShadowMask, bool bParallelBasePass, bool bRenderLightmapDensity)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderBasePass, FColor::Emerald);

	bool bDirty = false;

	RHICmdList.AutomaticCacheFlushAfterComputeShader(false);

	if (bRenderLightmapDensity)
	{
		// Override the base pass with the lightmap density pass if the viewmode is enabled.
		bDirty = RenderLightMapDensities(RHICmdList);
	}
	else if (ViewFamily.UseDebugViewPS())
	{
		// Override the base pass with one of the debug view shader mode (see EDebugViewShaderMode) if required.
		bDirty = RenderDebugViewMode(RHICmdList);
	}
	else
	{
		SCOPED_DRAW_EVENT(RHICmdList, BasePass);
		SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);
		SCOPED_GPU_STAT(RHICmdList, Basepass);

		if (bParallelBasePass)
		{
			check(RHICmdList.IsOutsideRenderPass());

			FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

				TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;
				CreateOpaqueBasePassUniformBuffer(RHICmdList, View, ForwardScreenSpaceShadowMask, BasePassUniformBuffer);

				FMeshPassProcessorRenderState DrawRenderState(View, BasePassUniformBuffer);

				SetupBasePassState(BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity, DrawRenderState);

				if (View.ShouldRenderView())
				{
					Scene->UniformBuffers.UpdateViewUniformBuffer(View);

					RenderBasePassViewParallel(View, RHICmdList, BasePassDepthStencilAccess, DrawRenderState);
				}
				
				check(RHICmdList.IsOutsideRenderPass());

				FSceneRenderTargets::Get(RHICmdList).BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, BasePassDepthStencilAccess, this->ViewFamily.EngineShowFlags.ShaderComplexity);
				RenderEditorPrimitives(RHICmdList, View, BasePassDepthStencilAccess, DrawRenderState, bDirty);
				RHICmdList.EndRenderPass();
			}

			bDirty = true; // assume dirty since we are not going to wait
		}
		else
		{
			// Must have an open renderpass before getting here in single threaded mode.
			check(RHICmdList.IsInsideRenderPass());

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

				TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;
				CreateOpaqueBasePassUniformBuffer(RHICmdList, View, ForwardScreenSpaceShadowMask, BasePassUniformBuffer);

				FMeshPassProcessorRenderState DrawRenderState(View, BasePassUniformBuffer);

				SetupBasePassState(BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity, DrawRenderState);

				if (View.ShouldRenderView())
				{
					Scene->UniformBuffers.UpdateViewUniformBuffer(View);

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

static void SetupBasePassView(FRHICommandList& RHICmdList, const FViewInfo& View, const FSceneRenderer* SceneRenderer, const bool bIsEditorPrimitivePass = false)
{
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
		const FMeshPassProcessorRenderState& InDrawRenderState)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Basepass), InView, InSceneRenderer, InParentCmdList, bInParallelExecute, bInCreateSceneContext, InDrawRenderState)
		, BasePassDepthStencilAccess(InBasePassDepthStencilAccess)
	{
	}

	virtual ~FBasePassParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		FSceneRenderTargets::Get(CmdList).BeginRenderingGBuffer(CmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, BasePassDepthStencilAccess, SceneRenderer->ViewFamily.EngineShowFlags.ShaderComplexity, false, FLinearColor(0, 0, 0, 1), SceneRenderer->ViewFamily.EngineShowFlags.Wireframe);
		SetupBasePassView(CmdList, View, SceneRenderer);
	}
};

void FDeferredShadingSceneRenderer::RenderBasePassViewParallel(FViewInfo& View, FRHICommandListImmediate& ParentCmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FMeshPassProcessorRenderState& InDrawRenderState)
{
	check(ParentCmdList.IsOutsideRenderPass());

	FBasePassParallelCommandListSet ParallelSet(View, ParentCmdList, 
		CVarRHICmdBasePassDeferredContexts.GetValueOnRenderThread() > 0, 
		CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
		this,
		BasePassDepthStencilAccess,
		InDrawRenderState);

	// enqueue RHIThread command that blocks on prereq, lock / unlock vertex buffer upload

	View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].DispatchDraw(&ParallelSet, ParentCmdList);
}

bool HasEditorPrimitivesForDPG(const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	bool bHasPrimitives = View.SimpleElementCollector.HasPrimitives(DepthPriorityGroup);

	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const TIndirectArray<FMeshBatch>& ViewMeshElementList = (DepthPriorityGroup == SDPG_Foreground ? View.TopViewMeshElements : View.ViewMeshElements);
		bHasPrimitives |= ViewMeshElementList.Num() > 0;

		const FBatchedElements& BatchedViewElements = DepthPriorityGroup == SDPG_World ? View.BatchedViewElements : View.TopBatchedViewElements;
		bHasPrimitives |= BatchedViewElements.HasPrimsToDraw();
	}

	return bHasPrimitives;
}

void FDeferredShadingSceneRenderer::RenderEditorPrimitives(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FMeshPassProcessorRenderState& InDrawRenderState, bool& bOutDirty) 
{
	FMeshPassProcessorRenderState DrawRenderState(InDrawRenderState);
	SetupBasePassState(BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity, DrawRenderState);
	SetupBasePassView(RHICmdList, View, this, true);

	RenderEditorPrimitivesForDPG(RHICmdList, View, BasePassDepthStencilAccess, DrawRenderState, SDPG_World, bOutDirty);

	if (HasEditorPrimitivesForDPG(View, SDPG_Foreground))
	{
		RHICmdList.EndRenderPass();

		// Write foreground primitives into depth buffer without testing 
		{
			// Change to depth writable
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			SceneContext.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite, false);

			// need to setup view again after reconfiguring render targets
			SetupBasePassView(RHICmdList, View, this, true);

			FMeshPassProcessorRenderState NoDepthTestDrawRenderState(DrawRenderState);
			NoDepthTestDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
			NoDepthTestDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
			RenderEditorPrimitivesForDPG(RHICmdList, View, BasePassDepthStencilAccess, NoDepthTestDrawRenderState, SDPG_Foreground, bOutDirty);

			RHICmdList.EndRenderPass();

			// Restore default base pass depth access
			SceneContext.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, BasePassDepthStencilAccess, false);
			SetupBasePassView(RHICmdList, View, this, true);
		}

		// Render foreground primitives with depth testing
		RenderEditorPrimitivesForDPG(RHICmdList, View, BasePassDepthStencilAccess, DrawRenderState, SDPG_Foreground, bOutDirty);
	}
}

void FDeferredShadingSceneRenderer::RenderEditorPrimitivesForDPG(FRHICommandList& RHICmdList, const FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FMeshPassProcessorRenderState& DrawRenderState, ESceneDepthPriorityGroup DepthPriorityGroup, bool& bOutDirty)
{
	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, DepthPriorityGroup);

	bool bDirty = false;
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(ShaderPlatform);

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
					View.Family->Scene->GetRenderScene(),
					View.GetFeatureLevel(),
					&View,
					DrawRenderState,
					false,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;
					
				for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

		const FBatchedElements& BatchedViewElements = DepthPriorityGroup == SDPG_World ? View.BatchedViewElements : View.TopBatchedViewElements;

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
					View.Family->Scene->GetRenderScene(),
					View.GetFeatureLevel(),
					&View,
					DrawRenderState,
					false,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;
					
				for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false) || bDirty;
	}

	if (bDirty)
	{
		bOutDirty = true;
	}
}

bool FDeferredShadingSceneRenderer::RenderBasePassView(FRHICommandListImmediate& RHICmdList, FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const FMeshPassProcessorRenderState& InDrawRenderState)
{
	bool bDirty = false; 
	FMeshPassProcessorRenderState DrawRenderState(InDrawRenderState);
	SetupBasePassView(RHICmdList, View, this);

	View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].DispatchDraw(nullptr, RHICmdList);

	return bDirty;
}

template<typename LightMapPolicyType>
void FBasePassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	EBlendMode BlendMode,
	EMaterialShadingModel ShadingModel,
	const LightMapPolicyType& RESTRICT LightMapPolicy,
	const typename LightMapPolicyType::ElementDataType& RESTRICT LightMapElementData,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	const bool bRenderSkylight = Scene && Scene->ShouldRenderSkylightInBasePass(BlendMode) && ShadingModel != MSM_Unlit;
	const bool bRenderAtmosphericFog = IsTranslucentBlendMode(BlendMode) && (Scene && Scene->HasAtmosphericFog() && Scene->ReadOnlyCVARCache.bEnableAtmosphericFog);

	TMeshProcessorShaders<
		TBasePassVertexShaderPolicyParamType<LightMapPolicyType>,
		FBaseHS,
		FBaseDS,
		TBasePassPixelShaderPolicyParamType<LightMapPolicyType>> BasePassShaders;

	GetBasePassShaders<LightMapPolicyType>(
		MaterialResource,
		VertexFactory->GetType(),
		LightMapPolicy,
		FeatureLevel,
		bRenderAtmosphericFog,
		bRenderSkylight,
		BasePassShaders.HullShader,
		BasePassShaders.DomainShader,
		BasePassShaders.VertexShader,
		BasePassShaders.PixelShader
		);


	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	const bool bEnableReceiveDecalOutput = Scene != nullptr;
	SetDepthStencilStateForBasePass(DrawRenderState, FeatureLevel, MeshBatch, PrimitiveSceneProxy, bEnableReceiveDecalOutput, false, nullptr);

	if (bTranslucentBasePass)
	{
		SetTranslucentRenderState(DrawRenderState, MaterialResource);
	}

	SetBasePassDitheredLODTransitionState(ViewIfDynamicMeshCommand, MeshBatch, StaticMeshId, DrawRenderState);

	TBasePassShaderElementData<LightMapPolicyType> ShaderElementData(LightMapElementData);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	FMeshDrawCommandSortKey SortKey = FMeshDrawCommandSortKey::Default;

	if (bTranslucentBasePass)
	{
		SortKey = CalculateTranslucentMeshStaticSortKey(PrimitiveSceneProxy, MeshBatch.MeshIdInPrimitive);
	}
	else
	{
		SortKey = CalculateBasePassMeshStaticSortKey(EarlyZPassMode, BlendMode, BasePassShaders.VertexShader, BasePassShaders.PixelShader);
	}

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		BasePassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

void FBasePassMeshProcessor::AddMeshBatchForSimpleForwardShading(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FLightMapInteraction& LightMapInteraction,
	bool bIsLitMaterial,
	bool bAllowStaticLighting,
	bool bUseVolumetricLightmap,
	bool bAllowIndirectLightingCache,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	const EMaterialShadingModel ShadingModel = Material.GetShadingModel();

	if (bAllowStaticLighting && LightMapInteraction.GetType() == LMIT_Texture)
	{
		const FShadowMapInteraction ShadowMapInteraction = (MeshBatch.LCI && bIsLitMaterial)
			? MeshBatch.LCI->GetShadowMapInteraction()
			: FShadowMapInteraction();

		if (ShadowMapInteraction.GetType() == SMIT_Texture)
		{
			Process< FUniformLightMapPolicy >(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				Material,
				BlendMode,
				ShadingModel,
				FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING),
				MeshBatch.LCI,
				MeshFillMode,
				MeshCullMode);
		}
		else
		{
			Process< FUniformLightMapPolicy >(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				Material,
				BlendMode,
				ShadingModel,
				FUniformLightMapPolicy(LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING),
				MeshBatch.LCI,
				MeshFillMode,
				MeshCullMode);
		}
	}
	else if (bIsLitMaterial
		&& bAllowStaticLighting
		&& bUseVolumetricLightmap
		&& PrimitiveSceneProxy)
	{
		Process< FUniformLightMapPolicy >(
			MeshBatch,
			BatchElementMask,
			StaticMeshId,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			Material,
			BlendMode,
			ShadingModel,
			FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING),
			MeshBatch.LCI,
			MeshFillMode,
			MeshCullMode);
	}
	else if (bIsLitMaterial
		&& IsIndirectLightingCacheAllowed(FeatureLevel)
		&& bAllowIndirectLightingCache
		&& PrimitiveSceneProxy)
	{
		const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
		const bool bPrimitiveIsMovable = PrimitiveSceneProxy->IsMovable();
		const bool bPrimitiveUsesILC = PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;

		// Use the indirect lighting cache shaders if the object has a cache allocation
		// This happens for objects with unbuilt lighting
		if (bPrimitiveUsesILC &&
			((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
				// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
				// And movable objects are sometimes rendered in the static draw lists
				|| bPrimitiveIsMovable))
		{
			// Use a lightmap policy that supports reading indirect lighting from a single SH sample
			Process< FUniformLightMapPolicy >(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				Material,
				BlendMode,
				ShadingModel,
				FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING),
				MeshBatch.LCI,
				MeshFillMode,
				MeshCullMode);
		}
		else
		{
			Process< FUniformLightMapPolicy >(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				Material,
				BlendMode,
				ShadingModel,
				FUniformLightMapPolicy(LMP_SIMPLE_NO_LIGHTMAP),
				MeshBatch.LCI,
				MeshFillMode,
				MeshCullMode);
		}
	}
	else if (bIsLitMaterial)
	{
		// Always choosing shaders to support dynamic directional even if one is not present
		Process< FUniformLightMapPolicy >(
			MeshBatch,
			BatchElementMask,
			StaticMeshId,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			Material,
			BlendMode,
			ShadingModel,
			FUniformLightMapPolicy(LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING),
			MeshBatch.LCI,
			MeshFillMode,
			MeshCullMode);
	}
	else
	{
		Process< FUniformLightMapPolicy >(
			MeshBatch,
			BatchElementMask,
			StaticMeshId,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			Material,
			BlendMode,
			ShadingModel,
			FUniformLightMapPolicy(LMP_SIMPLE_NO_LIGHTMAP),
			MeshBatch.LCI,
			MeshFillMode,
			MeshCullMode);
	}
}

void FBasePassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material.GetShadingModel();
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);


		bool bShouldDraw = false;

		if (bTranslucentBasePass)
		{
			if (bIsTranslucent && !Material.IsDeferredDecal())
			{
				switch (TranslucencyPassType)
				{
				case ETranslucencyPass::TPT_StandardTranslucency:
					bShouldDraw = !Material.IsTranslucencyAfterDOFEnabled();
					break;

				case ETranslucencyPass::TPT_TranslucencyAfterDOF:
					bShouldDraw = Material.IsTranslucencyAfterDOFEnabled();
					break;

				case ETranslucencyPass::TPT_AllTranslucency:
					bShouldDraw = true;
					break;
				}
			}
		}
		else
		{
			bShouldDraw = !bIsTranslucent;
		}


		// Only draw opaque materials.
		if (bShouldDraw
			&& (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
		{
			// Check for a cached light-map.
			const bool bIsLitMaterial = (ShadingModel != MSM_Unlit);
			static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
			const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

			const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && MeshBatch.LCI && bIsLitMaterial)
				? MeshBatch.LCI->GetLightMapInteraction(FeatureLevel)
				: FLightMapInteraction();

			// force LQ lightmaps based on system settings
			const bool bPlatformAllowsHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel);
			const bool bAllowHighQualityLightMaps = bPlatformAllowsHighQualityLightMaps && LightMapInteraction.AllowsHighQualityLightmaps();

			const bool bAllowIndirectLightingCache = Scene && Scene->PrecomputedLightVolumes.Num() > 0;
			const bool bUseVolumetricLightmap = Scene && Scene->VolumetricLightmapSceneData.HasData();

			FMeshMaterialShaderElementData MeshMaterialShaderElementData;
			MeshMaterialShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

			if (IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)))
			{
				// Only compiling simple lighting shaders for HQ lightmaps to save on permutations
				check(bPlatformAllowsHighQualityLightMaps);
				AddMeshBatchForSimpleForwardShading(
					MeshBatch,
					BatchElementMask,
					StaticMeshId,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					Material,
					LightMapInteraction,
					bIsLitMaterial,
					bAllowStaticLighting,
					bUseVolumetricLightmap,
					bAllowIndirectLightingCache,
					MeshFillMode,
					MeshCullMode);
			}
			// Render volumetric translucent self-shadowing only for >= SM4 and fallback to non-shadowed for lesser shader models
			else if (bIsLitMaterial
				&& bIsTranslucent
				&& PrimitiveSceneProxy
				&& PrimitiveSceneProxy->CastsVolumetricTranslucentShadow())
			{
				checkSlow(ViewIfDynamicMeshCommand && ViewIfDynamicMeshCommand->bIsViewInfo);
				const FViewInfo* ViewInfo = (FViewInfo*)ViewIfDynamicMeshCommand;

				const int32 PrimitiveIndex = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex();

				const FUniformBufferRHIRef* UniformBufferPtr = ViewInfo->TranslucentSelfShadowUniformBufferMap.Find(PrimitiveIndex);

				FSelfShadowLightCacheElementData ElementData;
				ElementData.LCI = MeshBatch.LCI;
				ElementData.SelfShadowTranslucencyUniformBuffer = UniformBufferPtr ? (*UniformBufferPtr).GetReference() : GEmptyTranslucentSelfShadowUniformBuffer.GetUniformBufferRHI();

				if (bIsLitMaterial
					&& bAllowStaticLighting
					&& bUseVolumetricLightmap
					&& PrimitiveSceneProxy)
				{
					Process< FSelfShadowedVolumetricLightmapPolicy >(
						MeshBatch,
						BatchElementMask,
						StaticMeshId,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						BlendMode,
						ShadingModel,
						FSelfShadowedVolumetricLightmapPolicy(),
						ElementData,
						MeshFillMode,
						MeshCullMode);
				}
				else if (IsIndirectLightingCacheAllowed(FeatureLevel)
					&& bAllowIndirectLightingCache
					&& PrimitiveSceneProxy)
				{
					// Apply cached point indirect lighting as well as self shadowing if needed
					Process< FSelfShadowedCachedPointIndirectLightingPolicy >(
						MeshBatch,
						BatchElementMask,
						StaticMeshId,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						BlendMode,
						ShadingModel,
						FSelfShadowedCachedPointIndirectLightingPolicy(),
						ElementData,
						MeshFillMode,
						MeshCullMode);
				}
				else
				{
					Process< FSelfShadowedTranslucencyPolicy >(
						MeshBatch,
						BatchElementMask,
						StaticMeshId,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						BlendMode,
						ShadingModel,
						FSelfShadowedTranslucencyPolicy(),
						ElementData.SelfShadowTranslucencyUniformBuffer,
						MeshFillMode,
						MeshCullMode);
				}
			}
			else
			{
				static const auto CVarSupportLowQualityLightmap = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
				const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmap) || (CVarSupportLowQualityLightmap->GetValueOnAnyThread() != 0);

				switch (LightMapInteraction.GetType())
				{
				case LMIT_Texture:
					if (bAllowHighQualityLightMaps)
					{
						const FShadowMapInteraction ShadowMapInteraction = (bAllowStaticLighting && MeshBatch.LCI && bIsLitMaterial)
							? MeshBatch.LCI->GetShadowMapInteraction()
							: FShadowMapInteraction();

						if (ShadowMapInteraction.GetType() == SMIT_Texture)
						{
							Process< FUniformLightMapPolicy >(
								MeshBatch,
								BatchElementMask,
								StaticMeshId,
								PrimitiveSceneProxy,
								MaterialRenderProxy,
								Material,
								BlendMode,
								ShadingModel,
								FUniformLightMapPolicy(LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP),
								MeshBatch.LCI,
								MeshFillMode,
								MeshCullMode);
						}
						else
						{
							Process< FUniformLightMapPolicy >(
								MeshBatch,
								BatchElementMask,
								StaticMeshId,
								PrimitiveSceneProxy,
								MaterialRenderProxy,
								Material,
								BlendMode,
								ShadingModel,
								FUniformLightMapPolicy(LMP_HQ_LIGHTMAP),
								MeshBatch.LCI,
								MeshFillMode,
								MeshCullMode);
						}
					}
					else if (bAllowLowQualityLightMaps)
					{
						Process< FUniformLightMapPolicy >(
							MeshBatch,
							BatchElementMask,
							StaticMeshId,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							BlendMode,
							ShadingModel,
							FUniformLightMapPolicy(LMP_LQ_LIGHTMAP),
							MeshBatch.LCI,
							MeshFillMode,
							MeshCullMode);
					}
					else
					{
						Process< FUniformLightMapPolicy >(
							MeshBatch,
							BatchElementMask,
							StaticMeshId,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							BlendMode,
							ShadingModel,
							FUniformLightMapPolicy(LMP_NO_LIGHTMAP),
							MeshBatch.LCI,
							MeshFillMode,
							MeshCullMode);
					}
					break;
				default:
					if (bIsLitMaterial
						&& bAllowStaticLighting
						&& Scene
						&& Scene->VolumetricLightmapSceneData.HasData()
						&& PrimitiveSceneProxy
						&& (PrimitiveSceneProxy->IsMovable()
							|| PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting()
							|| PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric))
					{
						Process< FUniformLightMapPolicy >(
							MeshBatch,
							BatchElementMask,
							StaticMeshId,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							BlendMode,
							ShadingModel,
							FUniformLightMapPolicy(LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING),
							MeshBatch.LCI,
							MeshFillMode,
							MeshCullMode);
					}
					else if (bIsLitMaterial
						&& IsIndirectLightingCacheAllowed(FeatureLevel)
						&& Scene
						&& Scene->PrecomputedLightVolumes.Num() > 0
						&& PrimitiveSceneProxy)
					{
						const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
						const bool bPrimitiveIsMovable = PrimitiveSceneProxy->IsMovable();
						const bool bPrimitiveUsesILC = PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;

						// Use the indirect lighting cache shaders if the object has a cache allocation
						// This happens for objects with unbuilt lighting
						if (bPrimitiveUsesILC &&
							((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
								// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
								// And movable objects are sometimes rendered in the static draw lists
								|| bPrimitiveIsMovable))
						{
							if (CanIndirectLightingCacheUseVolumeTexture(FeatureLevel)
								// Translucency forces point sample for pixel performance
								&& !bIsTranslucent
								&& ((IndirectLightingCacheAllocation && !IndirectLightingCacheAllocation->bPointSample)
									|| (bPrimitiveIsMovable && PrimitiveSceneProxy->GetIndirectLightingCacheQuality() == ILCQ_Volume)))
							{
								// Use a lightmap policy that supports reading indirect lighting from a volume texture for dynamic objects
								Process< FUniformLightMapPolicy >(
									MeshBatch,
									BatchElementMask,
									StaticMeshId,
									PrimitiveSceneProxy,
									MaterialRenderProxy,
									Material,
									BlendMode,
									ShadingModel,
									FUniformLightMapPolicy(LMP_CACHED_VOLUME_INDIRECT_LIGHTING),
									MeshBatch.LCI,
									MeshFillMode,
									MeshCullMode);
							}
							else
							{
								// Use a lightmap policy that supports reading indirect lighting from a single SH sample
								Process< FUniformLightMapPolicy >(
									MeshBatch,
									BatchElementMask,
									StaticMeshId,
									PrimitiveSceneProxy,
									MaterialRenderProxy,
									Material,
									BlendMode,
									ShadingModel,
									FUniformLightMapPolicy(LMP_CACHED_POINT_INDIRECT_LIGHTING),
									MeshBatch.LCI,
									MeshFillMode,
									MeshCullMode);
							}
						}
						else
						{
							Process< FUniformLightMapPolicy >(
								MeshBatch,
								BatchElementMask,
								StaticMeshId,
								PrimitiveSceneProxy,
								MaterialRenderProxy,
								Material,
								BlendMode,
								ShadingModel,
								FUniformLightMapPolicy(LMP_NO_LIGHTMAP),
								MeshBatch.LCI,
								MeshFillMode,
								MeshCullMode);
						}
					}
					else
					{
						Process< FUniformLightMapPolicy >(
							MeshBatch,
							BatchElementMask,
							StaticMeshId,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							BlendMode,
							ShadingModel,
							FUniformLightMapPolicy(LMP_NO_LIGHTMAP),
							MeshBatch.LCI,
							MeshFillMode,
							MeshCullMode);
					}
					break;
				};
			}
		}
	}
}

FBasePassMeshProcessor::FBasePassMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InDrawRenderState, FMeshPassDrawListContext* InDrawListContext, ETranslucencyPass::Type InTranslucencyPassType)
	: FMeshPassProcessor(Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InDrawRenderState)
	, TranslucencyPassType(InTranslucencyPassType)
	, bTranslucentBasePass(InTranslucencyPassType != ETranslucencyPass::TPT_MAX)
	, EarlyZPassMode(Scene ? Scene->EarlyZPassMode : DDM_None)
{
}

FMeshPassProcessor* CreateBasePassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.OpaqueBasePassUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	SetupBasePassState(Scene->DefaultBasePassDepthStencilAccess, false, PassDrawRenderState);

	return new(FMemStack::Get()) FBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}

FMeshPassProcessor* CreateTranslucencyStandardPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.TranslucentBasePassUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	return new(FMemStack::Get()) FBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, ETranslucencyPass::TPT_StandardTranslucency);
}

FMeshPassProcessor* CreateTranslucencyAfterDOFProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.TranslucentBasePassUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	return new(FMemStack::Get()) FBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, ETranslucencyPass::TPT_TranslucencyAfterDOF);
}

FMeshPassProcessor* CreateTranslucencyAllPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.TranslucentBasePassUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	return new(FMemStack::Get()) FBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, ETranslucencyPass::TPT_AllTranslucency);
}

FRegisterPassProcessorCreateFunction RegisterBasePass(&CreateBasePassProcessor, EShadingPath::Deferred, EMeshPass::BasePass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterTranslucencyStandardPass(&CreateTranslucencyStandardPassProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyStandard, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterTranslucencyAfterDOFPass(&CreateTranslucencyAfterDOFProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyAfterDOF, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterTranslucencyAllPass(&CreateTranslucencyAllPassProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyAll, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);