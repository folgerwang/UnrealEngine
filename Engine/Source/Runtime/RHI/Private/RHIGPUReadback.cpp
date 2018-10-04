// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIGPUReadback.cpp: Convenience function implementations for async GPU 
	memory updates and readbacks
=============================================================================*/

#include "RHIGPUReadback.h"


void FRHIGPUFence::Write()
{
	UE_LOG(LogRHI, Warning, TEXT("FRHIGPUFence::Write is not implemented"));
}

FGenericRHIGPUFence::FGenericRHIGPUFence(FName InName)
    : FRHIGPUFence(InName)
    , InsertedFrameNumber(0)
{}

void FGenericRHIGPUFence::Write()
{
	// GPU generally overlap the game. This overlap increases when using AFR.
	InsertedFrameNumber = GFrameNumberRenderThread + GNumAlternateFrameRenderingGroups;
}

bool FGenericRHIGPUFence::Poll() const
{
	if (GFrameNumberRenderThread > InsertedFrameNumber)
	{
		return true;
	}
	return false;
}
