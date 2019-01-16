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

extern FMeshDrawCommandSortKey CalculateStaticTranslucentMeshSortKey(const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, uint16 MeshIdInPrimitive);
EMeshPass::Type TranslucencyPassToMeshPass(ETranslucencyPass::Type TranslucencyPass);

/**
* Translucent mesh sort key format.
*/
union FTranslucentMeshSortKey
{
	uint64 PackedData;

	struct
	{
		uint64 MeshIdInPrimitive	: 16; // Order meshes belonging to the same primitive by a stable id.
		uint64 Distance				: 32; // Order by distance.
		uint64 Priority				: 16; // First order by priority.
	} Fields;
};
