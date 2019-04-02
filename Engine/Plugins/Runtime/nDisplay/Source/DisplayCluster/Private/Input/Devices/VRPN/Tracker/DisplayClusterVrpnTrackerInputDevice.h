// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Devices/VRPN/Tracker/DisplayClusterVrpnTrackerInputDataHolder.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "vrpn/vrpn_Tracker.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


/**
 * VRPN tracker device implementation
 */
class FDisplayClusterVrpnTrackerInputDevice
	: public FDisplayClusterVrpnTrackerInputDataHolder
{
public:
	FDisplayClusterVrpnTrackerInputDevice(const FDisplayClusterConfigInput& config);
	virtual ~FDisplayClusterVrpnTrackerInputDevice();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterInputDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void Update() override;
	virtual void PostUpdate() override;
	virtual bool Initialize() override;

protected:
	// Per-channel dirty state
	TMap<int32, bool> DirtyMap;

	// Transform form tracker space to DisplayCluster space
	void TransformCoordinates(FDisplayClusterVrpnTrackerChannelData& data) const;


private:
	// Tracker origin
	FVector  OriginLoc  = FVector::ZeroVector;
	FQuat    OriginQuat = FQuat::Identity;

private:
	// Coordinate system conversion
	enum AxisMapType { X = 0, NX, Y, NY, Z, NZ, W, NW };

	// Internal conversion helpers
	AxisMapType String2Map(const FString& str, const AxisMapType defaultMap) const;
	AxisMapType ComputeAxisW(const AxisMapType front, const AxisMapType right, const AxisMapType up) const;
	FVector  GetMappedLocation(const FVector& loc, const AxisMapType front, const AxisMapType right, const AxisMapType up) const;
	FQuat    GetMappedQuat(const FQuat& quat, const AxisMapType front, const AxisMapType right, const AxisMapType up, const AxisMapType axisW) const;

	// Tracker space to DisplayCluster space axis mapping
	AxisMapType AxisFront;
	AxisMapType AxisRight;
	AxisMapType AxisUp;
	AxisMapType AxisW;

private:
	// Data update handler
	static void VRPN_CALLBACK HandleTrackerDevice(void *userData, vrpn_TRACKERCB const tr);

private:
	// The device (PIMPL)
	TUniquePtr<vrpn_Tracker_Remote> DevImpl;
};
