// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;

DECLARE_MULTICAST_DELEGATE_TwoParams(FBackChannelDispatchDelegate, FBackChannelOSCMessage&, FBackChannelOSCDispatch&)

class BACKCHANNEL_API FBackChannelOSCDispatch
{
	
public:

	FBackChannelOSCDispatch();

	virtual ~FBackChannelOSCDispatch() {}


	FBackChannelDispatchDelegate& GetAddressHandler(const TCHAR* Path);

	void	DispatchMessage(FBackChannelOSCMessage& Message);


protected:

	TMap<FString, FBackChannelDispatchDelegate> DispatchMap;

};