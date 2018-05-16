// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
  RHIGPUReadback.h: classes for managing fences and staging buffers for
  asynchronous GPU memory updates and readbacks with minimal stalls and no
  RHI thread flushes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"


/* FRHIGPUMemoryUpdate: represents a memory update request scheduled with RHIScheduleGPUMemoryUpdate
 *
 */
class FRHIGPUMemoryUpdate
{
public:
	FRHIGPUMemoryUpdate(FRHIStagingBuffer *Staging, FVertexBufferRHIRef InBuffer, FName RequestName)
	{
		GPUVertexBuffer = InBuffer;
		StagingBuffer = Staging;
		Fence = RHICreateGPUFence(RequestName);
		Fence->Write();
	}

	FRHIGPUMemoryUpdate(FVertexBufferRHIRef InBuffer, FName RequestName)
	{
		GPUVertexBuffer = InBuffer;
		StagingBuffer = RHICreateStagingBuffer();
		Fence = RHICreateGPUFence(RequestName);
		Fence->Write();
	}

	bool WaitForFence(float Timeout) 
	{ 
		return Fence->Wait(Timeout); 
	}
	bool IsReady() 
	{ 
		return Fence->Poll(); 
	}

	void UpdateBuffer(void *InPtr, int32 NumBytes) 
	{ 
		check(Fence->Poll());  
	}
	
	void Finish() 
	{ 
	}

private:

	FRHIStagingBuffer *StagingBuffer;
	FVertexBufferRHIRef  GPUVertexBuffer;
	TRefCountPtr<FRHIGPUFence> Fence;
};



/* FRHIGPUMemoryReadback: represents a memory readback request scheduled with RHIScheduleGPUMemoryUpdate
*
*/
class FRHIGPUMemoryReadback
{
public:

	FRHIGPUMemoryReadback(FStagingBufferRHIRef Staging, FVertexBufferRHIRef InBuffer, FName RequestName)
	{
		GPUVertexBuffer = InBuffer;
		StagingBuffer = Staging;
		Fence = RHICreateGPUFence(RequestName);
		Fence->Write();
	}

	FRHIGPUMemoryReadback(FVertexBufferRHIRef InBuffer, FName RequestName)
	{
		GPUVertexBuffer = InBuffer;
		StagingBuffer = RHICreateStagingBuffer();
		Fence = RHICreateGPUFence(RequestName);
		Fence->Write();
	}

	~FRHIGPUMemoryReadback()
	{
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
		return StagingBuffer->Lock(GPUVertexBuffer, 0, NumBytes, EResourceLockMode::RLM_ReadOnly);
	}

	void Finish()
	{
		StagingBuffer->Unlock();
	}

private:

	FStagingBufferRHIRef StagingBuffer;
	FVertexBufferRHIRef  GPUVertexBuffer;
	FGPUFenceRHIRef Fence;
};




