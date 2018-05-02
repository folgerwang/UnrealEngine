// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.h: Multi-GPU support
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"


/** A mask where each bit is a GPU index. Can not be empty so that non SLI platforms can optimize it to be always 1.  */
struct FRHIGPUMask
{
private:

	uint32 Value;
 
public:

	FORCEINLINE explicit FRHIGPUMask(uint32 InValue) : Value(InValue) 
	{
#if WITH_SLI
		check(InValue != 0);
#else
		check(InValue == 1);
#endif
	}

	FORCEINLINE static FRHIGPUMask FromIndex(uint32 GPUIndex) { return FRHIGPUMask(1 << GPUIndex); }

	FORCEINLINE uint32 ToIndex() const
	{
#if WITH_SLI
		check(HasSingleIndex());
		return appCountTrailingZeros(Value);
#else
		return 0;
#endif
	}

	FORCEINLINE bool HasSingleIndex() const
	{ 
#if WITH_SLI
		return FPlatformMath::CountBits(Value) == 1; 
#else
		return true;
#endif
	}

	FORCEINLINE bool Contains(uint32 GPUIndex) const { return (Value & (1 << GPUIndex)) != 0; }
	FORCEINLINE bool Intersects(const FRHIGPUMask& Rhs) const { return (Value & Rhs.Value) != 0; }

	FORCEINLINE bool operator ==(const FRHIGPUMask& Rhs) const { return Value == Rhs.Value; }

	void operator &=(const FRHIGPUMask& Rhs) { Value &= Rhs.Value; }
	void operator |=(const FRHIGPUMask& Rhs) { Value |= Rhs.Value; }

	FORCEINLINE explicit operator uint32() const { return Value; }

	FORCEINLINE FRHIGPUMask operator &(const FRHIGPUMask& Rhs) const 
	{ 
		return FRHIGPUMask(Value & Rhs.Value);
	}

	FORCEINLINE FRHIGPUMask operator |(const FRHIGPUMask& Rhs) const 
	{ 
		return FRHIGPUMask(Value | Rhs.Value);
	}

	// Used to track reference to GPU0
	FORCEINLINE static FRHIGPUMask GPU0() { return FRHIGPUMask(1); }

	FORCEINLINE static FRHIGPUMask All() 
	{ 
#if WITH_SLI
		return FRHIGPUMask((1 << (uint32)GNumActiveGPUsForRendering) - 1); 
#else
		return FRHIGPUMask(1);
#endif
	}

	struct FIterator
	{
		FORCEINLINE FIterator(const uint32 InGPUMask) : GPUMask(InGPUMask), FirstGPUIndexInMask(0)
		{
#if WITH_SLI
			FirstGPUIndexInMask = appCountTrailingZeros(InGPUMask);
#endif
		}

		FORCEINLINE void operator++()
		{
#if WITH_SLI
			GPUMask &= ~(1 << FirstGPUIndexInMask);
			FirstGPUIndexInMask = appCountTrailingZeros(GPUMask);
#else
			GPUMask = 0;
#endif
		}

		FORCEINLINE uint32 operator*() const { return FirstGPUIndexInMask; }

		FORCEINLINE bool operator !=(const FIterator& Rhs) const { return GPUMask != Rhs.GPUMask; }

	private:
		uint32 GPUMask;
		unsigned long FirstGPUIndexInMask;
	};

	FORCEINLINE friend FRHIGPUMask::FIterator begin(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(NodeMask.Value); }
	FORCEINLINE friend FRHIGPUMask::FIterator end(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(0); }
};

enum class EMultiGPUStrategy : uint32
{
	None,			// Use GPU0
	FrameIndex,		// Use GPU# where # = FrameIndex % NumGPU
	ViewIndex,		// Use GPU# where # = ViewIndex % NumGPU
	BroadCast
};

RHI_API EMultiGPUStrategy GetMultiGPUStrategyFromCommandLine();
RHI_API FRHIGPUMask GetNodeMaskFromMultiGPUStrategy(EMultiGPUStrategy Strategy, uint32 ViewIndex, uint32 FrameIndex);