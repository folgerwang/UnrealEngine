// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterVrpnKeyboardInputDataHolder.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "vrpn/vrpn_Button.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


/**
 * VRPN button device implementation
 */
class FDisplayClusterVrpnKeyboardInputDevice
	: public FDisplayClusterVrpnKeyboardInputDataHolder
{
public:
	FDisplayClusterVrpnKeyboardInputDevice(const FDisplayClusterConfigInput& config);
	virtual ~FDisplayClusterVrpnKeyboardInputDevice();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterInputDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void PreUpdate() override;
	virtual void Update() override;
	virtual bool Initialize() override;

private:
	// Data update handler
	static void VRPN_CALLBACK HandleKeyboardDevice(void *userData, vrpn_BUTTONCB const b);

private:
	// The device (PIMPL)
	TUniquePtr<vrpn_Button_Remote> DevImpl;
};
