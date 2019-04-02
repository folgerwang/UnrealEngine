// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Input/Devices/VRPN/Analog/DisplayClusterVrpnAnalogInputDataHolder.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterVrpnAnalogInputDataHolder::FDisplayClusterVrpnAnalogInputDataHolder(const FDisplayClusterConfigInput& config) :
	FDisplayClusterInputDeviceBase<EDisplayClusterInputDeviceType::VrpnAnalog>(config)
{
}

FDisplayClusterVrpnAnalogInputDataHolder::~FDisplayClusterVrpnAnalogInputDataHolder()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterVrpnAnalogInputDataHolder::Initialize()
{
	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStringSerializable
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterVrpnAnalogInputDataHolder::SerializeToString() const
{
	FString result;
	result.Reserve(128);

	for (auto it = DeviceData.CreateConstIterator(); it; ++it)
	{
		result += FString::Printf(TEXT("%d%s%f%s"), it->Key, SerializationDelimiter, it->Value.axisValue, SerializationDelimiter);
	}

	return result;
}

bool FDisplayClusterVrpnAnalogInputDataHolder::DeserializeFromString(const FString& data)
{
	TArray<FString> parsed;
	data.ParseIntoArray(parsed, SerializationDelimiter);

	if (parsed.Num() % SerializationItems)
	{
		UE_LOG(LogDisplayClusterInputVRPN, Error, TEXT("Wrong items amount after deserialization [%s]"), *data);
		return false;
	}

	for (int i = 0; i < parsed.Num(); i += SerializationItems)
	{
		const int   ch = FCString::Atoi(*parsed[i]);
		const float val = FCString::Atof(*parsed[i + 1]);
		DeviceData.Add(ch, FDisplayClusterVrpnAnalogChannelData{ val });
	}

	return true;
}
