// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12RHICommon.h: Common D3D12 RHI definitions for Windows.
=============================================================================*/

#pragma once

DECLARE_STATS_GROUP(TEXT("D3D12RHI"), STATGROUP_D3D12RHI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("D3D12RHI: Pipeline State (PSO)"), STATGROUP_D3D12PipelineState, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("D3D12RHI: Descriptor Heap (GPU visible)"), STATGROUP_D3D12DescriptorHeap, STATCAT_Advanced);

#include "Windows/WindowsHWrapper.h"
#include "D3D12RHI.h"

class FD3D12Adapter;
class FD3D12Device;

// Defines a unique command queue type within a FD3D12Device (owner by the command list managers).
enum class ED3D12CommandQueueType
{
	Default,
	Copy,
	Async
};

class FD3D12AdapterChild
{
protected:
	FD3D12Adapter* ParentAdapter;

public:
	FD3D12AdapterChild(FD3D12Adapter* InParent = nullptr) : ParentAdapter(InParent) {}

	FORCEINLINE FD3D12Adapter* GetParentAdapter() const
	{
		// If this fires an object was likely created with a default constructor i.e in an STL container
		// and is therefore an orphan
		check(ParentAdapter != nullptr);
		return ParentAdapter;
	}

	// To be used with delayed setup
	inline void SetParentAdapter(FD3D12Adapter* InParent)
	{
		check(ParentAdapter == nullptr);
		ParentAdapter = InParent;
	}
};

class FD3D12DeviceChild
{
protected:
	FD3D12Device* Parent;

public:
	FD3D12DeviceChild(FD3D12Device* InParent = nullptr) : Parent(InParent) {}

	FORCEINLINE FD3D12Device* GetParentDevice() const
	{
		// If this fires an object was likely created with a default constructor i.e in an STL container
		// and is therefore an orphan
		check(Parent != nullptr);
		return Parent;
	}

	// To be used with delayed setup
	inline void SetParentDevice(FD3D12Device* InParent)
	{
		check(Parent == nullptr);
		Parent = InParent;
	}
};

class FD3D12GPUObject
{
public:
	FD3D12GPUObject(FRHIGPUMask InGPUMask, FRHIGPUMask InVisibiltyMask)
		: GPUMask(InGPUMask)
		, VisibilityMask(InVisibiltyMask)
	{
		// Note that node mask can't be null.
	}

	FORCEINLINE const FRHIGPUMask& GetGPUMask() const { return GPUMask; }
	FORCEINLINE const FRHIGPUMask& GetVisibilityMask() const { return VisibilityMask; }

protected:
	const FRHIGPUMask GPUMask;
	// Which GPUs have direct access to this object
	const FRHIGPUMask VisibilityMask;
};

class FD3D12SingleNodeGPUObject : public FD3D12GPUObject
{
public:
	FD3D12SingleNodeGPUObject(FRHIGPUMask GPUMask)
		: FD3D12GPUObject(GPUMask, GPUMask)
		, GPUIndex(GPUMask.ToIndex())
	{}

	FORCEINLINE const uint32 GetGPUIndex() const
	{
		return GPUIndex;
	}

private:
	const uint32 GPUIndex;
};

class FD3D12MultiNodeGPUObject : public FD3D12GPUObject
{
public:
	FD3D12MultiNodeGPUObject(FRHIGPUMask NodeMask, FRHIGPUMask VisibiltyMask)
		: FD3D12GPUObject(NodeMask, VisibiltyMask)
	{
		check(NodeMask.Intersects(VisibiltyMask));// A GPU objects must be visible on the device it belongs to
	}
};

template<typename ObjectType>
class FD3D12LinkedAdapterObject
{
public:
	FD3D12LinkedAdapterObject() : bIsHeadLink(true) {};

	FORCEINLINE void SetNextObject(ObjectType* Object)
	{
		NextNode = Object;
		if (Object)
		{
			Object->bIsHeadLink = false; 
		}
	}

	FORCEINLINE bool IsHeadLink() const
	{
		return bIsHeadLink;
	}

	FORCEINLINE void SetIsHeadLink(bool InIsHeadLink)
	{
		bIsHeadLink = InIsHeadLink;
	}

	FORCEINLINE ObjectType* GetNextObject()
	{
		return NextNode.GetReference();
	}

private:

	TRefCountPtr<ObjectType> NextNode;
	// True if this is the first object in the linked list.
	bool bIsHeadLink;
};