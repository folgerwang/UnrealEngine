// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"

int32 GEnableRayTracingMaterials = 1;
static FAutoConsoleVariableRef CVarEnableRayTracingMaterials(
	TEXT("r.RayTracing.EnableMaterials"),
	GEnableRayTracingMaterials,
	TEXT(" 0: bind default material shader that outputs placeholder data\n")
	TEXT(" 1: bind real material shaders (default)\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingTextureLodCvar(
	TEXT("r.RayTracing.UseTextureLod"),
	0,
	TEXT("0 to disable texture LOD.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe);

static bool IsSupportedVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
{
	static FName LocalVfFname = FName(TEXT("FLocalVertexFactory"), FNAME_Find);
	static FName LSkinnedVfFname = FName(TEXT("FGPUSkinPassthroughVertexFactory"), FNAME_Find);
	static FName InstancedVfFname = FName(TEXT("FInstancedStaticMeshVertexFactory"), FNAME_Find);
	static FName NiagaraSpriteVfFname = FName(TEXT("FNiagaraSpriteVertexFactory"), FNAME_Find);
	static FName GeometryCacheVfFname = FName(TEXT("FGeometryCacheVertexVertexFactory"), FNAME_Find);

	return (VertexFactoryType == FindVertexFactoryType(LocalVfFname)
		|| VertexFactoryType == FindVertexFactoryType(LSkinnedVfFname)
		|| VertexFactoryType == FindVertexFactoryType(NiagaraSpriteVfFname)
		|| VertexFactoryType == FindVertexFactoryType(GeometryCacheVfFname));
}

class FMaterialCHS : public FMeshMaterialShader, public FUniformLightMapPolicyShaderParametersType
{
public:
	FMaterialCHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
		FUniformLightMapPolicyShaderParametersType::Bind(Initializer.ParameterMap);
	}

	FMaterialCHS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		FUniformLightMapPolicyShaderParametersType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const TBasePassShaderElementData<FUniformLightMapPolicy>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		
		FUniformLightMapPolicy::GetPixelShaderBindings(
			PrimitiveSceneProxy,
			ShaderElementData.LightMapPolicyElementData,
			this,
			ShaderBindings);
	}

	void GetElementShaderBindings(
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		bool bShaderRequiresPositionOnlyStream,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch, 
		const FMeshBatchElement& BatchElement,
		const TBasePassShaderElementData<FUniformLightMapPolicy>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		FMeshMaterialShader::GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, bShaderRequiresPositionOnlyStream, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
	}
};

template<typename LightMapPolicyType, bool UseAnyHitShader, bool UseRayConeTextureLod>
class TMaterialCHS : public FMaterialCHS
{
	DECLARE_SHADER_TYPE(TMaterialCHS, MeshMaterial);
public:

	TMaterialCHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMaterialCHS(Initializer)
	{}

	TMaterialCHS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// #dxr_todo: this should also check if ray tracing is enabled for the target platform & project
		return IsSupportedVertexFactoryType(VertexFactoryType)
			&& (Material->IsMasked() == UseAnyHitShader)
			&& LightMapPolicyType::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& ShouldCompileRayTracingShadersForProject(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("USE_RAYTRACED_TEXTURE_RAYCONE_LOD"), UseRayConeTextureLod ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const TArray<FMaterial*>& Materials, const FVertexFactoryType* VertexFactoryType, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		if (ParameterMap.ContainsParameterAllocation(FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName()))
		{
			OutError.Add(TEXT("Ray tracing closest hit shaders cannot read from the SceneTexturesStruct."));
			return false;
		}

		return true;
	}
};


#define IMPLEMENT_MATERIALCHS_TYPE(LightMapPolicyType, LightMapPolicyName, AnyHitShaderName) \
	typedef TMaterialCHS<LightMapPolicyType, false, false> TMaterialCHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS##LightMapPolicyName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, true, false> TMaterialCHS##LightMapPolicyName##AnyHitShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS##LightMapPolicyName##AnyHitShaderName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS anyhit=MaterialAHS"), SF_RayHitGroup) \
	typedef TMaterialCHS<LightMapPolicyType, false, true> TMaterialCHSLod##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHSLod##LightMapPolicyName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, true, true> TMaterialCHSLod##LightMapPolicyName##AnyHitShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHSLod##LightMapPolicyName##AnyHitShaderName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS anyhit=MaterialAHS"), SF_RayHitGroup);

IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>, FPrecomputedVolumetricLightmapLightingPolicy, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>, TLightMapPolicyHQ, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyHQ, FAnyHitShader);

IMPLEMENT_GLOBAL_SHADER(FHiddenMaterialHitGroup, "/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf", "closesthit=HiddenMaterialCHS anyhit=HiddenMaterialAHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FOpaqueShadowHitGroup, "/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf", "closesthit=OpaqueShadowCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FDefaultMaterialMS, "/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf", "DefaultMaterialMS", SF_RayMiss);

template<typename LightMapPolicyType>
static FMaterialCHS* GetMaterialHitShader(const FMaterial& RESTRICT MaterialResource, const FVertexFactory* VertexFactory, bool UseTextureLod)
{
	if (MaterialResource.IsMasked())
	{
		if(UseTextureLod)
		{ 
			return MaterialResource.GetShader<TMaterialCHS<LightMapPolicyType, true, true>>(VertexFactory->GetType());
		}
		else
		{
			return MaterialResource.GetShader<TMaterialCHS<LightMapPolicyType, true, false>>(VertexFactory->GetType());
		}
	}
	else
	{
		if (UseTextureLod)
		{
			return MaterialResource.GetShader<TMaterialCHS<LightMapPolicyType, false, true>>(VertexFactory->GetType());
		}
		else
		{
			return MaterialResource.GetShader<TMaterialCHS<LightMapPolicyType, false, false>>(VertexFactory->GetType());
		}
	}
}

template<typename PassShadersType, typename ShaderElementDataType>
void FRayTracingMeshProcessor::BuildRayTracingMeshCommands(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
	PassShadersType PassShaders,
	const ShaderElementDataType& ShaderElementData)
{
	const FVertexFactory* RESTRICT VertexFactory = MeshBatch.VertexFactory;

	checkf(MaterialRenderProxy.ImmutableSamplerState.ImmutableSamplers[0] == nullptr, TEXT("Immutable samplers not yet supported in Mesh Draw Command pipeline"));

	FRayTracingMeshCommand SharedCommand;

	SharedCommand.SetShaders(PassShaders.GetUntypedShaders());
	SharedCommand.InstanceMask = ComputeBlendModeMask(MaterialResource.GetBlendMode());
	SharedCommand.bCastRayTracedShadows = MeshBatch.CastRayTracedShadow;
	SharedCommand.bOpaque = MaterialResource.GetBlendMode() == EBlendMode::BLEND_Opaque;

	FVertexInputStreamArray VertexStreams;
	VertexFactory->GetStreams(ERHIFeatureLevel::SM5, VertexStreams);

	if (PassShaders.RayHitGroupShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedCommand.ShaderBindings.GetSingleShaderBindings(SF_RayHitGroup);
		PassShaders.RayHitGroupShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	const int32 NumElements = MeshBatch.Elements.Num();

	for (int32 BatchElementIndex = 0; BatchElementIndex < NumElements; BatchElementIndex++)
	{
		if ((1ull << BatchElementIndex) & BatchElementMask)
		{
			const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];
			FRayTracingMeshCommand& RayTracingMeshCommand = CommandContext->AddCommand(SharedCommand);

			if (PassShaders.RayHitGroupShader)
			{
				FMeshDrawSingleShaderBindings RayHitGroupShaderBindings = RayTracingMeshCommand.ShaderBindings.GetSingleShaderBindings(SF_RayHitGroup);
				PassShaders.RayHitGroupShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, false, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, RayHitGroupShaderBindings, VertexStreams);
			}

			int32 GeometrySegmentIndex = MeshBatch.SegmentIndex + BatchElementIndex;
			RayTracingMeshCommand.GeometrySegmentIndex = (GeometrySegmentIndex < UINT8_MAX) ? uint8(GeometrySegmentIndex) : UINT8_MAX;

			CommandContext->FinalizeCommand(RayTracingMeshCommand);
		}
	}
}

void FRayTracingMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	EMaterialShadingModel ShadingModel,
	const FUniformLightMapPolicy& RESTRICT LightMapPolicy,
	const typename FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMaterialCHS> RayTracingShaders;

	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
		RayTracingShaders.RayHitGroupShader = GetMaterialHitShader<TUniformLightMapPolicy<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>>(MaterialResource, VertexFactory, CVarRayTracingTextureLodCvar.GetValueOnRenderThread());
		break;
	case LMP_LQ_LIGHTMAP:
		RayTracingShaders.RayHitGroupShader = GetMaterialHitShader<TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>>(MaterialResource, VertexFactory, CVarRayTracingTextureLodCvar.GetValueOnRenderThread());
		break;
	case LMP_HQ_LIGHTMAP:
		RayTracingShaders.RayHitGroupShader = GetMaterialHitShader<TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>>(MaterialResource, VertexFactory, CVarRayTracingTextureLodCvar.GetValueOnRenderThread());
		break;
	case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
		RayTracingShaders.RayHitGroupShader = GetMaterialHitShader<TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>>(MaterialResource, VertexFactory, CVarRayTracingTextureLodCvar.GetValueOnRenderThread());
		break;
	case LMP_NO_LIGHTMAP:
		RayTracingShaders.RayHitGroupShader = GetMaterialHitShader<TUniformLightMapPolicy<LMP_NO_LIGHTMAP>>(MaterialResource, VertexFactory, CVarRayTracingTextureLodCvar.GetValueOnRenderThread());
		break;
	default:
		check(false);
	}

	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.OpaqueBasePassUniformBuffer);
	PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	TBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(LightMapElementData);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, -1, true);

	BuildRayTracingMeshCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		RayTracingShaders,
		ShaderElementData);
}


void FRayTracingMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy)
{
	// Caveat: there are also branches not emitting any MDC
	if (MeshBatch.bUseForMaterial && IsSupportedVertexFactoryType(MeshBatch.VertexFactory->GetType()))
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material.GetShadingModel();
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

		// Only draw opaque materials.
		if ((!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
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
							Process(
								MeshBatch,
								BatchElementMask,
								PrimitiveSceneProxy,
								MaterialRenderProxy,
								Material,
								ShadingModel,
								FUniformLightMapPolicy(LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP),
								MeshBatch.LCI);
						}
						else
						{
							Process(
								MeshBatch,
								BatchElementMask,
								PrimitiveSceneProxy,
								MaterialRenderProxy,
								Material,
								ShadingModel,
								FUniformLightMapPolicy(LMP_HQ_LIGHTMAP),
								MeshBatch.LCI);
						}
					}
					else if (bAllowLowQualityLightMaps)
					{
						Process(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							ShadingModel,
							FUniformLightMapPolicy(LMP_LQ_LIGHTMAP),
							MeshBatch.LCI);
					}
					else
					{
						Process(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							ShadingModel,
							FUniformLightMapPolicy(LMP_NO_LIGHTMAP),
							MeshBatch.LCI);
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
						Process(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							ShadingModel,
							FUniformLightMapPolicy(LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING),
							MeshBatch.LCI);
					}
					else
					{
						Process(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							ShadingModel,
							FUniformLightMapPolicy(LMP_NO_LIGHTMAP),
							MeshBatch.LCI);
					}
					break;
				};
			}
		}
	}
}

FRHIRayTracingPipelineState* FDeferredShadingSceneRenderer::BindRayTracingMaterialPipeline(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const TArrayView<const FRayTracingShaderRHIParamRef>& RayGenShaderTable,
	FRayTracingShaderRHIParamRef MissShader,
	FRayTracingShaderRHIParamRef DefaultClosestHitShader
)
{
	SCOPE_CYCLE_COUNTER(STAT_BindRayTracingPipeline);

	FRHIRayTracingPipelineState* PipelineState = nullptr;

	FRayTracingPipelineStateInitializer Initializer;

	Initializer.MaxPayloadSizeInBytes = 52; // sizeof(FPackedMaterialClosestHitPayload)
	Initializer.bAllowHitGroupIndexing = true;

	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	FRayTracingShaderRHIParamRef MissShaderTable[] = { MissShader };
	Initializer.SetMissShaderTable(MissShaderTable);

	const bool bEnableMaterials = GEnableRayTracingMaterials;

	TArray<FRayTracingShaderRHIParamRef> RayTracingMaterialLibrary;

	if (bEnableMaterials)
	{
		FShaderResource::GetRayTracingMaterialLibrary(RayTracingMaterialLibrary, DefaultClosestHitShader);
	}
	else
	{
		RayTracingMaterialLibrary.Add(DefaultClosestHitShader);
	}

	int32 OpaqueShadowMaterialIndex = RayTracingMaterialLibrary.Add(View.ShaderMap->GetShader<FOpaqueShadowHitGroup>()->GetRayTracingShader());
	int32 HiddenMaterialIndex = RayTracingMaterialLibrary.Add(View.ShaderMap->GetShader<FHiddenMaterialHitGroup>()->GetRayTracingShader());

	Initializer.SetHitGroupTable(RayTracingMaterialLibrary);

	PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer);

	const FViewInfo& ReferenceView = Views[0];

	static auto CVarEnableShadowMaterials = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows.EnableMaterials"));
	bool bEnableShadowMaterials = CVarEnableShadowMaterials ? CVarEnableShadowMaterials->GetInt() != 0 : true;

	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : ReferenceView.VisibleRayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		const uint32 HitGroupIndex = bEnableMaterials
			? MeshCommand.MaterialShaderIndex
			: 0; // Force the same shader to be used on all geometry

		// Bind primary material shader

		MeshCommand.ShaderBindings.SetRayTracingShaderBindingsForHitGroup(RHICmdList,
			View.RayTracingScene.RayTracingSceneRHI,
			VisibleMeshCommand.InstanceIndex,
			MeshCommand.GeometrySegmentIndex,
			PipelineState,
			HitGroupIndex,
			RAY_TRACING_SHADER_SLOT_MATERIAL);

		// Bind shadow shader

		if (MeshCommand.bCastRayTracedShadows)
		{
			if (MeshCommand.bOpaque || !bEnableShadowMaterials)
			{
				// Fully opaque surfaces don't need the full material, so we bind a specialized shader that simply updates HitT.
				RHICmdList.SetRayTracingHitGroup(View.RayTracingScene.RayTracingSceneRHI,
					VisibleMeshCommand.InstanceIndex,
					MeshCommand.GeometrySegmentIndex,
					RAY_TRACING_SHADER_SLOT_SHADOW,
					PipelineState, OpaqueShadowMaterialIndex,
					0, nullptr, 0);
			}
			else
			{
				// Masked materials require full material evaluation with any-hit shader.
				// #dxr_todo: we need to generate a shadow-specific closest hit shader for this!
				MeshCommand.ShaderBindings.SetRayTracingShaderBindingsForHitGroup(RHICmdList,
					View.RayTracingScene.RayTracingSceneRHI,
					VisibleMeshCommand.InstanceIndex,
					MeshCommand.GeometrySegmentIndex,
					PipelineState,
					HitGroupIndex,
					RAY_TRACING_SHADER_SLOT_SHADOW);
			}
		}
		else
		{
			RHICmdList.SetRayTracingHitGroup(View.RayTracingScene.RayTracingSceneRHI,
				VisibleMeshCommand.InstanceIndex,
				MeshCommand.GeometrySegmentIndex,
				RAY_TRACING_SHADER_SLOT_SHADOW,
				PipelineState, HiddenMaterialIndex,
				0, nullptr, 0);
		}
	}

	return PipelineState;
}

#endif // RHI_RAYTRACING
