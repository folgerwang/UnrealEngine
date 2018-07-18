// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterVrpnAnalogInputDevice.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterStrings.h"


FDisplayClusterVrpnAnalogInputDevice::FDisplayClusterVrpnAnalogInputDevice(const FDisplayClusterConfigInput& config) :
	FDisplayClusterVrpnAnalogInputDataHolder(config)
{
}

FDisplayClusterVrpnAnalogInputDevice::~FDisplayClusterVrpnAnalogInputDevice()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterVrpnAnalogInputDevice::Update()
{
	if (DevImpl)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Verbose, TEXT("Updating device: %s"), *GetId());
		DevImpl->mainloop();
	}
}

bool FDisplayClusterVrpnAnalogInputDevice::Initialize()
{
	FString addr;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, FString(DisplayClusterStrings::cfg::data::input::Address), addr))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - device address not found"), *ToString());
		return false;
	}

	// Instantiate device implementation
	DevImpl.Reset(new vrpn_Analog_Remote(TCHAR_TO_UTF8(*addr)));
	
	// Register update handler
	if (DevImpl->register_change_handler(this, &FDisplayClusterVrpnAnalogInputDevice::HandleAnalogDevice) != 0)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - couldn't register VRPN change handler"), *ToString());
		return false;
	}

	// Base initialization
	return FDisplayClusterVrpnAnalogInputDataHolder::Initialize();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterVrpnAnalogInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void VRPN_CALLBACK FDisplayClusterVrpnAnalogInputDevice::HandleAnalogDevice(void * userData, vrpn_ANALOGCB const an)
{
	auto pDev = reinterpret_cast<FDisplayClusterVrpnAnalogInputDevice*>(userData);

	for (int32 i = 0; i < an.num_channel; ++i)
	{
		auto pItem = pDev->DeviceData.Find(i);
		if (!pItem)
		{
			pItem = &pDev->DeviceData.Add(i);
		}

		pItem->axisValue = static_cast<float>(an.channel[i]);
		UE_LOG(LogDisplayClusterInputVRPN, VeryVerbose, TEXT("Axis %s:%d - %f"), *pDev->GetId(), i, pItem->axisValue);
	}
}
