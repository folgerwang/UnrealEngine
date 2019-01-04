// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BackChannel/Protocol/OSC/BackChannelOSCDispatch.h"
#include "BackChannel/Private/BackChannelCommon.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"


FBackChannelOSCDispatch::FBackChannelOSCDispatch()
{

}

FBackChannelDispatchDelegate& FBackChannelOSCDispatch::GetAddressHandler(const TCHAR* Path)
{
	FString LowerPath = FString(Path).ToLower();

	if (DispatchMap.Contains(LowerPath) == false)
	{
		DispatchMap.Add(LowerPath);
	}

	return DispatchMap.FindChecked(LowerPath);
}


void FBackChannelOSCDispatch::DispatchMessage(FBackChannelOSCMessage& Message)
{
	FString LowerAddress = Message.GetAddress().ToLower();

	for (const auto& KV : DispatchMap)
	{
		FString LowerPath = KV.Key.ToLower();

		if (LowerAddress.StartsWith(LowerPath))
		{
			KV.Value.Broadcast(Message, *this);
			Message.ResetRead();
		}
	}
}
