// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Input/Devices/VRPN/Button/DisplayClusterVrpnButtonInputDataHolder.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterVrpnButtonInputDataHolder::FDisplayClusterVrpnButtonInputDataHolder(const FDisplayClusterConfigInput& config) :
	FDisplayClusterInputDeviceBase<EDisplayClusterInputDeviceType::VrpnButton>(config)
{
}

FDisplayClusterVrpnButtonInputDataHolder::~FDisplayClusterVrpnButtonInputDataHolder()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterVrpnButtonInputDataHolder::Initialize()
{
	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStringSerializable
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterVrpnButtonInputDataHolder::SerializeToString() const
{
	FString result;
	result.Reserve(64);

	for (auto it = DeviceData.CreateConstIterator(); it; ++it)
	{
		result += FString::Printf(TEXT("%d%s%d%s%d%s"), it->Key, SerializationDelimiter, it->Value.btnStateOld, SerializationDelimiter, it->Value.btnStateNew, SerializationDelimiter);
	}

	return result;
}

bool FDisplayClusterVrpnButtonInputDataHolder::DeserializeFromString(const FString& data)
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
		const int  ch = FCString::Atoi(*parsed[i]);
		const bool stateOld = (FCString::Atoi(*parsed[i + 1]) != 0);
		const bool stateNew = (FCString::Atoi(*parsed[i + 2]) != 0);
		DeviceData.Add(ch, FDisplayClusterVrpnButtonChannelData{ stateOld, stateNew });
	}

	return true;
}
