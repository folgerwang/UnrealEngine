// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Rendering/SkeletalMeshLODModel.h"

namespace BuildOptimizationThirdParty
{
	MESHBUILDER_API void CacheOptimizeIndexBuffer(TArray<uint16>& Indices);
	MESHBUILDER_API void CacheOptimizeIndexBuffer(TArray<uint32>& Indices);

	/*------------------------------------------------------------------------------
	NVTriStrip for cache optimizing index buffers.
	------------------------------------------------------------------------------*/

	namespace NvTriStripHelper
	{
		/*****************************
		 * Skeletal mesh helpers
		 */
		MESHBUILDER_API void BuildStaticAdjacencyIndexBuffer(
			const FPositionVertexBuffer& PositionVertexBuffer,
			const FStaticMeshVertexBuffer& VertexBuffer,
			const TArray<uint32>& Indices,
			TArray<uint32>& OutPnAenIndices
		);

		MESHBUILDER_API void BuildSkeletalAdjacencyIndexBuffer(
			const TArray<FSoftSkinVertex>& VertexBuffer,
			const uint32 TexCoordCount,
			const TArray<uint32>& Indices,
			TArray<uint32>& OutPnAenIndices
		);
	}
}
