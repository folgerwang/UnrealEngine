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
