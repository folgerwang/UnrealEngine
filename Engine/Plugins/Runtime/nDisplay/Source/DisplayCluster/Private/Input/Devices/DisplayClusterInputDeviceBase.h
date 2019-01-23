// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Devices/IDisplayClusterInputDevice.h"
#include "Input/Devices/DisplayClusterInputDeviceTraits.h"
#include "Misc/DisplayClusterLog.h"

#include "CoreMinimal.h"


/**
 * Abstract input device
 */
template <int DevTypeID>
class FDisplayClusterInputDeviceBase
	: public IDisplayClusterInputDevice
{
public:
	typedef typename display_cluster_input_device_traits<DevTypeID>::dev_channel_data_type   TChannelData;

public:
	FDisplayClusterInputDeviceBase(const FDisplayClusterConfigInput& config) :
		ConfigData(config)
	{ }

	virtual ~FDisplayClusterInputDeviceBase()
	{ }

public:
	virtual bool GetChannelData(const uint8 channel, TChannelData& data) const
	{
		uint8 channelToGet = channel;
		if (ConfigData.ChMap.Contains(channel))
		{
			channelToGet = (uint8)ConfigData.ChMap[channel];
			UE_LOG(LogDisplayClusterInputVRPN, Verbose, TEXT("DevType %d, channel %d - remapped to channel %d"), DevTypeID, channel, channelToGet);
		}

		if (!DeviceData.Contains(static_cast<int32>(channelToGet)))
		{
			UE_LOG(LogDisplayClusterInputVRPN, Verbose, TEXT("%s - channel %d data is not available yet"), *GetId(), channelToGet);
			return false;
		}

		data = DeviceData[static_cast<int32>(channelToGet)];

		return true;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterInputDevice
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString GetId() const override
	{ return ConfigData.Id; }

	virtual FString GetType() const override
	{ return ConfigData.Type; }

	virtual EDisplayClusterInputDeviceType GetTypeId() const override
	{ return static_cast<EDisplayClusterInputDeviceType>(DevTypeID); }

	virtual FDisplayClusterConfigInput GetConfig() const override
	{ return ConfigData; }

	virtual void PreUpdate() override
	{ }

	virtual void Update() override
	{ }

	virtual void PostUpdate() override
	{ }

	virtual FString ToString() const override
	{ return FString::Printf(TEXT("DisplayCluster input device: id=%s, type=%s"), *GetId(), *GetType()); }

protected:
	// Original config data
	const FDisplayClusterConfigInput ConfigData;
	// Device data
	TMap<int32, TChannelData> DeviceData;
};
