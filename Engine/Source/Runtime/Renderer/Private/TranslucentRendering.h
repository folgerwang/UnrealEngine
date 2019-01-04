// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentRendering.h: Translucent rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "VolumeRendering.h"

bool UseNearestDepthNeighborUpsampleForSeparateTranslucency(const FSceneRenderTargets& SceneContext);

extern float CalculateTranslucentSortKey(const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo, const FSceneView& View);
extern FMeshDrawCommandSortKey CalculateStaticTranslucentMeshSortKey(const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, uint16 MeshIdInPrimitive);
EMeshPass::Type TranslucencyPassToMeshPass(ETranslucencyPass::Type TranslucencyPass);

/**
* Translucent mesh sort key format.
*/
union UTranslucentMeshSortKey
{
	uint64 PackedData;

	struct
	{
		uint64 MeshIdInPrimitive	: 16; // Order meshes belonging to the same primitive by a stable id.
		uint64 Distance				: 32; // Order by distance.
		uint64 Priority				: 16; // First order by priority.
	} Fields;
};

/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FMobileTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		ETranslucencyPass::Type TranslucencyPass;

		ContextType(ETranslucencyPass::Type InTranslucencyPass)
		: TranslucencyPass(InTranslucencyPass)
		{}
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};