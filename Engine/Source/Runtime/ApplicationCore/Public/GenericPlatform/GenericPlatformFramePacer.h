// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformFramePacer.h: Generic platform frame pacer classes
==============================================================================================*/


#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"

/**
 * Generic implementation for most platforms, these tend to be unused and unimplemented
 **/
struct APPLICATIONCORE_API FGenericPlatformRHIFramePacer
{
    /**
     * Should the Frame Pacer be enabled?
     */
    bool IsEnabled() { return false; }
    
    /**
     * Init the RHI frame pacer
     *
     * @param InTriggeredEvent - The event we wish to trigger when the frame interval has been triggered by the hardware.
     * @param InFrameInterval - How often should the event be triggered, in Frames.
     */
	static void InitWithEvent(FEvent* InTriggeredEvent) {}
    
    /**
     * Teardown the Frame Pacer.
     */
    static void Destroy() {}
	
	/**
	 * The pace we are running at (30 = 30fps, 0 = unpaced)
	 */
	static uint32 GetFramePace() { return 0; };
};
