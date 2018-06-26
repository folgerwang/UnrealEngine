// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.h: Multi-GPU support
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/** When greater than one, indicates that SLI rendering is enabled */
#if PLATFORM_DESKTOP
#define WITH_SLI 1	// Implicit SLI
#define WITH_MGPU 1	// Explicit MGPU
#define MAX_NUM_GPUS 4
extern RHI_API uint32 GNumExplicitGPUsForRendering;
extern RHI_API uint32 GNumAlternateFrameRenderingGroups;
#else
#define WITH_SLI 0
#define WITH_MGPU 0
#define MAX_NUM_GPUS 1
#define GNumExplicitGPUsForRendering 1
#define GNumAlternateFrameRenderingGroups 1
#endif

/** A mask where each bit is a GPU index. Can not be empty so that non SLI platforms can optimize it to be always 1.  */
struct FRHIGPUMask
{
private:
	uint32 GPUMask;

public:
	FORCEINLINE explicit FRHIGPUMask(uint32 InGPUMask) : GPUMask(InGPUMask)
	{
#if WITH_MGPU
		check(InGPUMask != 0);
#else
		check(InGPUMask == 1);
#endif
	}

	FORCEINLINE FRHIGPUMask() : GPUMask(FRHIGPUMask::GPU0())
	{
	}

	FORCEINLINE static FRHIGPUMask FromIndex(uint32 GPUIndex) { return FRHIGPUMask(1 << GPUIndex); }

	FORCEINLINE uint32 ToIndex() const
	{
#if WITH_MGPU
		check(HasSingleIndex());
		return FMath::CountTrailingZeros(GPUMask);
#else
		return 0;
#endif
	}

	FORCEINLINE bool HasSingleIndex() const
	{
#if WITH_MGPU
		return FMath::IsPowerOfTwo(GPUMask);
#else
		return true;
#endif
	}

	FORCEINLINE uint32 GetLastIndex()
	{
#if WITH_MGPU
		return FPlatformMath::FloorLog2(GPUMask);
#else
		return 0;
#endif
	}

	FORCEINLINE uint32 GetFirstIndex()
	{
#if WITH_MGPU
		return FPlatformMath::CountTrailingZeros(GPUMask);
#else
		return 0;
#endif
	}

	FORCEINLINE bool Contains(uint32 GPUIndex) const { return (GPUMask & (1 << GPUIndex)) != 0; }
	FORCEINLINE bool Intersects(const FRHIGPUMask& Rhs) const { return (GPUMask & Rhs.GPUMask) != 0; }

	FORCEINLINE bool operator ==(const FRHIGPUMask& Rhs) const { return GPUMask == Rhs.GPUMask; }

	void operator |=(const FRHIGPUMask& Rhs) { GPUMask |= Rhs.GPUMask; }
	void operator &=(const FRHIGPUMask& Rhs) { GPUMask &= Rhs.GPUMask; }
	FORCEINLINE operator uint32() const { return GPUMask; }

	FORCEINLINE FRHIGPUMask operator &(const FRHIGPUMask& Rhs) const
	{
		return FRHIGPUMask(GPUMask & Rhs.GPUMask);
	}

	FORCEINLINE FRHIGPUMask operator |(const FRHIGPUMask& Rhs) const
	{
		return FRHIGPUMask(GPUMask | Rhs.GPUMask);
	}

	FORCEINLINE static const FRHIGPUMask GPU0() { return FRHIGPUMask(1); }
	FORCEINLINE static const FRHIGPUMask All() { return FRHIGPUMask((1 << GNumExplicitGPUsForRendering) - 1); }

	struct FIterator
	{
		FORCEINLINE FIterator(const uint32 InGPUMask) : GPUMask(InGPUMask), FirstGPUIndexInMask(0)
		{
#if WITH_MGPU
			FirstGPUIndexInMask = FPlatformMath::CountTrailingZeros(InGPUMask);
#endif
		}

		FORCEINLINE void operator++()
		{
#if WITH_MGPU
			GPUMask &= ~(1 << FirstGPUIndexInMask);
			FirstGPUIndexInMask = FPlatformMath::CountTrailingZeros(GPUMask);
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

	FORCEINLINE friend FRHIGPUMask::FIterator begin(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(NodeMask.GPUMask); }
	FORCEINLINE friend FRHIGPUMask::FIterator end(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(0); }
};
