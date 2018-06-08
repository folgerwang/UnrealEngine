// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.cpp: Multi-GPU support
=============================================================================*/

#include "MultiGPU.h"
#include "RHI.h"

#if WITH_SLI || WITH_MGPU
uint32 GNumAlternateFrameRenderingGroups = 1;
uint32 GNumExplicitGPUsForRendering = 1;
#endif

EMultiGPUMode GetMultiGPUMode()
{
	static EMultiGPUMode GPUMode = EMultiGPUMode::GPU0;
	static bool bInitialized = false;
	
	if (!bInitialized && GNumExplicitGPUsForRendering > 1)
	{
		FString GPUModeToken;
		FParse::Value(FCommandLine::Get(), TEXT("MGPUMode="), GPUModeToken);

		if (GPUModeToken == TEXT("AFR"))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in ALTERNATE_FRAME mode"));
			GPUMode = EMultiGPUMode::AlternateFrame;
		}
		else if (GPUModeToken == TEXT("AVR"))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in ALTERNATE_VIEW mode"));
			GPUMode = EMultiGPUMode::AlternateView;
		}
		else if (GPUModeToken == TEXT("Broadcast"))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in BROADCAST mode"));
			GPUMode = EMultiGPUMode::Broadcast;
		}
		else if (GPUModeToken == TEXT("GPU1"))
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in GPU1 mode"));
			GPUMode = EMultiGPUMode::GPU1;
		}
		else
		{
			UE_LOG(LogRHI, Log, TEXT("Using multi-GPU in GPU0 mode"));
			GPUMode = EMultiGPUMode::GPU0;
		}
		bInitialized = true;
	}

	return GPUMode;
}


FRHIGPUMask GetNodeMaskFromMultiGPUMode(EMultiGPUMode GPUMode, uint32 ViewIndex, uint32 FrameIndex)
{
	switch (GPUMode)
	{
	case EMultiGPUMode::AlternateView:
		return FRHIGPUMask::FromIndex(ViewIndex % GNumExplicitGPUsForRendering);
	case EMultiGPUMode::AlternateFrame:
		return FRHIGPUMask::FromIndex((FrameIndex + 1) % GNumExplicitGPUsForRendering);
	case EMultiGPUMode::Broadcast:
		return FRHIGPUMask::All();
	case EMultiGPUMode::GPU1:
		return FRHIGPUMask::FromIndex(1 % GNumExplicitGPUsForRendering);
	default:
		return FRHIGPUMask::GPU0();
	}
}
