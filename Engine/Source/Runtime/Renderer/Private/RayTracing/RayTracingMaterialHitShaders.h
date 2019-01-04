// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MeshMaterialShader.h"
#include "LightMapRendering.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"

#if RHI_RAYTRACING

class FRayTracingMeshProcessor : public FMeshPassProcessor
{
public:

	FRayTracingMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 MeshId = -1) override final;

private:

	void Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 MeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		EBlendMode BlendMode,
		EMaterialShadingModel ShadingModel,
		const FUniformLightMapPolicy& RESTRICT LightMapPolicy,
		const typename FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);
};

#endif // RHI_RAYTRACING
