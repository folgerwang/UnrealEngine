// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERemoteSessionChannelMode
{
	Receive,
	Send
};

class FBackChannelOSCConnection;

class REMOTESESSION_API IRemoteSessionChannel
{

public:

	IRemoteSessionChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection) {}

	virtual ~IRemoteSessionChannel() {}

	virtual void Tick(const float InDeltaTime) = 0;

	virtual FString GetType() const = 0;

};
