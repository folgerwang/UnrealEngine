// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Devices/VRPN/Analog/DisplayClusterVrpnAnalogInputDataHolder.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "vrpn/vrpn_Analog.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


/**
 * VRPN analog device implementation
 */
class FDisplayClusterVrpnAnalogInputDevice
	: public FDisplayClusterVrpnAnalogInputDataHolder
{
public:
	FDisplayClusterVrpnAnalogInputDevice(const FDisplayClusterConfigInput& config);
	virtual ~FDisplayClusterVrpnAnalogInputDevice();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterInputDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void Update() override;
	virtual bool Initialize() override;

private:
	// Data update handler
	static void VRPN_CALLBACK HandleAnalogDevice(void *userData, vrpn_ANALOGCB const tr);

private:
	// The device (PIMPL)
	TUniquePtr<vrpn_Analog_Remote> DevImpl;
};
