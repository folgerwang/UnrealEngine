// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if RHI_RAYTRACING

class RENDERER_API FRayTracingDynamicGeometryCollection
{
public:
	void AddDynamicMeshBatchForGeometryUpdate(const FScene* Scene, const FSceneView* View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& MeshBatch, FRayTracingGeometry& Geometry, uint32 NumMaxVertices, FRWBuffer& Buffer);
	void DispatchUpdates(FRHICommandListImmediate& RHICmdList);
	void Clear();

private:
	struct FMeshComputeDispatchCommand
	{
		FMeshDrawShaderBindings ShaderBindings;
		class FRayTracingDynamicGeometryConverterCS* MaterialShader;

		uint32 NumMaxVertices;
		uint32 NumCPUVertices;
		FRWBuffer* TargetBuffer;
		FRayTracingGeometry* TargetGeometry;
	};

	TArray<FMeshComputeDispatchCommand> DispatchCommands;
};

#endif // RHI_RAYTRACING
