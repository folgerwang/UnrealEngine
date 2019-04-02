// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputDevice.h"

/**
The public interface for MagicLeapHMD to control input device initialization & termination.

Note: SupportsExplicitEnable() is needed specifically for the gestures API. If it's not initialized
at the correct time, we might actually introduce performance degradataion for the feature. The hope
is that this function will not be needed in the future once gesture issues have been sorted out.
*/
class IMagicLeapInputDevice : public IInputDevice
{
public:
	virtual void Enable() = 0;
	virtual bool SupportsExplicitEnable() const = 0;
	virtual void Disable() = 0;
	virtual void OnBeginRendering_GameThread_Update() {}
};
