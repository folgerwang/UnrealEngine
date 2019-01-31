// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VelocityRendering.h: Velocity rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "HitProxies.h"
#include "RendererInterface.h"
#include "DepthRendering.h"

class FPrimitiveSceneInfo;
class FPrimitiveSceneProxy;
class FScene;
class FStaticMeshBatch;
class FViewInfo;

/** Get the cvar clamped state */
int32 GetMotionBlurQualityFromCVar();

/** If this view need motion blur processing */
bool IsMotionBlurEnabled(const FViewInfo& View);

// Group Velocity Rendering accessors, types, etc.
struct FVelocityRendering
{
	static FPooledRenderTargetDesc GetRenderTargetDesc();

	/** Returns true the velocity can be outputted in the BasePass. */
	static bool BasePassCanOutputVelocity(EShaderPlatform ShaderPlatform);

	/** Returns true the velocity can be outputted in the BasePass. Only valid for the current platform. */
	static bool BasePassCanOutputVelocity(ERHIFeatureLevel::Type FeatureLevel);

	/** Returns true the velocity is output in the BasePass. */
	static bool VertexFactoryOnlyOutputsVelocityInBasePass(EShaderPlatform ShaderPlatform, bool bVertexFactorySupportsStaticLighting);

	/** Returns true if the object needs to be rendered in the velocity pass (is not moving like the world, needed for motionblur and TemporalAA). */
	static bool PrimitiveHasVelocity(ERHIFeatureLevel::Type FeatureLevel, const FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Returns true if the object needs to be rendered in the velocity pass for a given view. */
	static bool PrimitiveHasVelocityForView(const FViewInfo& View, const FBoxSphereBounds& Bounds, const FPrimitiveSceneInfo* PrimitiveSceneInfo);
};


class FVelocityMeshProcessor : public FMeshPassProcessor
{
public:

	FVelocityMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;

private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);
};