// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
  RHIGPUReadback.h: classes for managing fences and staging buffers for
  asynchronous GPU memory updates and readbacks with minimal stalls and no
  RHI thread flushes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

/* FRHIGPUMemoryReadback: represents a memory readback request scheduled with RHIScheduleGPUMemoryUpdate
*
*/
class FRHIGPUMemoryReadback
{
public:

	FRHIGPUMemoryReadback(FVertexBufferRHIRef InBuffer, FName RequestName)
	{
		GPUVertexBuffer = InBuffer;
		StagingBuffer = RHICreateStagingBuffer(InBuffer);
		Fence = RHICreateGPUFence(RequestName);
	}

	~FRHIGPUMemoryReadback()
	{
	}

	void Insert()
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		RHICmdList.EnqueueStagedRead(StagingBuffer, Fence, 0, GPUVertexBuffer->GetSize());
	}

	bool WaitForFence(float Timeout)
	{
		return Fence->Wait(Timeout);
	}

	bool IsReady()
	{
		return Fence->Poll();
	}

	void *RetrieveData(uint32 NumBytes)
	{
		ensure(Fence->Poll());
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		return RHICmdList.LockStagingBuffer(StagingBuffer, 0, NumBytes);
	}

	void Finish()
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		RHICmdList.UnlockStagingBuffer(StagingBuffer);
	}

private:
	FStagingBufferRHIRef StagingBuffer;
	FVertexBufferRHIRef  GPUVertexBuffer;
	FGPUFenceRHIRef Fence;
};




