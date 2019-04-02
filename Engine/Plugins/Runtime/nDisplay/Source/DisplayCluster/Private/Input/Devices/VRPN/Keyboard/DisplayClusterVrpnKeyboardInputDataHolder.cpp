// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterVrpnKeyboardInputDataHolder.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterVrpnKeyboardInputDataHolder::FDisplayClusterVrpnKeyboardInputDataHolder(const FDisplayClusterConfigInput& config) :
	FDisplayClusterInputDeviceBase<EDisplayClusterInputDeviceType::VrpnKeyboard>(config)
{
}

FDisplayClusterVrpnKeyboardInputDataHolder::~FDisplayClusterVrpnKeyboardInputDataHolder()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputDevice
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterVrpnKeyboardInputDataHolder::Initialize()
{
	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStringSerializable
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterVrpnKeyboardInputDataHolder::SerializeToString() const
{
	FString result;
	result.Reserve(64);

	for (auto it = DeviceData.CreateConstIterator(); it; ++it)
	{
		result += FString::Printf(TEXT("%d%s%d%s%d%s"), it->Key, SerializationDelimiter, it->Value.btnStateOld, SerializationDelimiter, it->Value.btnStateNew, SerializationDelimiter);
	}

	return result;
}

bool FDisplayClusterVrpnKeyboardInputDataHolder::DeserializeFromString(const FString& data)
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
		const bool GetStateOld = (FCString::Atoi(*parsed[i + 1]) != 0);
		const bool GetStateNew = (FCString::Atoi(*parsed[i + 2]) != 0);
		DeviceData.Add(ch, FDisplayClusterVrpnKeyboardChannelData{ GetStateOld, GetStateNew });
	}

	return true;
}