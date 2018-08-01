// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterVrpnButtonInputDevice.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterStrings.h"


FDisplayClusterVrpnButtonInputDevice::FDisplayClusterVrpnButtonInputDevice(const FDisplayClusterConfigInput& config) :
	FDisplayClusterVrpnButtonInputDataHolder(config)
{
}

FDisplayClusterVrpnButtonInputDevice::~FDisplayClusterVrpnButtonInputDevice()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterVrpnButtonInputDevice::PreUpdate()
{
	// Update 'old' states before calling mainloop
	for (auto it = DeviceData.CreateIterator(); it; ++it)
	{
		it->Value.btnStateOld = it->Value.btnStateNew;
	}
}

void FDisplayClusterVrpnButtonInputDevice::Update()
{
	if (DevImpl)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Verbose, TEXT("Updating device: %s"), *GetId());
		DevImpl->mainloop();
	}
}

bool FDisplayClusterVrpnButtonInputDevice::Initialize()
{
	FString addr;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, FString(DisplayClusterStrings::cfg::data::input::Address), addr))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - device address not found"), *ToString());
		return false;
	}

	// Instantiate device implementation
	DevImpl.Reset(new vrpn_Button_Remote(TCHAR_TO_UTF8(*addr)));
	// Register update handler
	if(DevImpl->register_change_handler(this, &FDisplayClusterVrpnButtonInputDevice::HandleButtonDevice) != 0)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - couldn't register VRPN change handler"), *ToString());
		return false;
	}

	// Base initialization
	return FDisplayClusterVrpnButtonInputDataHolder::Initialize();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterVrpnButtonInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void VRPN_CALLBACK FDisplayClusterVrpnButtonInputDevice::HandleButtonDevice(void *userData, vrpn_BUTTONCB const b)
{
	auto pDev = reinterpret_cast<FDisplayClusterVrpnButtonInputDevice*>(userData);
	
	auto pItem = pDev->DeviceData.Find(b.button);
	if (!pItem)
	{
		pItem = &pDev->DeviceData.Add(b.button);
		// Explicit initial old state set
		pItem->btnStateOld = false;
	}

	//@note: Actually the button can change state for several time during one update cycle. For example
	//       it could change 0->1->0. Then we will send only the latest state and as a result the state
	//       change won't be processed. I don't process such situations because it's not ok if button
	//       changes the state so quickly. It's probably a contact shiver or something else. Normal button
	//       usage will lead to state change separation between update frames.


	// Convert button state from int to bool here. Actually VRPN has only two states for
	// buttons (0-released, 1-pressed) but still uses int32 type for the state.
	pItem->btnStateNew = (b.state != 0);
	UE_LOG(LogDisplayClusterInputVRPN, VeryVerbose, TEXT("Button %s:%d - %d"), *pDev->GetId(), b.button, b.state);
}
