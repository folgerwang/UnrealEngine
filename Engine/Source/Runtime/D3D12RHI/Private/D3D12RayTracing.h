// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHIPrivate.h"

#if D3D12_RHI_RAYTRACING

class FD3D12RayTracingPipelineState;
class FD3D12RayTracingShaderTable;

// #dxr_todo: perhaps FD3D12VertexBuffer in the core RHI should be generalized
typedef FD3D12VertexBuffer FD3D12MemBuffer; // Generic GPU memory buffer

class FD3D12RayTracingGeometry : public FRHIRayTracingGeometry, public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<FD3D12RayTracingGeometry>
{
public:

	FD3D12RayTracingGeometry(FD3D12Device* Device) : FD3D12DeviceChild(Device) {}
	void TransitionBuffers(FD3D12CommandContext& CommandContext);
	void BuildAccelerationStructure(FD3D12CommandContext& CommandContext, bool bIsUpdate);

	bool bIsAccelerationStructureDirty = false;

	uint32 IndexStride = 0; // 0 for non-indexed / implicit triangle list, 2 for uint16, 4 for uint32
	uint32 VertexOffsetInBytes = 0;
	uint32 VertexStrideInBytes = 0;
	uint32 BaseVertexIndex = 0;
	uint32 TotalPrimitiveCount = 0; // Combined number of primitives in all mesh segments

	TArray<FRayTracingGeometrySegment> Segments; // Defines addressable parts of the mesh that can be used for material assignment (one segment = one SBT record)
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags;

	EVertexElementType VertexElemType;

	TRefCountPtr<FD3D12IndexBuffer>  IndexBuffer;
	TRefCountPtr<FD3D12VertexBuffer> PositionVertexBuffer;

	TRefCountPtr<FD3D12MemBuffer> AccelerationStructureBuffer;
	TRefCountPtr<FD3D12MemBuffer> ScratchBuffer;

};

class FD3D12RayTracingScene : public FRHIRayTracingScene, public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<FD3D12RayTracingScene>
{
public:

	FD3D12RayTracingScene(FD3D12Device* Device) 
		: FD3D12DeviceChild(Device)
		, AccelerationStructureView(new FD3D12ShaderResourceView(Device))
	{
		ShaderResourceView = AccelerationStructureView;
	};

	~FD3D12RayTracingScene();

	void BuildAccelerationStructure(FD3D12CommandContext& CommandContext, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags);

	// #dxr_todo: only GPU resources should be managed as LinkedAdapterObjects
	TRefCountPtr<FD3D12MemBuffer> AccelerationStructureBuffer;
	TRefCountPtr<FD3D12ShaderResourceView> AccelerationStructureView;
	bool bAccelerationStructureViewInitialized = false;

	// #dxr_todo: add API to update geometries and instances
	TArray<FRayTracingGeometryInstance> Instances;

	// Scene keeps track of child acceleration structures to manage their residency
	TArray<TRefCountPtr<FD3D12MemBuffer>> BottomLevelAccelerationStructureBuffers;
	void UpdateResidency(FD3D12CommandContext& CommandContext);

	uint32 ShaderSlotsPerGeometrySegment = 1;

	// Exclusive prefix sum of instance geometry segments is used to calculate SBT record address from instance and segment indices.
	TArray<uint32> SegmentPrefixSum;
	uint32 NumTotalSegments = 0;
	uint32 GetHitRecordBaseIndex(uint32 InstanceIndex, uint32 SegmentIndex) const { return (SegmentPrefixSum[InstanceIndex] + SegmentIndex) * ShaderSlotsPerGeometrySegment; }

	// #dxr_todo: shader tables should be explicitly registered and unregistered with the scene
	FD3D12RayTracingShaderTable* FindOrCreateShaderTable(const FD3D12RayTracingPipelineState* Pipeline);
	FD3D12RayTracingShaderTable* FindExistingShaderTable(const FD3D12RayTracingPipelineState* Pipeline) const;

	TMap<const FD3D12RayTracingPipelineState*, FD3D12RayTracingShaderTable*> ShaderTables;
};

#endif // D3D12_RHI_RAYTRACING
