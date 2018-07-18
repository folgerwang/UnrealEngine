// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D11Resources.h: D3D resource RHI definitions.
=============================================================================*/

#pragma once

/* D3D11 implementation of staging buffer class
*/
class FD3D11StagingBuffer : public FRHIStagingBuffer
{
public:
	FD3D11StagingBuffer()
	{
	}

	// copyresource, map, return ptr
	virtual void *Lock(FVertexBufferRHIRef GPUBuffer, uint32 Offset, uint32 NumBytes, EResourceLockMode LockMode) override
	{
		check(!LastLockedBuffer);	// attempting to map another buffer to this staging buffer without unlocking the previous first?

		MappedPtr = RHILockVertexBuffer(GPUBuffer, Offset, NumBytes, LockMode);
		LastLockedBuffer = GPUBuffer;
		return MappedPtr;
	}

	// unmap, free memory
	virtual void Unlock() override
	{
		check(LastLockedBuffer);	// attempting to unlock without having locked first?

		RHIUnlockVertexBuffer(LastLockedBuffer);
		MappedPtr = nullptr;
		LastLockedBuffer = nullptr;
	}
};



