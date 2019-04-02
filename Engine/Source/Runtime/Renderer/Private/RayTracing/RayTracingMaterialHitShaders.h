// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MeshMaterialShader.h"
#include "LightMapRendering.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"

#if RHI_RAYTRACING

class FRayTracingMeshProcessor
{
public:

	FRayTracingMeshProcessor(FRayTracingMeshCommandContext* InCommandContext, const FScene* InScene, const FSceneView* InViewIfDynamicMeshCommand)
		:
		CommandContext(InCommandContext),
		Scene(InScene),
		ViewIfDynamicMeshCommand(InViewIfDynamicMeshCommand),
		FeatureLevel(InScene->GetFeatureLevel())
	{}

	void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy);

private:
	FRayTracingMeshCommandContext* CommandContext;
	const FScene* Scene;
	const FSceneView* ViewIfDynamicMeshCommand;
	ERHIFeatureLevel::Type FeatureLevel;

	void Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		EMaterialShadingModel ShadingModel,
		const FUniformLightMapPolicy& RESTRICT LightMapPolicy,
		const typename FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData);

	template<typename PassShadersType, typename ShaderElementDataType>
	void BuildRayTracingMeshCommands(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
		PassShadersType PassShaders,
		const ShaderElementDataType& ShaderElementData);
};

class FHiddenMaterialHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHiddenMaterialHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FHiddenMaterialHitGroup, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

class FOpaqueShadowHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOpaqueShadowHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FOpaqueShadowHitGroup, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

class FDefaultMaterialMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDefaultMaterialMS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FDefaultMaterialMS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

#endif // RHI_RAYTRACING
