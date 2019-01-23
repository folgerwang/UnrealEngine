// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "DisplayClusterInputControllerBase.h"

class FGenericApplicationMessageHandler;


class FTrackerController :
	public FControllerDeviceBase<EDisplayClusterInputDeviceType::VrpnTracker>
{
public:
	virtual ~FTrackerController()
	{ }

public:
	virtual void Initialize() override;

	virtual void ProcessStartSession() override;
	virtual void ProcessEndSession() override;
	virtual void ProcessPreTick() override;

	// Create tracker bind for specified channel on vrpn device to target
	bool BindTracker(const FString& DeviceID, uint32 VrpnChannel, const FString& TargetName);
	bool BindTracker(const FString& DeviceID, uint32 VrpnChannel, const EControllerHand TargetHand);

	// Return total count of binded trackers
	int GetTrackersCount() const;
	// Return true, if required tracker is binded
	bool IsTrackerConnected(const EControllerHand DeviceHand) const;
	// Synchronize trackers data to current value
	void ApplyTrackersChanges();
	// Return true & tracker pos+rot, if binded
	bool GetTrackerOrientationAndPosition(const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition) const;

private:
	// Return nullptr or bind first data for requested EControllerHand
	const FTrackerState* GetDeviceBindData(const EControllerHand DeviceHand) const;
};
