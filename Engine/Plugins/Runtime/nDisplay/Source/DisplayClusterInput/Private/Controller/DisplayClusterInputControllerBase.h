// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "Controller/IDisplayClusterInputController.h"

#include "Input/IDisplayClusterInputManager.h"
#include "Input/Devices/DisplayClusterInputDeviceTraits.h"

#include "InputCoreTypes.h"
#include "DisplayClusterInputStateAnalog.h"
#include "DisplayClusterInputStateButton.h"
#include "DisplayClusterInputStateKeyboard.h"
#include "DisplayClusterInputStateTracker.h"

#include "Misc/DisplayClusterInputLog.h"


/**
* Input device traits
*/
template<EDisplayClusterInputDeviceType DevTypeID>
struct display_cluster_input_controller_traits
{ 
	virtual ~display_cluster_input_controller_traits()
	{}
};

/**
* Specialization for all device types
*/
template <>
struct display_cluster_input_controller_traits<EDisplayClusterInputDeviceType::VrpnAnalog>
{
	typedef FAnalogState           dev_channel_data_type;
};

template <>
struct display_cluster_input_controller_traits<EDisplayClusterInputDeviceType::VrpnButton>
{
	typedef FButtonState           dev_channel_data_type;
};

template <>
struct display_cluster_input_controller_traits<EDisplayClusterInputDeviceType::VrpnKeyboard>
{
	typedef FButtonState           dev_channel_data_type;
};

template <>
struct display_cluster_input_controller_traits<EDisplayClusterInputDeviceType::VrpnTracker>
{
	typedef FTrackerState           dev_channel_data_type;
};


class FControllerDeviceHelper
{
public:
	static bool FindUnrealEngineKeyByName(EDisplayClusterInputDeviceType DevType, const FString& TargetName, FName& TargetKey);
	static bool FindTrackerByName(const FString& TargetName, EControllerHand& TargetTracker);

private:
	static void InitializeAllDefinedFKey();
	static TArray<FKey> AllDefinedKeys;
};


template <EDisplayClusterInputDeviceType DevTypeID>
class FControllerDeviceBase : 
	public IDisplayClusterInputController,
	public display_cluster_input_controller_traits<DevTypeID>
{
protected:
	typedef typename display_cluster_input_controller_traits<DevTypeID>::dev_channel_data_type FChannelBindData;
	typedef TMap<int32, FChannelBindData> FChannelBinds;

public:
	virtual ~FControllerDeviceBase()
	{
		ResetAllBindings();
	}

public:
	virtual void Initialize()
	{ }

public:
	virtual bool HasDevice(const FString DeviceName) const override
	{
		for (auto& It : BindMap)
		{
			if (DeviceName.Compare(It.Key, ESearchCase::IgnoreCase) == 0)
			{
				return true;
			}
		}

		return false;
	}

	EDisplayClusterInputDeviceType GetDevTypeID() const
	{
		return DevTypeID;
	}

	// Release all internal bindings
	void ResetAllBindings()
	{
		for (auto& DeviceIt : BindMap)
		{
			for (auto& ChannelIt : DeviceIt.Value)
			{
				ChannelIt.Value.Reset();
			}

			DeviceIt.Value.Empty();
		}

		BindMap.Empty();
	}

	// Creates channels data for vrpn device
	FChannelBinds&  AddDevice(const FString& DeviceID)
	{
		if (!HasDevice(DeviceID))
		{
			// Create dummy for new device
			BindMap.Add(DeviceID, FChannelBinds());
		}

		return BindMap[DeviceID];
	}

	// Creates new vrpn device channel binding
	FChannelBindData& AddDeviceChannelBind(const FString& DeviceID, int32 VrpnChannelIndex)
	{
		FChannelBinds& DeviceData = AddDevice(DeviceID);
		if (!DeviceData.Contains(VrpnChannelIndex))
		{
			// Create unique channel binds data
			DeviceData.Add(VrpnChannelIndex, FChannelBindData());
		}

		return DeviceData[VrpnChannelIndex];
	}

	// Create bind for specified channel on vrpn device to target
	bool BindChannel(const FString& DeviceID, uint32 VrpnChannel, const FString& TargetName)
	{
		// Find target FKey analog value from user-friendly TargetName:
		FName TargetKey;
		if (!FControllerDeviceHelper::FindUnrealEngineKeyByName(GetDevTypeID(), TargetName, TargetKey))
		{
			// Bad target name, handle error details:
			UE_LOG(LogDisplayClusterInputModule, Error, TEXT("Unknown bind target name <%s> for device <%s> channel <%i>"), *TargetName, *DeviceID, VrpnChannel);
			return false;
		}

		FChannelBindData& BindData = AddDeviceChannelBind(DeviceID, VrpnChannel);
		if (!BindData.BindTarget(TargetKey))
		{
			// Duplicate bind call, warning
			UE_LOG(LogDisplayClusterInputModule, Warning, TEXT("Duplicated bind <%s> for device <%s> channel <%i>"), *TargetKey.ToString(), *DeviceID, VrpnChannel);
			return false;
		}

		return true;
	}

	// Send vrpn channels data to UE4 core
	void UpdateEvents(const double CurrentTime, FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId)
	{
		for (auto& DeviceIt : BindMap)
		{
			for (auto& ChannelIt : DeviceIt.Value)
			{
				ChannelIt.Value.UpdateEvents(MessageHandler, ControllerId, CurrentTime);
			}
		}
	}

protected:
	TMap<FString, FChannelBinds> BindMap;
};
