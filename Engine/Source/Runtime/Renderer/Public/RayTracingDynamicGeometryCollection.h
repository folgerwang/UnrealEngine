// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if RHI_RAYTRACING

class RENDERER_API FRayTracingDynamicGeometryCollection
{
public:
	FRayTracingDynamicGeometryCollection();

	void AddDynamicMeshBatchForGeometryUpdate(const FScene* Scene, const FSceneView* View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& MeshBatch, FRayTracingGeometry& Geometry, uint32 NumMaxVertices, FRWBuffer& Buffer);
	void DispatchUpdates(FRHICommandListImmediate& RHICmdList);
	void Clear();

private:
	TUniquePtr<TArray<struct FMeshComputeDispatchCommand>> DispatchCommands;
};

#endif // RHI_RAYTRACING
