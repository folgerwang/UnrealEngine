// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorPrimitivesRendering
=============================================================================*/

#include "EditorPrimitivesRendering.h"
#include "BasePassRendering.h"
#include "ScenePrivate.h"
#include "MobileBasePassRendering.h"
#include "MeshPassProcessor.inl"

FEditorPrimitivesBasePassMeshProcessor::FEditorPrimitivesBasePassMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InDrawRenderState, bool bInTranslucentBasePass, FMeshPassDrawListContext* InDrawListContext) 
	: FMeshPassProcessor(Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InDrawRenderState)
	, bTranslucentBasePass(bInTranslucentBasePass)
{}

void FEditorPrimitivesBasePassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

		if (bIsTranslucent == bTranslucentBasePass
			&& (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
		{
			if (Scene->GetShadingPath(FeatureLevel) == EShadingPath::Mobile)
			{
				ProcessMobileShadingPath(MeshBatch, BatchElementMask, Material, MaterialRenderProxy, PrimitiveSceneProxy, StaticMeshId);
			}
			else
			{
				ProcessDeferredShadingPath(MeshBatch, BatchElementMask, Material, MaterialRenderProxy, PrimitiveSceneProxy, StaticMeshId);
			}
		}
	}
}

void FEditorPrimitivesBasePassMeshProcessor::ProcessDeferredShadingPath(const FMeshBatch& MeshBatch, uint64 BatchElementMask, const FMaterial& Material, const FMaterialRenderProxy& MaterialRenderProxy, const FPrimitiveSceneProxy* PrimitiveSceneProxy, int32 StaticMeshId)
{
	FUniformLightMapPolicy NoLightmapPolicy(LMP_NO_LIGHTMAP);
	typedef FUniformLightMapPolicy LightMapPolicyType;

	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
	const bool bRenderSkylight = false;
	const bool bRenderAtmosphericFog = false;

	TMeshProcessorShaders<
		TBasePassVertexShaderPolicyParamType<LightMapPolicyType>,
		FBaseHS,
		FBaseDS,
		TBasePassPixelShaderPolicyParamType<LightMapPolicyType>> BasePassShaders;

	GetBasePassShaders<LightMapPolicyType>(
		Material, 
		VertexFactory->GetType(), 
		NoLightmapPolicy,
		FeatureLevel,
		bRenderAtmosphericFog,
		bRenderSkylight,
		BasePassShaders.HullShader,
		BasePassShaders.DomainShader,
		BasePassShaders.VertexShader,
		BasePassShaders.PixelShader
		);

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	if (bTranslucentBasePass)
	{
		extern void SetTranslucentRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FMaterial& Material);
		SetTranslucentRenderState(DrawRenderState, Material);
	}

	ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
	ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);
	
	TBasePassShaderElementData<LightMapPolicyType> ShaderElementData(nullptr);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(BasePassShaders.VertexShader, BasePassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		Material,
		DrawRenderState,
		BasePassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

void FEditorPrimitivesBasePassMeshProcessor::ProcessMobileShadingPath(const FMeshBatch& MeshBatch, uint64 BatchElementMask, const FMaterial& Material, const FMaterialRenderProxy& MaterialRenderProxy, const FPrimitiveSceneProxy* PrimitiveSceneProxy, int32 StaticMeshId)
{
	FUniformLightMapPolicy NoLightmapPolicy(LMP_NO_LIGHTMAP);
	typedef FUniformLightMapPolicy LightMapPolicyType;

	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
	const int32 NumMovablePointLights = 0;
	const bool bEnableSkyLight = false;

	TMeshProcessorShaders<
		TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>,
		FBaseHS,
		FBaseDS,
		TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>> BasePassShaders;

	MobileBasePass::GetShaders(
		NoLightmapPolicy.GetIndirectPolicy(), 
		NumMovablePointLights,
		Material, 
		VertexFactory->GetType(), 
		bEnableSkyLight, 
		BasePassShaders.VertexShader, 
		BasePassShaders.PixelShader);
	
	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	if (bTranslucentBasePass)
	{
		MobileBasePass::SetTranslucentRenderState(DrawRenderState, Material);
	}

	ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
	ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);
	
	TMobileBasePassShaderElementData<LightMapPolicyType> ShaderElementData(nullptr);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(BasePassShaders.VertexShader, BasePassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		Material,
		DrawRenderState,
		BasePassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}