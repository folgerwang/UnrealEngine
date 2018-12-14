// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderTargetPool.cpp: Scene render target pool manager.
=============================================================================*/

#include "RenderGraphResourcePool.h"
#include "RenderGraphResources.h"


uint32 FPooledRDGBuffer::Release()
{
	return RefCount--;

	if (RefCount == 0)
	{
		VertexBuffer.SafeRelease();
		IndexBuffer.SafeRelease();
		StructuredBuffer.SafeRelease();
		UAVs.Empty();
		SRVs.Empty();
	}

	return RefCount;
}


FRenderGraphResourcePool::FRenderGraphResourcePool()
{ }


void FRenderGraphResourcePool::FindFreeBuffer(
	FRHICommandList& RHICmdList,
	const FRDGBufferDesc& Desc,
	TRefCountPtr<FPooledRDGBuffer>& Out,
	const TCHAR* InDebugName)
{
	// First find if available.
	for (auto& PooledBuffer : AllocatedBuffers)
	{
		// Still being used outside the pool.
		if (PooledBuffer->GetRefCount() > 1)
		{
			continue;
		}

		if (PooledBuffer->Desc == Desc)
		{
			Out = PooledBuffer;
			// TODO(RDG): assign name on RHI.
			return;
		}
	}

	// Allocate new one
	{
		Out = new FPooledRDGBuffer;
		AllocatedBuffers.Add(Out);
		check(Out->GetRefCount() == 2);

		Out->Desc = Desc;

		uint32 NumBytes = Desc.GetTotalNumBytes();

		FRHIResourceCreateInfo CreateInfo;
		CreateInfo.DebugName = InDebugName;

		if (Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::VertexBuffer)
		{
			Out->VertexBuffer = RHICreateVertexBuffer(NumBytes, Desc.Usage, CreateInfo);
		}
		else if (Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::StructuredBuffer)
		{
			Out->StructuredBuffer = RHICreateStructuredBuffer(Desc.BytesPerElement, NumBytes, Desc.Usage, CreateInfo);
		}
		else
		{
			check(0);
		}
	}
}

void FRenderGraphResourcePool::ReleaseDynamicRHI()
{
	AllocatedBuffers.Empty();
}

void FRenderGraphResourcePool::FreeUnusedResources()
{
	for (auto& PooledBuffer : AllocatedBuffers)
	{
		if (PooledBuffer.GetRefCount() == 1)
		{
			PooledBuffer = nullptr;
		}
	}
}

TGlobalResource<FRenderGraphResourcePool> GRenderGraphResourcePool;
