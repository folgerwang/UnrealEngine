// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Serialization/LargeMemoryData.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"

/*----------------------------------------------------------------------------
	FLargeMemoryData
----------------------------------------------------------------------------*/

FLargeMemoryData::FLargeMemoryData(const int64 PreAllocateBytes)
	: Data(nullptr)
	, NumBytes(FMath::Max<int64>(0, PreAllocateBytes))
	, MaxBytes(0)
{
	GrowBuffer();
}

FLargeMemoryData::~FLargeMemoryData()
{
	if (Data)
	{
		FMemory::Free(Data);
	}
}

bool FLargeMemoryData::Write(void* InData, int64 InOffset, int64 InNum)
{
	// Allow InData to be null if InNum == 0.
	if (InOffset >= 0 && (InData ? InNum >= 0 : InNum == 0))
	{
		// Grow the buffer to the offset position even if InNum == 0.
		NumBytes = FMath::Max<int64>(NumBytes, InOffset + InNum);
		if (NumBytes > MaxBytes)
		{
			GrowBuffer();
		}

		if (InNum)
		{
			FMemory::Memcpy(&Data[InOffset], InData, InNum);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool FLargeMemoryData::Read(void* OutData, int64 InOffset, int64 InNum) const
{
	// Allow OutData to be null if InNum == 0.
	if (InOffset >= 0 && (OutData ? InNum >= 0 : InNum == 0) && (InOffset + InNum <= NumBytes))
	{
		if (InNum)
		{
			FMemory::Memcpy(OutData, &Data[InOffset], InNum);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FLargeMemoryData::ReleaseOwnership()
{
	Data = nullptr;
	NumBytes = 0;
	MaxBytes = 0;
}

void FLargeMemoryData::GrowBuffer()
{
	// Allocate slack proportional to the buffer size. Min 64 KB
	MaxBytes = FMath::Max<int64>(64 * 1024, FMemory::QuantizeSize(NumBytes + 3 * NumBytes / 8 + 16));

	if (Data)
	{
		Data = (uint8*)FMemory::Realloc(Data, MaxBytes);
	}
	else
	{
		Data = (uint8*)FMemory::Malloc(MaxBytes);
	}
}
