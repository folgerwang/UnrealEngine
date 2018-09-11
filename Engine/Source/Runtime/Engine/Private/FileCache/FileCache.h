// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"

/**
 * Destroy this object to free the associated memory of a request.
 */
class IFileCacheReadBuffer
{
public:
	void *GetData() { return Memory;  }
	size_t GetSize() const { return Size; }
	virtual ~IFileCacheReadBuffer() {};
protected:
	uint8 *Memory;
	size_t Size;
};

class FAllocatedFileCacheReadBuffer : public IFileCacheReadBuffer
{
public:

	FAllocatedFileCacheReadBuffer(const void *Data, size_t NumBytes)
	{
		Memory = (uint8*)FMemory::Malloc(NumBytes);
		FMemory::Memcpy(Memory, Data, NumBytes);
		Size = NumBytes;
	}
	
	FAllocatedFileCacheReadBuffer(size_t NumBytes)
	{
		Memory = (uint8*)FMemory::Malloc(NumBytes);
		Size = NumBytes;
	}

	virtual ~FAllocatedFileCacheReadBuffer()
	{
		FMemory::Free(Memory);
	}
};

/**
 * Thready safety note: Once created a IFileCacheHandle is assumed to be only used from a single thread.
 * (i.e. the IFileCacheHandle interface is not thread safe, and the user will need to ensure serialization).
 * Of course you can create several IFileCacheHandle's on separate threads if needed. And obviously Internally threading
 * will also be used to do async IO and cache management.
 * 
 * Also note, if you create several IFileCacheHandle's to the same file on separate threads these will be considered
 * as individual separate files from the cache point of view and thus each will have their own cache data allocated.
 */
class IFileCacheHandle
{
public:
	static IFileCacheHandle *CreateFileCacheHandle(const FString &FileName);
	virtual ~IFileCacheHandle() {};

	/**
	 * Read a byte range form the file. This can be a high-throughput operation and done lots of times for small reads.
	 * The system will handle this efficiently.
	 * 
	 * If the data is not currently available this function will return nullptr. The user is encouraged to try reading
	 * the byte range again at a later time as the system strives to make data that was tried for a read but not available
	 * resident in the future.
	 */
	virtual IFileCacheReadBuffer *ReadData(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority) = 0;

	/**
	 * Wait until all outstanding read requests complete. 
	 * Note: this does not guarantee that any previous calls to ReadData that returned null will in fact return data now.
	 */
	virtual void WaitAll() = 0;
};