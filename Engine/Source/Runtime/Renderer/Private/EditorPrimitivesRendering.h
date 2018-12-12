// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorPrimitivesRendering
=============================================================================*/
#include "MeshPassProcessor.h"
#include "DrawingPolicy.h"

class FEditorPrimitivesBasePassMeshProcessor : public FMeshPassProcessor
{
public:
	FEditorPrimitivesBasePassMeshProcessor(const FScene* InScene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FDrawingPolicyRenderState& InDrawRenderState, bool bTranslucentBasePass, FMeshPassDrawListContext& InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 MeshId = -1) override final;

	const FDrawingPolicyRenderState PassDrawRenderState;
	const bool bTranslucentBasePass;

private:
	void ProcessDeferredShadingPath(const FMeshBatch& MeshBatch, uint64 BatchElementMask, const FMaterial& Material, const FMaterialRenderProxy& MaterialRenderProxy, const FPrimitiveSceneProxy* PrimitiveSceneProxy, int32 MeshId);
	void ProcessMobileShadingPath(const FMeshBatch& MeshBatch, uint64 BatchElementMask, const FMaterial& Material, const FMaterialRenderProxy& MaterialRenderProxy, const FPrimitiveSceneProxy* PrimitiveSceneProxy, int32 MeshId);
};