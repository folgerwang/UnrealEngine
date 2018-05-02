// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.cpp: Multi-GPU support
=============================================================================*/

#include "MultiGPU.h"

EMultiGPUStrategy GetMultiGPUStrategyFromCommandLine()
{
	static EMultiGPUStrategy Strategy;
	static bool bInitialized = false;
	
	if (!bInitialized)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("mGPU_AFR")))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in FRAME_INDEX mode"));
			Strategy = EMultiGPUStrategy::FrameIndex;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("mGPU_AVR")))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in VIEW_INDEX mode"));
			Strategy = EMultiGPUStrategy::ViewIndex;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("mGPU_BroadCast")))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in BROADCAST mode"));
			Strategy = EMultiGPUStrategy::BroadCast;
		}
		bInitialized = true;
	}

	return Strategy;
}


FRHIGPUMask GetNodeMaskFromMultiGPUStrategy(EMultiGPUStrategy Strategy, uint32 ViewIndex, uint32 FrameIndex)
{
	switch (Strategy)
	{
	case EMultiGPUStrategy::ViewIndex:
		return FRHIGPUMask::FromIndex(ViewIndex % GNumActiveGPUsForRendering);
	case EMultiGPUStrategy::FrameIndex:
		return FRHIGPUMask::FromIndex((FrameIndex + 1) % GNumActiveGPUsForRendering);
	case EMultiGPUStrategy::BroadCast:
		return FRHIGPUMask::All();
	default:
		return FRHIGPUMask::GPU0();
	}
}