// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
DynamicBufferAllocator.h: Classes for allocating transient rendering data.
==============================================================================*/

#pragma once

#include "RenderResource.h"

struct FDynamicReadBufferPool;

struct FDynamicAllocReadBuffer : public FDynamicReadBuffer
{
	FDynamicAllocReadBuffer()
		: FDynamicReadBuffer()
		, AllocatedByteCount(0)
	{}

	int32 AllocatedByteCount;

	/**
	* Unocks the buffer so the GPU may read from it.
	*/
	void Unlock()
	{
		FDynamicReadBuffer::Unlock();
		AllocatedByteCount = 0;
	}
};

/**
* A system for dynamically allocating GPU memory for rendering. Note that this must derive from FRenderResource 
  so that we can safely free the shader resource views for OpenGL and other platforms. If we wait until the module is shutdown,
  the renderer RHI will have already been destroyed and we can execute code on invalid data. By making ourself a render resource, we
  clean up immediately before the renderer dies.
*/
class RENDERCORE_API FGlobalDynamicReadBuffer : public FRenderResource
{
public:
	/**
	* Information regarding an allocation from this buffer.
	*/
	struct FAllocation
	{
		/** The location of the buffer in main memory. */
		uint8* Buffer;
		/** The read buffer to bind for draw calls. */
		FDynamicAllocReadBuffer* ReadBuffer;
		/** The offset in to the read buffer. */
		uint32 FirstIndex;

		/** Default constructor. */
		FAllocation()
			: Buffer(NULL)
			, ReadBuffer(NULL)
			, FirstIndex(0)
		{
		}

		/** Returns true if the allocation is valid. */
		FORCEINLINE bool IsValid() const
		{
			return Buffer != NULL;
		}
	};

	FGlobalDynamicReadBuffer();
	~FGlobalDynamicReadBuffer();
	
	FAllocation AllocateFloat(uint32 Num);
	FAllocation AllocateInt32(uint32 Num);

	/**
	* Commits allocated memory to the GPU.
	*		WARNING: Once this buffer has been committed to the GPU, allocations
	*		remain valid only until the next call to Allocate!
	*/
	void Commit();


	/** Returns true if log statements should be made because we exceeded GMaxVertexBytesAllocatedPerFrame */
	bool IsRenderAlarmLoggingEnabled() const;

protected:
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	void Cleanup();

	/** The pools of read buffers from which allocations are made. */
	FDynamicReadBufferPool* FloatBufferPool;
	FDynamicReadBufferPool* Int32BufferPool;

	/** A total of all allocations made since the last commit. Used to alert about spikes in memory usage. */
	size_t TotalAllocatedSinceLastCommit;
};

