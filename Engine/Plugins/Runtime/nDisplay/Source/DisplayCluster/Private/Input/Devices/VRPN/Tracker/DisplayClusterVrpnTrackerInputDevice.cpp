// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterVrpnTrackerInputDevice.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterBuildConfig.h"
#include "DisplayClusterStrings.h"


FDisplayClusterVrpnTrackerInputDevice::FDisplayClusterVrpnTrackerInputDevice(const FDisplayClusterConfigInput& config) :
	FDisplayClusterVrpnTrackerInputDataHolder(config)
{
}

FDisplayClusterVrpnTrackerInputDevice::~FDisplayClusterVrpnTrackerInputDevice()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterVrpnTrackerInputDevice::Update()
{
	if (DevImpl)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Verbose, TEXT("Updating device: %s"), *GetId());
		DevImpl->mainloop();
	}
}

void FDisplayClusterVrpnTrackerInputDevice::PostUpdate()
{
	// Perform coordinates conversion
	for (auto it = DeviceData.CreateIterator(); it; ++it)
	{
		if (DirtyMap.Contains(it->Key))
		{
			// Convert data from updated channels only
			if (DirtyMap[it->Key] == true)
			{
				TransformCoordinates(it->Value);
				DirtyMap[it->Key] = false;
			}
		}
	}
}

bool FDisplayClusterVrpnTrackerInputDevice::Initialize()
{
	FString addr;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, DisplayClusterStrings::cfg::data::input::Address, addr))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - device address not found"), *ToString());
		return false;
	}

	// Instantiate device implementation
	DevImpl.Reset(new vrpn_Tracker_Remote(TCHAR_TO_UTF8(*addr)));

	// Register update handler
	if (DevImpl->register_change_handler(this, &FDisplayClusterVrpnTrackerInputDevice::HandleTrackerDevice) != 0)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - couldn't register VRPN change handler"), *ToString());
		return false;
	}

	// Extract tracker location
	FString loc;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, DisplayClusterStrings::cfg::data::Loc, loc, false))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - tracker origin location not found"), *ToString());
		return false;
	}

	// Extract tracker rotation
	FString rot;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, DisplayClusterStrings::cfg::data::Rot, rot, false))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - tracker origin rotation not found"), *ToString());
		return false;
	}

	// Parse location
	if (!OriginLoc.InitFromString(loc))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - unable to parse the tracker origin location"), *ToString());
		return false;
	}

	// Parse rotation
	FRotator originRot;
	if (!originRot.InitFromString(rot))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - unable to parse the tracker origin rotation"), *ToString());
		return false;
	}
	else
	{
		OriginQuat = originRot.Quaternion();
	}

	// Parse 'right' axis mapping
	FString right;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, DisplayClusterStrings::cfg::data::input::Right, right))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - 'right' axis mapping not found"), *ToString());
		return false;
	}

	// Parse 'forward' axis mapping
	FString front;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, DisplayClusterStrings::cfg::data::input::Front, front))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - 'front' axis mapping not found"), *ToString());
		return false;
	}

	// Parse 'up' axis mapping
	FString up;
	if (!DisplayClusterHelpers::str::ExtractParam(ConfigData.Params, DisplayClusterStrings::cfg::data::input::Up, up))
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("%s - 'up' axis mapping not found"), *ToString());
		return false;
	}
	
	// Store mapping rules
	AxisFront = String2Map(front, AxisMapType::X);
	AxisRight = String2Map(right, AxisMapType::Y);
	AxisUp = String2Map(up, AxisMapType::Z);
	AxisW = ComputeAxisW(AxisFront, AxisRight, AxisUp);

	// Base initialization
	return FDisplayClusterVrpnTrackerInputDataHolder::Initialize();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterVrpnTrackerInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	// Location
	float LocGetX(const FVector& loc)  { return  loc.X; }
	float LocGetNX(const FVector& loc) { return -loc.X; }

	float LocGetY(const FVector& loc)  { return  loc.Y; }
	float LocGetNY(const FVector& loc) { return -loc.Y; }

	float LocGetZ(const FVector& loc)  { return  loc.Z; }
	float LocGetNZ(const FVector& loc) { return -loc.Z; }

	// Rotation
	float RotGetX(const FQuat& quat)  { return  quat.X; }
	float RotGetNX(const FQuat& quat) { return -quat.X; }

	float RotGetY(const FQuat& quat)  { return  quat.Y; }
	float RotGetNY(const FQuat& quat) { return -quat.Y; }

	float RotGetZ(const FQuat& quat)  { return  quat.Z; }
	float RotGetNZ(const FQuat& quat) { return -quat.Z; }

	float RotGetW(const FQuat& quat)  { return  quat.W; }
	float RotGetNW(const FQuat& quat) { return -quat.W; }

	typedef float(*TLocGetter)(const FVector& loc);
	typedef float(*TRotGetter)(const FQuat&   rot);
}

FDisplayClusterVrpnTrackerInputDevice::AxisMapType FDisplayClusterVrpnTrackerInputDevice::String2Map(const FString& str, const AxisMapType defaultMap) const
{
	const FString mapVal = str.ToLower();

	if (mapVal == DisplayClusterStrings::cfg::data::input::MapX)
		return AxisMapType::X;
	else if (mapVal == DisplayClusterStrings::cfg::data::input::MapNX)
		return AxisMapType::NX;
	else if (mapVal == DisplayClusterStrings::cfg::data::input::MapY)
		return AxisMapType::Y;
	else if (mapVal == DisplayClusterStrings::cfg::data::input::MapNY)
		return AxisMapType::NY;
	else if (mapVal == DisplayClusterStrings::cfg::data::input::MapZ)
		return AxisMapType::Z;
	else if (mapVal == DisplayClusterStrings::cfg::data::input::MapNZ)
		return AxisMapType::NZ;
	else
	{
		UE_LOG(LogDisplayClusterInputVRPN, Warning, TEXT("Unknown mapping type: %s"), *str);
	}

	return defaultMap;
}

FDisplayClusterVrpnTrackerInputDevice::AxisMapType FDisplayClusterVrpnTrackerInputDevice::ComputeAxisW(const AxisMapType front, const AxisMapType right, const AxisMapType up) const
{
	int det = 1;

	if (front == AxisMapType::NX || front == AxisMapType::NY || front == AxisMapType::NZ)
		det *= -1;

	if (right == AxisMapType::NX || right == AxisMapType::NY || right == AxisMapType::NZ)
		det *= -1;

	if (up == AxisMapType::NX || up == AxisMapType::NY || up == AxisMapType::NZ)
		det *= -1;

	return (det < 0) ? AxisMapType::NW : AxisMapType::W;
}

FVector FDisplayClusterVrpnTrackerInputDevice::GetMappedLocation(const FVector& loc, const AxisMapType front, const AxisMapType right, const AxisMapType up) const
{
	static TLocGetter funcs[] = { &LocGetX, &LocGetNX, &LocGetY, &LocGetNY, &LocGetZ, &LocGetNZ };
	return FVector(funcs[front](loc), funcs[right](loc), funcs[up](loc));
}

FQuat FDisplayClusterVrpnTrackerInputDevice::GetMappedQuat(const FQuat& quat, const AxisMapType front, const AxisMapType right, const AxisMapType up, const AxisMapType axisW) const
{
	static TRotGetter funcs[] = { &RotGetX, &RotGetNX, &RotGetY, &RotGetNY, &RotGetZ, &RotGetNZ, &RotGetW, &RotGetNW };
	return FQuat(funcs[front](quat), funcs[right](quat), funcs[up](quat), -quat.W);// funcs[axisW](quat));
}

void FDisplayClusterVrpnTrackerInputDevice::TransformCoordinates(FDisplayClusterVrpnTrackerChannelData &data) const
{
	UE_LOG(LogDisplayClusterInputVRPN, VeryVerbose, TEXT("TransformCoordinates old: <loc:%s> <quat:%s>"), *data.trLoc.ToString(), *data.trQuat.ToString());

	// Transform location
	data.trLoc = OriginLoc + GetMappedLocation(data.trLoc, AxisFront, AxisRight, AxisUp);
	data.trLoc *= 100.f;

	// Transform rotation
	data.trQuat = OriginQuat * data.trQuat;
	data.trQuat = GetMappedQuat(data.trQuat, AxisFront, AxisRight, AxisUp, AxisW);

	UE_LOG(LogDisplayClusterInputVRPN, VeryVerbose, TEXT("TransformCoordinates new: <loc:%s> <quat:%s>"), *data.trLoc.ToString(), *data.trQuat.ToString());
}

void VRPN_CALLBACK FDisplayClusterVrpnTrackerInputDevice::HandleTrackerDevice(void *userData, vrpn_TRACKERCB const tr)
{
	auto pDev = reinterpret_cast<FDisplayClusterVrpnTrackerInputDevice*>(userData);
	
	const FVector loc(tr.pos[0], tr.pos[1], tr.pos[2]);
	const FQuat   quat(tr.quat[0], tr.quat[1], tr.quat[2], tr.quat[3]);

	const FDisplayClusterVrpnTrackerChannelData data{ loc, quat };
	auto pItem = &pDev->DeviceData.Add(tr.sensor, data);

	pDev->DirtyMap.Add(static_cast<int32>(tr.sensor), true);

	UE_LOG(LogDisplayClusterInputVRPN, VeryVerbose, TEXT("Tracker %s:%d {loc %s} {rot %s}"), *pDev->GetId(), tr.sensor, *pItem->trLoc.ToString(), *pItem->trQuat.ToString());
}
