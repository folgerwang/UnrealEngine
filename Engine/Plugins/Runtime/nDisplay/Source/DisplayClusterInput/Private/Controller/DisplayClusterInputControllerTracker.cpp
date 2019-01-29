// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputControllerTracker.h"
#include "IDisplayClusterInputModule.h"
#include "Misc/DisplayClusterInputLog.h"

#include "IDisplayCluster.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Input/IDisplayClusterInputManager.h"

#include "IDisplayClusterInputModule.h"


void FTrackerController::Initialize()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	//todo: Register new trackers for nDisplayCluster
}

void FTrackerController::ProcessStartSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	// Clear old binds
	ResetAllBindings();

	IDisplayClusterInputManager&  InputMgr  = *IDisplayCluster::Get().GetInputMgr();
	IDisplayClusterConfigManager& ConfigMgr = *IDisplayCluster::Get().GetConfigMgr();

	TArray<FString> DeviceNames;
	InputMgr.GetTrackerDeviceIds(DeviceNames);
	for (const FString& DeviceName : DeviceNames)
	{
		AddDevice(DeviceName);

		TArray<FDisplayClusterConfigInputSetup> Records = ConfigMgr.GetInputSetupRecords();
		for (const FDisplayClusterConfigInputSetup& Record : Records)
		{
			if (DeviceName.Compare(Record.Id, ESearchCase::IgnoreCase) == 0)
			{
				BindTracker(DeviceName, Record.Channel, Record.BindName);
			}
		}
	}
}

void FTrackerController::ProcessEndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	UE_LOG(LogDisplayClusterInputTracker, Verbose, TEXT("Removing all tracker bindings..."));

	ResetAllBindings();
}

void FTrackerController::ProcessPreTick()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	// Get data from VRPN devices
	IDisplayClusterInputManager& InputMgr = *IDisplayCluster::Get().GetInputMgr();
	for (auto& DeviceIt : BindMap)
	{
		for (auto& ChannelIt : DeviceIt.Value)
		{
			FQuat NewQuat;
			FVector NewPosition;
			if (InputMgr.GetTrackerLocation(DeviceIt.Key, ChannelIt.Key, NewPosition) &&
				InputMgr.GetTrackerQuat(DeviceIt.Key, ChannelIt.Key, NewQuat))
			{
				UE_LOG(LogDisplayClusterInputTracker, Verbose, TEXT("Obtained tracker data %s:%d => %s / %s"), *DeviceIt.Key, ChannelIt.Key, *NewPosition.ToString(), *NewQuat.ToString());
				ChannelIt.Value.SetData(NewQuat.Rotator(), NewPosition);
			}
		}
	}
}

bool FTrackerController::BindTracker(const FString& DeviceID, uint32 VrpnChannel, const FString& TargetName)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	// Find target TargetName analog value from user-friendly TargetName:
	EControllerHand TargetHand;
	if (!FControllerDeviceHelper::FindTrackerByName(TargetName, TargetHand))
	{
		// Bad target name, handle error details:
		UE_LOG(LogDisplayClusterInputTracker, Error, TEXT("Unknown bind tracker name <%s> for device <%s> channel <%i>"), *TargetName, *DeviceID, VrpnChannel);
		return false;
	}

	// Add new bind for tracker:
	return BindTracker(DeviceID, VrpnChannel, TargetHand);
}
bool FTrackerController::BindTracker(const FString& DeviceID, uint32 VrpnChannel, const EControllerHand TargetHand)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	// Create new bind:
	dev_channel_data_type& BindData = AddDeviceChannelBind(DeviceID, VrpnChannel);
	return BindData.BindTarget(TargetHand);
}

const FTrackerState* FTrackerController::GetDeviceBindData(const EControllerHand DeviceHand) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	for (const auto& DeviceIt : BindMap)
	{
		const FChannelBinds& ChannelBinds = DeviceIt.Value;
		for (const auto& ChannelIt : ChannelBinds)
		{
			const FTrackerState& ChannelBindData = ChannelIt.Value;
			if (ChannelBindData.FindTracker(DeviceHand) != INDEX_NONE)
			{
				//Found, return tracker data
				return &ChannelBindData;
			}
		}
	}

	// Not found any bind for this tracker type
	return nullptr; 
}

bool FTrackerController::IsTrackerConnected(const EControllerHand DeviceHand) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	return GetDeviceBindData(DeviceHand) != nullptr;
}

void FTrackerController::ApplyTrackersChanges()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	for (auto& DeviceIt : BindMap)
	{
		for (auto& ChannelIt : DeviceIt.Value)
		{
			ChannelIt.Value.ApplyChanges();
		}
	}
}

int FTrackerController::GetTrackersCount() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	int Result = 0;
	for (auto& DeviceIt : BindMap)
	{
		for (auto& ChannelIt : DeviceIt.Value)
		{
			Result += ChannelIt.Value.GetTrackersNum();
		}
	}

	return Result;
}

bool FTrackerController::GetTrackerOrientationAndPosition(const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputTracker);

	const FTrackerState* TrackerData = GetDeviceBindData(DeviceHand);
	if (TrackerData!=nullptr)
	{
		TrackerData->GetCurrentData(OutOrientation, OutPosition);
		return true;
	}

	return false;
}
