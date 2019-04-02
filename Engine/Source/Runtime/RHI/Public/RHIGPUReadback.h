// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
  RHIGPUReadback.h: classes for managing fences and staging buffers for
  asynchronous GPU memory updates and readbacks with minimal stalls and no
  RHI thread flushes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

/* FRHIGPUMemoryReadback: Represents a memory readback request scheduled with CopyToStagingBuffer
** Wraps a FRHIStagingBuffer with a FRHIGPUFence for synchronization.
*/
class FRHIGPUMemoryReadback
{
public:

	FRHIGPUMemoryReadback(FName RequestName)
	{
		DestinationStagingBuffer = RHICreateStagingBuffer();
		Fence = RHICreateGPUFence(RequestName);
	}

	~FRHIGPUMemoryReadback()
	{
	}

	/**
	 * Returns the CPU accessible pointer that backs this staging buffer.
	 * @param RHICmdList The command list to enqueue the copy request on.
	 * @param SourceBuffer The buffer holding the source data.
	 * @param NumBytes The number of bytes to copy. If 0, this will copy the entire buffer.
	 */

	void EnqueueCopy(FRHICommandList& RHICmdList, FVertexBufferRHIParamRef SourceBuffer, uint32 NumBytes = 0)
	{
		Fence->Clear();
		RHICmdList.CopyToStagingBuffer(SourceBuffer, DestinationStagingBuffer, 0, NumBytes ? NumBytes : SourceBuffer->GetSize(), Fence);
	}

	/**
	 * Indicates if the data is in place and ready to be read.
	 */
	bool IsReady()
	{
		return !Fence || Fence->Poll();
	}

	/**
	 * Returns the CPU accessible pointer that backs this staging buffer.
	 * @param NumBytes The maximum number of bytes the host will read from this pointer.
	 * @returns A CPU accessible pointer to the backing buffer.
	 */
	void* Lock(uint32 NumBytes)
	{
		ensure(Fence->Poll());
		return RHILockStagingBuffer(DestinationStagingBuffer, 0, NumBytes);
	}

	/**
	 * Signals that the host is finished reading from the backing buffer.
	 */
	void Unlock()
	{
		RHIUnlockStagingBuffer(DestinationStagingBuffer);
	}

private:
	FStagingBufferRHIRef DestinationStagingBuffer;
	FGPUFenceRHIRef Fence;
};
