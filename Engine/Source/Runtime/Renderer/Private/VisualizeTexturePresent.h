// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VisualizeTexturePresent.h: Display texture visualization on screen.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

class FViewInfo;


/** Presents */
class FVisualizeTexturePresent
{
public:
	/** Starts texture visualization capture. */
	static void OnStartRender(const FViewInfo& View);

	/** Present the visualize texture tool on screen. */
	static void PresentContent(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	/** Dumps every information to log. */
	static void DebugLog(bool bExtended);

private:
	static uint32 ComputeEventDisplayHeight();

};
