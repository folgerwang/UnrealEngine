// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"
#include "Network/DisplayClusterMessage.h"


/**
 * Cluster state synchronization protocol
 */
class IPDisplayClusterClusterSyncProtocol
{
public:
	// Game start barrier
	virtual void WaitForGameStart() = 0;

	// Frame start barrier
	virtual void WaitForFrameStart() = 0;

	// Frame end barrier
	virtual void WaitForFrameEnd() = 0;

	// Tick end barrier
	virtual void WaitForTickEnd() = 0;

	// Provides with time delta for current frame
	virtual void GetDeltaTime(float& deltaTime) = 0;

	// Get the Timecode value for the current frame.
	virtual void GetTimecode(FTimecode& timecode, FFrameRate& frameRate) = 0;

	// Sync objects
	virtual void GetSyncData(FDisplayClusterMessage::DataType& data) = 0;

	// Sync input
	virtual void GetInputData(FDisplayClusterMessage::DataType& data) = 0;
};

