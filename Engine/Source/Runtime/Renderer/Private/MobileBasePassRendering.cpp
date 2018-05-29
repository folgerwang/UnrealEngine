// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "MobileBasePassRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "StaticMeshDrawList.h"
#include "ScenePrivate.h"
#include "ShaderPlatformQualitySettings.h"
#include "MaterialShaderQualitySettings.h"
#include "PrimitiveSceneInfo.h"

#include "FramePro/FrameProProfiler.h"

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarMobileDisableVertexFog(
	TEXT("r.Mobile.DisableVertexFog"),
	1,
	TEXT("Set to 1 to disable vertex fogging in all mobile shaders."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarMobileUseLegacyShadingModel(
	TEXT("r.Mobile.UseLegacyShadingModel"),
	0,
	TEXT("If 1 then use legacy (pre 4.20) shading model (such as spherical guassian specular calculation.) (will cause a shader rebuild)"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);


// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarMobileSeparateMaskedPass(
	TEXT("r.Mobile.SeparateMaskedPass"),
	1,
	TEXT("Draw masked primitives in separate pass after all opaque (default)"),
	ECVF_RenderThreadSafe);
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FMobileBasePassUniformParameters, TEXT("MobileBasePass"));

static TAutoConsoleVariable<int32> CVarMobileParallelBasePass(
	TEXT("r.Mobile.ParallelBasePass"),
	0,
	TEXT("Toggles parallel base pass rendering for the mobile renderer. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
);


#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TMobileBasePassVS< LightMapPolicyType, LDR_GAMMA_32 > TMobileBasePassVS##LightMapPolicyName##LDRGamma32; \
	typedef TMobileBasePassVS< LightMapPolicyType, HDR_LINEAR_64 > TMobileBasePassVS##LightMapPolicyName##HDRLinear64; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassVS##LightMapPolicyName##LDRGamma32, TEXT("/Engine/Private/MobileBasePassVertexShader.usf"), TEXT("Main"), SF_Vertex); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassVS##LightMapPolicyName##HDRLinear64, TEXT("/Engine/Private/MobileBasePassVertexShader.usf"), TEXT("Main"), SF_Vertex);

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName,NumMovablePointLights) \
	typedef TMobileBasePassPS< LightMapPolicyType, LDR_GAMMA_32, false, NumMovablePointLights > TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##LDRGamma32; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_64, false, NumMovablePointLights > TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##HDRLinear64; \
	typedef TMobileBasePassPS< LightMapPolicyType, LDR_GAMMA_32, true, NumMovablePointLights > TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##LDRGamma32##Skylight; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_64, true, NumMovablePointLights > TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##HDRLinear64##Skylight; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##LDRGamma32, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##HDRLinear64, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##LDRGamma32##Skylight, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumMovablePointLights##HDRLinear64##Skylight, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel);

static_assert(MAX_BASEPASS_DYNAMIC_POINT_LIGHTS == 4, "If you change MAX_BASEPASS_DYNAMIC_POINT_LIGHTS, you need to add shader types below");

// Permutations for the number of point lights to support. INT32_MAX indicates the shader should use branching to support a variable number of point lights.
#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 0) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 1) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 2) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 3) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 4) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, INT32_MAX)

// Implement shader types per lightmap policy 
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP>, FMobileDistanceFieldShadowsAndLQLightMapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM>, FMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT>, FMobileDirectionalLightAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT>, FMobileMovableDirectionalLightAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT>, FMobileMovableDirectionalLightCSMAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT>, FMobileDirectionalLightCSMAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT>, FMobileMovableDirectionalLightLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM>, FMobileMovableDirectionalLightCSMLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP>, FMobileMovableDirectionalLightWithLightmapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP>, FMobileMovableDirectionalLightCSMWithLightmapPolicy);

const FLightSceneInfo* GetSceneMobileDirectionalLights(FScene const* Scene, uint32 LightChannel)
{
	return Scene->MobileDirectionalLights[LightChannel];
}

template<typename PixelParametersType>
bool TMobileBasePassPSPolicyParamType<PixelParametersType>::ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment)
{
	// Get quality settings for shader platform
	const UShaderPlatformQualitySettings* MaterialShadingQuality = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(Platform);
	const FMaterialQualityOverrides& QualityOverrides = MaterialShadingQuality->GetQualityOverrides(QualityLevel);

	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_FULLY_ROUGH"), QualityOverrides.bEnableOverride && QualityOverrides.bForceFullyRough != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_NONMETAL"), QualityOverrides.bEnableOverride && QualityOverrides.bForceNonMetal != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("QL_FORCEDISABLE_LM_DIRECTIONALITY"), QualityOverrides.bEnableOverride && QualityOverrides.bForceDisableLMDirectionality != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_LQ_REFLECTIONS"), QualityOverrides.bEnableOverride && QualityOverrides.bForceLQReflections != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_CSM_QUALITY"), (uint32)QualityOverrides.MobileCSMQuality);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_DISABLE_MATERIAL_NORMAL"), QualityOverrides.bEnableOverride && QualityOverrides.bDisableMaterialNormalCalculation);
	return true;
}

bool UseSkyReflectionCapture(const FScene* RenderScene)
{
	return RenderScene
		&& RenderScene->ReflectionSceneData.RegisteredReflectionCapturePositions.Num() == 0
		&& RenderScene->SkyLight
		&& RenderScene->SkyLight->ProcessedTexture->TextureRHI;
}

void GetSkyTextureParams(const FScene* Scene, float& AverageBrightnessOUT, FTexture*& ReflectionTextureOUT, float& OutSkyMaxMipIndex)
{
	if (Scene && Scene->SkyLight && Scene->SkyLight->ProcessedTexture->TextureRHI)
	{
		AverageBrightnessOUT = Scene->SkyLight->AverageBrightness;
		ReflectionTextureOUT = Scene->SkyLight->ProcessedTexture;
		OutSkyMaxMipIndex = FMath::Log2(ReflectionTextureOUT->GetSizeX());
	}
}

FMobileBasePassMovablePointLightInfo::FMobileBasePassMovablePointLightInfo(const FPrimitiveSceneProxy* InSceneProxy)
: NumMovablePointLights(0)
{
	static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
	const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnRenderThread();

	if (InSceneProxy != nullptr)
	{
		for (FLightPrimitiveInteraction* LPI = InSceneProxy->GetPrimitiveSceneInfo()->LightList; LPI && NumMovablePointLights < MobileNumDynamicPointLights; LPI = LPI->GetNextLight())
		{
			FLightSceneProxy* LightProxy = LPI->GetLight()->Proxy;
			if (LightProxy->GetLightType() == LightType_Point && LightProxy->IsMovable() && (LightProxy->GetLightingChannelMask() & InSceneProxy->GetLightingChannelMask()) != 0)
			{
				FLightParameters LightParameters;

				LightProxy->GetParameters(LightParameters);

				LightPositionAndInvRadius[NumMovablePointLights] = LightParameters.LightPositionAndInvRadius;
				LightColorAndFalloffExponent[NumMovablePointLights] = LightParameters.LightColorAndFalloffExponent;

				if (LightProxy->IsInverseSquared())
				{
					LightColorAndFalloffExponent[NumMovablePointLights].W = 0;
				}

				NumMovablePointLights++;
			}
		}
	}
}

/** The action used to draw a base pass static mesh element. */
class FDrawMobileBasePassStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawMobileBasePassStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	bool CanUseDrawlistToToggleCombinedStaticAndCSM(const FPrimitiveSceneProxy* PrimitiveSceneProxy, ELightMapPolicyType LightMapPolicyType) const
	{
		switch (LightMapPolicyType)
		{
			case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
			case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
			case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			{
				static auto* CVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
				return CVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnRenderThread() != 0;
			}
			case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
			case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
			case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
			case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
			{
				static auto* CVarMobileEnableMovableLightCSMShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableMovableLightCSMShaderCulling"));
				return CVarMobileEnableMovableLightCSMShaderCulling->GetValueOnRenderThread() != 0;
			}
			default:
			{
				return false;
			}
		}
	}

	bool CanReceiveCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy) const
	{
		// For movable directional lights, when CSM culling is disabled the default behavior is to receive CSM.
		static auto* CVarMobileEnableMovableLightCSMShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableMovableLightCSMShaderCulling"));
		if (LightSceneInfo && LightSceneInfo->Proxy->IsMovable() && CVarMobileEnableMovableLightCSMShaderCulling->GetValueOnRenderThread() == 0)
		{		
			return true;
		}

		// If culling is enabled then CSM receiving is determined during InitDynamicShadows.
		// If culling is disabled then stationary directional lights default to no CSM. 
		return false; 
	}

	const FScene* GetScene() const 
	{ 
		return Scene;
	}

	/** Draws the mesh with a specific light-map type */
	void Process(
		FRHICommandList& RHICmdList, 
		const FMobileProcessBasePassMeshParameters& Parameters,
		const FUniformLightMapPolicy& LightMapPolicy,
		const FUniformLightMapPolicy::ElementDataType& LightMapElementData
		) const
	{
		EBasePassDrawListType DrawType = EBasePass_Default;

		if (StaticMesh->IsMasked(Parameters.FeatureLevel) && CVarMobileSeparateMaskedPass.GetValueOnRenderThread() != 0)
		{
			DrawType = EBasePass_Masked;	
		}

		if ( Scene )
		{
			// Determine if this primitive has the possibility of using combined static and CSM.
			if (CanUseDrawlistToToggleCombinedStaticAndCSM(Parameters.PrimitiveSceneProxy, LightMapPolicy.GetIndirectPolicy()))
			{
				// if applicable, returns the corresponding CSM or non-CSM lightmap policy of LightMapPolicyType
				auto GetAlternativeLightMapPolicy = [](ELightMapPolicyType LightMapPolicyType)
				{
					switch (LightMapPolicyType)
					{
						case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
							return LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP;
						case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
							return LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT;
						case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
							return LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM;
						case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
							return LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT;

						// movable light CSMs
						case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
							return LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP;
						case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
							return LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP;

						case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
							return LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT;
						case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
							return LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT;

						case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
							return LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT;
						case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
							return LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM;

					}
					return LightMapPolicyType;
				};

				const ELightMapPolicyType AlternativeLightMapPolicy = GetAlternativeLightMapPolicy(LightMapPolicy.GetIndirectPolicy());
				const bool bHasCSMCounterpart = AlternativeLightMapPolicy != LightMapPolicy.GetIndirectPolicy();
				if (bHasCSMCounterpart)
				{
					// Is the passed in lightmap policy CSM capable or not
					const bool bIsCSMCapableLightPolicy = LightMapPolicy.GetIndirectPolicy() == LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM
						|| LightMapPolicy.GetIndirectPolicy() == LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT
						|| LightMapPolicy.GetIndirectPolicy() == LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP
						|| LightMapPolicy.GetIndirectPolicy() == LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT
						|| LightMapPolicy.GetIndirectPolicy() == LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM
						;

					if (bIsCSMCapableLightPolicy)
					{
						// Alternative policy is the non-CSM version.
						AddMeshToStaticDrawList(Scene->GetMobileBasePassCSMDrawList<FUniformLightMapPolicy>(DrawType), Parameters, LightMapPolicy, LightMapElementData);
						AddMeshToStaticDrawList(Scene->GetMobileBasePassDrawList<FUniformLightMapPolicy>(DrawType), Parameters, FUniformLightMapPolicy(AlternativeLightMapPolicy), LightMapElementData);
					}
					else
					{
						// Alternative policy is the CSM version.
						AddMeshToStaticDrawList(Scene->GetMobileBasePassCSMDrawList<FUniformLightMapPolicy>(DrawType), Parameters, FUniformLightMapPolicy(AlternativeLightMapPolicy), LightMapElementData);
						AddMeshToStaticDrawList(Scene->GetMobileBasePassDrawList<FUniformLightMapPolicy>(DrawType), Parameters, LightMapPolicy, LightMapElementData);
					}

					return; // avoid adding to draw list twice.
				}
			}

			AddMeshToStaticDrawList(Scene->GetMobileBasePassDrawList<FUniformLightMapPolicy>(DrawType), Parameters, LightMapPolicy, LightMapElementData);
		}
	}

	template<typename LightMapPolicyType>
	void AddMeshToStaticDrawList(TStaticMeshDrawList<TMobileBasePassDrawingPolicy<LightMapPolicyType>> &DrawList,
		const FMobileProcessBasePassMeshParameters &Parameters, const LightMapPolicyType& LightMapPolicy, const typename LightMapPolicyType::ElementDataType& LightMapElementData) const
	{
		ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();
		// Add the static mesh to the draw list.
		DrawList.AddMesh(
			StaticMesh,
			typename TMobileBasePassDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
			TMobileBasePassDrawingPolicy<LightMapPolicyType>(
				StaticMesh->VertexFactory,
				StaticMesh->MaterialRenderProxy,
				*Parameters.Material,
				LightMapPolicy,
				Parameters.NumMovablePointLights,
				Parameters.BlendMode,
				Parameters.ShadingModel != MSM_Unlit && Scene->ShouldRenderSkylightInBasePass(Parameters.BlendMode),
				ComputeMeshOverrideSettings(Parameters.Mesh),
				DVSM_None,
				FeatureLevel,
				IsMobileHDR() // bEnableReceiveDecalOutput
				),
			FeatureLevel
		);
	}
};

void FMobileBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
{
	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = Scene->GetFeatureLevel();
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if( !IsTranslucentBlendMode(BlendMode) )
	{
		// following check moved from ProcessMobileBasePassMesh to avoid passing feature level.
		check(!AllowHighQualityLightmaps(Scene->GetFeatureLevel()));

		const bool bIsUnlit = Material->GetShadingModel() == MSM_Unlit;

		ProcessMobileBasePassMesh<FDrawMobileBasePassStaticMeshAction>(
			RHICmdList,
			FMobileProcessBasePassMeshParameters(
				*StaticMesh,
				Material,
				StaticMesh->PrimitiveSceneInfo->Proxy,
				true,
				FeatureLevel
			),
			FDrawMobileBasePassStaticMeshAction(Scene, StaticMesh)
		);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawMobileBasePassDynamicMeshAction
{
public:

	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	FHitProxyId HitProxyId;

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	bool CanReceiveCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy) const
	{
		if (PrimitiveSceneProxy == nullptr || LightSceneInfo == nullptr)
		{
			return false;
		}

		// Check that this primitive is eligible for CSM.
 		const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];

		static auto* CVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
		static auto* CVarMobileEnableMovableLightCSMShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableMovableLightCSMShaderCulling"));
		const bool bMobileEnableMovableLightCSMShaderCulling = CVarMobileEnableMovableLightCSMShaderCulling->GetValueOnRenderThread() == 1;
		const bool bMobileEnableStaticAndCSMShadowReceivers = CVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnRenderThread() == 1;

		const bool bMovableLight = LightSceneInfo->Proxy->IsMovable();
		const bool bMovableLightCastsCSM = bMovableLight && LightSceneInfo->ShouldRenderViewIndependentWholeSceneShadows();

		return PrimitiveSceneProxy->ShouldReceiveMobileCSMShadows()
			&&	(	// movable CSM culling is disabled and movable light is in use
					(!bMobileEnableMovableLightCSMShaderCulling && bMovableLightCastsCSM)
					||
					// CSM culling is active
					(View.MobileCSMVisibilityInfo.bMobileDynamicCSMInUse
						&& (bMobileEnableStaticAndCSMShadowReceivers || bMobileEnableMovableLightCSMShaderCulling)
						&& View.MobileCSMVisibilityInfo.MobilePrimitiveCSMReceiverVisibilityMap[PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex()])
				);
	}

	const FScene* GetScene() const
	{
		auto* Scene = (FScene*)View.Family->Scene;
		return Scene;
	}

	/** Initialization constructor. */
	FDrawMobileBasePassDynamicMeshAction(
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

	/** Draws mesh with a specific light-map type, and shader complexity predicate. */
	void Process(
		FRHICommandList& RHICmdList, 
		const FMobileProcessBasePassMeshParameters& Parameters,
		const FUniformLightMapPolicy& LightMapPolicy,
		const typename FUniformLightMapPolicy::ElementDataType& LightMapElementData
		)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Treat masked materials as if they don't occlude in shader complexity, which is PVR behavior
		if(Parameters.BlendMode == BLEND_Masked && View.Family->EngineShowFlags.ShaderComplexity)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
		}
#endif

		const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		TMobileBasePassDrawingPolicy<FUniformLightMapPolicy> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.NumMovablePointLights,
			Parameters.BlendMode,
			Parameters.ShadingModel != MSM_Unlit && Scene && Scene->ShouldRenderSkylightInBasePass(Parameters.BlendMode),
			ComputeMeshOverrideSettings(Parameters.Mesh),
			View.Family->GetDebugViewShaderMode(),
			View.GetFeatureLevel(),
			IsMobileHDR() // bEnableReceiveDecalOutput
			);

		DrawingPolicy.SetupPipelineState(DrawRenderState, View);
		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderState, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, DrawRenderState, &View, typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy>::ContextDataType());

		for( int32 BatchElementIndex=0;BatchElementIndex<Parameters.Mesh.Elements.Num();BatchElementIndex++ )
		{
			TDrawEvent<FRHICommandList> MeshEvent;
			BeginMeshDrawEvent(RHICmdList, Parameters.PrimitiveSceneProxy, Parameters.Mesh, MeshEvent, EnumHasAnyFlags(EShowMaterialDrawEventTypes(GShowMaterialDrawEventTypes), EShowMaterialDrawEventTypes::MobileBasePass));

			DrawingPolicy.SetMeshRenderState(
				RHICmdList, 
				View,
				Parameters.PrimitiveSceneProxy,
				Parameters.Mesh,
				BatchElementIndex,
				DrawRenderState,
				typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy>::ElementDataType(LightMapElementData),
				typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy>::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, View, Parameters.Mesh, BatchElementIndex);
		}
	}
};

bool FMobileBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = View.GetFeatureLevel();
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if(!IsTranslucentBlendMode(BlendMode))
	{
		ProcessMobileBasePassMesh<FDrawMobileBasePassDynamicMeshAction>(
		RHICmdList, 
		FMobileProcessBasePassMeshParameters(
			Mesh, 
			Material, 
			PrimitiveSceneProxy, 
			true, 
			View.GetFeatureLevel()
		),
		FDrawMobileBasePassDynamicMeshAction(
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

/** Base pass sorting modes. */
namespace EBasePassSort
{
	enum Type
	{
		/** Automatically select based on hardware/platform. */
		Auto = 0,
		/** No sorting. */
		None = 1,
		/** Sorts state buckets, not individual meshes. */
		SortStateBuckets = 2,
		/** Per mesh sorting. */
		SortPerMesh = 3,

		/** Useful range of sort modes. */
		FirstForcedMode = None,
		LastForcedMode = SortPerMesh
	};
};
TAutoConsoleVariable<int32> GSortBasePass(TEXT("r.ForwardBasePassSort"),0,
	TEXT("How to sort the mobile base pass:\n")
	TEXT("\t0: Decide automatically based on the hardware and threading configuration.\n")
	TEXT("\t1: No sorting.\n")
	TEXT("\t2: Sort drawing policies.\n")
	TEXT("\t3: Sort drawing policies and the meshes within them. Will not use the parallel path."),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> GMaxBasePassDraws(TEXT("r.MaxForwardBasePassDraws"),0,TEXT("Stops rendering static mobile base pass draws after the specified number of times. Useful for seeing the order in which meshes render when optimizing."),ECVF_RenderThreadSafe);

EBasePassSort::Type GetSortMode()
{
	int32 SortMode = GSortBasePass.GetValueOnRenderThread();
	if (SortMode >= EBasePassSort::FirstForcedMode && SortMode <= EBasePassSort::LastForcedMode)
	{
		return (EBasePassSort::Type)SortMode;
	}

	// Determine automatically.
	if (GRHICommandList.UseParallelAlgorithms() || GHardwareHiddenSurfaceRemoval)
	{
		return EBasePassSort::None;
	}
	else
	{
		return EBasePassSort::SortPerMesh;
	}
}

/** Helper function for drawing sorted meshes */
static void DrawVisibleFrontToBack(
	FRHICommandListImmediate& RHICmdList,
	FScene* const Scene,
	const EBasePassDrawListType DrawListType, 
	const FViewInfo& View, 
	const FDrawingPolicyRenderState& DrawRenderState,
	const FMobileCSMVisibilityInfo* const MobileCSMVisibilityInfo, 
	const StereoPair& StereoView, 
	const StereoPair& StereoViewCSM, 
	const StereoPair& StereoViewNonCSM, 
	int32& MaxDraws)
{
#if FRAMEPRO_ENABLED
	FFrameProProfiler::PushEvent();
#else
	QUICK_SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTimeFrontToBack);
#endif // FRAMEPRO_ENABLED

	const bool bIsCSM = MobileCSMVisibilityInfo != nullptr;
	int32 NumDraws = 0;
	if (View.bIsMobileMultiViewEnabled)
	{
		if (bIsCSM)
		{
			NumDraws += Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[DrawListType].DrawVisibleFrontToBackMobileMultiView(RHICmdList, StereoViewCSM, DrawRenderState, MaxDraws);
			NumDraws += Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisibleFrontToBackMobileMultiView(RHICmdList, StereoViewNonCSM, DrawRenderState, MaxDraws);
		}
		else
		{
			NumDraws += Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisibleFrontToBackMobileMultiView(RHICmdList, StereoView, DrawRenderState, MaxDraws);
		}
	}
	else
	{
		if (bIsCSM)
		{
			NumDraws += Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[DrawListType].DrawVisibleFrontToBack(RHICmdList, View, DrawRenderState, MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility, MaxDraws);
			NumDraws += Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisibleFrontToBack(RHICmdList, View, DrawRenderState, MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility, MaxDraws);
		}
		else
		{
			NumDraws += Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisibleFrontToBack(RHICmdList, View, DrawRenderState, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
		}
	}

	MaxDraws -= NumDraws;

#if FRAMEPRO_ENABLED
	FFrameProProfiler::PopEvent(*FString::Printf(TEXT("STAT_StaticDrawListDrawTimeFrontToBack (%d draws)"), NumDraws));
#endif // FRAMEPRO_ENABLED
}

/** Helper function for drawing unsorted meshes */
static void DrawVisible(
	FRHICommandListImmediate& RHICmdList,
	FScene* const Scene,
	const EBasePassDrawListType DrawListType,
	const FViewInfo& View,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FMobileCSMVisibilityInfo* const MobileCSMVisibilityInfo,
	const StereoPair& StereoView,
	const StereoPair& StereoViewCSM,
	const StereoPair& StereoViewNonCSM)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
	const bool bIsCSM = MobileCSMVisibilityInfo != nullptr;
	if (View.bIsMobileMultiViewEnabled)
	{
		if (bIsCSM)
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[DrawListType].DrawVisibleMobileMultiView(RHICmdList, StereoViewCSM, DrawRenderState);
			Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisibleMobileMultiView(RHICmdList, StereoViewNonCSM, DrawRenderState);
		}
		else
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisibleMobileMultiView(RHICmdList, StereoView, DrawRenderState);
		}
	}
	else
	{
		if (bIsCSM)
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[DrawListType].DrawVisible(RHICmdList, View, DrawRenderState, MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility);
			Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisible(RHICmdList, View, DrawRenderState, MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility);
		}
		else
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawListType].DrawVisible(RHICmdList, View, DrawRenderState, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
		}
	}
}

void CreateMobileBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	bool bTranslucentPass,
	TUniformBufferRef<FMobileBasePassUniformParameters>& BasePassUniformBuffer)
{
	FMobileBasePassUniformParameters BasePassParameters;
	SetupFogUniformParameters(View, BasePassParameters.Fog);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SetupMobileSceneTextureUniformParameters(SceneContext, View.FeatureLevel, bTranslucentPass, BasePassParameters.SceneTextures);

	BasePassUniformBuffer = TUniformBufferRef<FMobileBasePassUniformParameters>::CreateUniformBufferImmediate(BasePassParameters, UniformBuffer_SingleFrame);
}

void FMobileSceneRenderer::RenderMobileEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EditorDynamicPrimitiveDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, DynamicEd);

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked);

	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[FeatureLevel]) && !IsMobileHDR();

		// Draw the base pass for the view's batched mesh elements.
		DrawViewElements<FMobileBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, FMobileBasePassOpaqueDrawingPolicyFactory::ContextType(), SDPG_World, true);

		// Draw the view's batched simple elements(lines, sprites, etc).
		View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false);

		// Draw foreground objects last
		DrawViewElements<FMobileBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, FMobileBasePassOpaqueDrawingPolicyFactory::ContextType(), SDPG_Foreground, true);

		// Draw the view's batched simple elements(lines, sprites, etc).
		View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false);
	}
}

void FMobileSceneRenderer::RenderMobileBasePassDynamicData(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, EBlendMode BlendMode, bool bWireFrame, int32 FirstElement, int32 AfterLastElement)
{
	AfterLastElement = FMath::Min(View.DynamicMeshElements.Num(), AfterLastElement);

	if (FirstElement >= AfterLastElement)
	{
		return;
	}
	SCOPE_CYCLE_COUNTER(STAT_DynamicPrimitiveDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

	FMobileBasePassOpaqueDrawingPolicyFactory::ContextType Context;

	for (int32 Index = FirstElement; Index < AfterLastElement; Index++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[Index];

		if ((BlendMode == BLEND_Opaque && MeshBatchAndRelevance.GetHasOpaqueMaterial()) || 
			(BlendMode == BLEND_Masked && MeshBatchAndRelevance.GetHasMaskedMaterial()) || bWireFrame)
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FMobileBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
		}
	}
}

struct FMobileBasePassViewInfo
{
	const FMobileCSMVisibilityInfo* MobileCSMVisibilityInfo;
	const FMobileCSMVisibilityInfo* MobileCSMVisibilityInfoStereo;
	StereoPair StereoView;
	StereoPair StereoViewCSM;
	StereoPair StereoViewNonCSM;


	FMobileBasePassViewInfo(const FViewInfo& View, TArray<FViewInfo>& Views)
	{
		MobileCSMVisibilityInfo = View.MobileCSMVisibilityInfo.bMobileDynamicCSMInUse ? &View.MobileCSMVisibilityInfo : nullptr;
		MobileCSMVisibilityInfoStereo = nullptr;

		if (View.bIsMobileMultiViewEnabled)
		{
			checkSlow(Views.Num() > 1);
			StereoView.LeftView = &Views[0];
			StereoView.RightView = &Views[1];
			StereoView.LeftViewVisibilityMap = &Views[0].StaticMeshVisibilityMap;
			StereoView.LeftViewBatchVisibilityArray = &Views[0].StaticMeshBatchVisibility;
			StereoView.RightViewVisibilityMap = &Views[1].StaticMeshVisibilityMap;
			StereoView.RightViewBatchVisibilityArray = &Views[1].StaticMeshBatchVisibility;

			if (MobileCSMVisibilityInfo)
			{
				MobileCSMVisibilityInfoStereo = &Views[1].MobileCSMVisibilityInfo;

				StereoViewCSM.LeftView = &Views[0];
				StereoViewCSM.RightView = &Views[1];
				StereoViewCSM.LeftViewVisibilityMap = &MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap;
				StereoViewCSM.LeftViewBatchVisibilityArray = &MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility;
				StereoViewCSM.RightViewVisibilityMap = &MobileCSMVisibilityInfoStereo->MobileCSMStaticMeshVisibilityMap;
				StereoViewCSM.RightViewBatchVisibilityArray = &MobileCSMVisibilityInfoStereo->MobileCSMStaticBatchVisibility;

				StereoViewNonCSM.LeftView = &Views[0];
				StereoViewNonCSM.RightView = &Views[1];
				StereoViewNonCSM.LeftViewVisibilityMap = &MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap;
				StereoViewNonCSM.LeftViewBatchVisibilityArray = &MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility;
				StereoViewNonCSM.RightViewVisibilityMap = &MobileCSMVisibilityInfoStereo->MobileNonCSMStaticMeshVisibilityMap;
				StereoViewNonCSM.RightViewBatchVisibilityArray = &MobileCSMVisibilityInfoStereo->MobileNonCSMStaticBatchVisibility;
			}
		}
	}
};

static void SetupMobileBasePassView(FRHICommandList& RHICmdList, const FViewInfo& View, FDrawingPolicyRenderState& DrawRenderState)
{
	// Opaque blending
	if (View.bIsPlanarReflection)
	{
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_Zero, BF_Zero>::GetRHI());
	}
	else
	{
		DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
	}

	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
}

DECLARE_CYCLE_STAT(TEXT("MobileBasepass"), STAT_CLP_MobileBasepass, STATGROUP_ParallelCommandListMarkers);

class FMobileBasePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	const FSceneViewFamily& ViewFamily;

	FMobileBasePassParallelCommandListSet(
		const FViewInfo& InView, 
		const FSceneRenderer* InSceneRenderer, 
		FRHICommandListImmediate& InParentCmdList, 
		bool bInParallelExecute, 
		bool bInCreateSceneContext, 
		const FSceneViewFamily& InViewFamily, 
		const FDrawingPolicyRenderState& InDrawRenderState)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_MobileBasepass), InView, InSceneRenderer, InParentCmdList, bInParallelExecute, bInCreateSceneContext, InDrawRenderState)
		, ViewFamily(InViewFamily)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FMobileBasePassParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		check(!bParallelExecute); // SetupMobileBasePassView is not (yet) complete enough for deferred contexts. Also need the rendertargets and ALL other state required!
		SetupMobileBasePassView(CmdList, View, DrawRenderState);
	}
};

class FRenderMobileBasePassDynamicDataThreadTask : public FRenderTask
{
	FMobileSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	int32 FirstElement;
	int32 AfterLastElement;
	EBlendMode BlendMode;
	bool bWireFrame;

public:

	FRenderMobileBasePassDynamicDataThreadTask(
		FMobileSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState,
		EBlendMode InBlendMode,
		bool bInWireFrame,
		int32 InFirstElement,
		int32 InAfterLastElement

	)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
		, FirstElement(InFirstElement)
		, AfterLastElement(InAfterLastElement)
		, BlendMode(InBlendMode)
		, bWireFrame(bInWireFrame)
	{
		check(FirstElement < AfterLastElement); // don't create useless tasks
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderMobileBasePassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderMobileBasePassDynamicData(RHICmdList, View, DrawRenderState, BlendMode, bWireFrame, FirstElement, AfterLastElement);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

static void RenderMobileBasePassDynamicDataParallel(FMobileSceneRenderer& ThisRenderer, FMobileBasePassParallelCommandListSet& ParallelSet, EBlendMode BlendMode, bool bWireframe)
{
	if (ParallelSet.View.DynamicMeshElements.Num() > 0)
	{
		int32 NumExpectedPrimitives = ParallelSet.View.DynamicMeshElements.Num() / 2; // opaque and masked rendered separately 
		int32 EffectiveThreads = FMath::Min<int32>(NumExpectedPrimitives, ParallelSet.Width);

		int32 NumPer = ParallelSet.View.DynamicMeshElements.Num() / EffectiveThreads;
		int32 Extra = ParallelSet.View.DynamicMeshElements.Num() - NumPer * EffectiveThreads;
		int32 Start = 0;
		for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
		{
			int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
			check(Last >= Start);

			{
				FRHICommandList* CmdList = ParallelSet.NewParallelCommandList();
				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderMobileBasePassDynamicDataThreadTask>::CreateTask(ParallelSet.GetPrereqs(), ENamedThreads::ActualRenderingThread)
					.ConstructAndDispatchWhenReady(ThisRenderer, *CmdList, ParallelSet.View, ParallelSet.DrawRenderState, BlendMode, bWireframe, Start, Last + 1);
				ParallelSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
			}

			Start = Last + 1;
		}
		check(Start == ParallelSet.View.DynamicMeshElements.Num());
	}
}

void FMobileSceneRenderer::RenderMobileBasePassViewParallel(const FViewInfo& View, FRHICommandListImmediate& ParentCmdList, TArray<FViewInfo>& InViews, const FDrawingPolicyRenderState& DrawRenderState)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderMobileBasePassViewParallel);
	FMobileBasePassViewInfo VI(View, InViews);

	bool bCreateContexts = false; // CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0;
	const bool bIsCSM = VI.MobileCSMVisibilityInfo != nullptr;
	check(!View.bIsMobileMultiViewEnabled && GetSortMode() != EBasePassSort::SortPerMesh); // easy to support, just isn't supported yet

	{
		FMobileBasePassParallelCommandListSet ParallelSet(View, this, ParentCmdList,
			false, // no support for deferred contexts yet
			bCreateContexts,
			ViewFamily,
			DrawRenderState
			);

		if (bIsCSM)
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_Default].DrawVisibleParallel(VI.MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, VI.MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility, ParallelSet);
			Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Default].DrawVisibleParallel(VI.MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, VI.MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility, ParallelSet);
		}
		else
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Default].DrawVisibleParallel(View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, ParallelSet);
		}
		
		const bool bWireframe = !!ViewFamily.EngineShowFlags.Wireframe;
		RenderMobileBasePassDynamicDataParallel(*this, ParallelSet, BLEND_Opaque, bWireframe);
	}

	RenderMobileEditorPrimitives(ParentCmdList, View, DrawRenderState);

	{
		FMobileBasePassParallelCommandListSet ParallelSet(View, this, ParentCmdList,
			false, // no support for deferred contexts yet
			bCreateContexts,
			ViewFamily,
			DrawRenderState
		);

		if (bIsCSM)
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_Masked].DrawVisibleParallel(VI.MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, VI.MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility, ParallelSet);
			Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Masked].DrawVisibleParallel(VI.MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, VI.MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility, ParallelSet);
		}
		else
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Masked].DrawVisibleParallel(View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, ParallelSet);
		}

		const bool bWireframe = !!ViewFamily.EngineShowFlags.Wireframe;
		if (!bWireframe)
		{
			RenderMobileBasePassDynamicDataParallel(*this, ParallelSet, BLEND_Masked, false);
		}
	}

}


void FMobileSceneRenderer::RenderMobileBasePass(FRHICommandListImmediate& RHICmdList, const TArrayView<const FViewInfo*> PassViews)
{
	SCOPED_DRAW_EVENT(RHICmdList, MobileBasePass);
	SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);

	EBasePassSort::Type SortMode = GetSortMode();
#if UE_BUILD_DEVELOPMENT
	if (SortMode == EBasePassSort::SortPerMesh)
	{
		FDrawListSortKey Test1;
		FDrawListSortKey Test2;
		FDrawListSortKey Test3;

		ZeroDrawListSortKey(Test1);
		ZeroDrawListSortKey(Test2);
		ZeroDrawListSortKey(Test3);
		Test1.Fields.bBackground = 1;
		Test2.Fields.MeshElementIndex = 1;
		Test3.Fields.DepthBits = 1;

		UE_CLOG(Test1 < Test2 || Test3 < Test2, LogRHI, Fatal, TEXT("FDrawListSortKey is using non-portable code that doesn't work"));
	}
#endif	

	int32 MaxDraws = GMaxBasePassDraws.GetValueOnRenderThread();
	if (MaxDraws <= 0)
	{
		MaxDraws = MAX_int32;
	}

	if (SortMode == EBasePassSort::SortStateBuckets)
	{
		SCOPE_CYCLE_COUNTER(STAT_SortStaticDrawLists);

		for (int32 DrawType = 0; DrawType < EBasePass_MAX; DrawType++)
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[DrawType].SortFrontToBack(Views[0].ViewLocation);
		}
	}

	if (MaxDraws == MAX_int32 && PassViews.Num() && !Views[0].bIsMobileMultiViewEnabled && // we don't support parallel multiview...it would not be hard to add
		SortMode != EBasePassSort::SortPerMesh && // we don't support sorting...not sure how hard it would be to add
		GRHICommandList.UseParallelAlgorithms() && CVarMobileParallelBasePass.GetValueOnRenderThread())
	{
		bool bFlush = true; // CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0;
		FScopedCommandListWaitForTasks Flusher(bFlush, RHICmdList); 
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			FViewInfo& View = Views[ViewIndex];
			if (!View.ShouldRenderView())
			{
				continue;
			}

			TUniformBufferRef<FMobileBasePassUniformParameters> BasePassUniformBuffer;
			CreateMobileBasePassUniformBuffer(RHICmdList, View, false, BasePassUniformBuffer);
			FDrawingPolicyRenderState DrawRenderState(View, BasePassUniformBuffer);
			SetupMobileBasePassView(RHICmdList, View, DrawRenderState);
			RenderMobileBasePassViewParallel(View, RHICmdList, Views, DrawRenderState);
		}
	}
	else
	{
		// Draw the scene's emissive and light-map color.
		for (int32 ViewIndex = 0; ViewIndex < PassViews.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			const FViewInfo& View = *PassViews[ViewIndex];

			if (!View.ShouldRenderView())
			{
				continue;
			}

			TUniformBufferRef<FMobileBasePassUniformParameters> BasePassUniformBuffer;
			CreateMobileBasePassUniformBuffer(RHICmdList, View, false, BasePassUniformBuffer);
			FDrawingPolicyRenderState DrawRenderState(View, BasePassUniformBuffer);

			SetupMobileBasePassView(RHICmdList, View, DrawRenderState);

			FMobileBasePassViewInfo VI(View, Views);

			// Render the base pass static data
			if (SortMode == EBasePassSort::SortPerMesh)
			{
				DrawVisibleFrontToBack(RHICmdList, Scene, EBasePass_Default, View, DrawRenderState, VI.MobileCSMVisibilityInfo, VI.StereoView, VI.StereoViewNonCSM, VI.StereoViewCSM, MaxDraws);
			}
			else
			{
				DrawVisible(RHICmdList, Scene, EBasePass_Default, View, DrawRenderState, VI.MobileCSMVisibilityInfo, VI.StereoView, VI.StereoViewNonCSM, VI.StereoViewCSM);
			}
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

			// render dynamic opaque primitives (or all if Wireframe)
			const bool bWireframe = !!ViewFamily.EngineShowFlags.Wireframe;
			RenderMobileBasePassDynamicData(RHICmdList, View, DrawRenderState, BLEND_Opaque, bWireframe);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

			RenderMobileEditorPrimitives(RHICmdList, View, DrawRenderState);

			// Issue static draw list masked draw calls last, as PVR wants it
			if (SortMode == EBasePassSort::SortPerMesh)
			{
				DrawVisibleFrontToBack(RHICmdList, Scene, EBasePass_Masked, View, DrawRenderState, VI.MobileCSMVisibilityInfo, VI.StereoView, VI.StereoViewNonCSM, VI.StereoViewCSM, MaxDraws);
			}
			else
			{
				DrawVisible(RHICmdList, Scene, EBasePass_Masked, View, DrawRenderState, VI.MobileCSMVisibilityInfo, VI.StereoView, VI.StereoViewNonCSM, VI.StereoViewCSM);
			}
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

			// render dynamic masked primitives (or none if Wireframe)
			if (!bWireframe)
			{
				RenderMobileBasePassDynamicData(RHICmdList, View, DrawRenderState, BLEND_Masked, false);
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			}
		}
	}
}
