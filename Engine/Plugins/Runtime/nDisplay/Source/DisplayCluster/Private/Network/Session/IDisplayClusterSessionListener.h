// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/DisplayClusterLog.h"

class FDisplayClusterMessage;
class FDisplayClusterSessionBase;
class FJsonObject;


/**
 * TCP session listener interface
 */
class IDisplayClusterSessionListener
{
public:
	virtual ~IDisplayClusterSessionListener()
	{ }

	virtual void NotifySessionOpen(FDisplayClusterSessionBase* InSession)
	{ }

	virtual void NotifySessionClose(FDisplayClusterSessionBase* InSession)
	{ }

	// Pass a message to a concrete implementation
	virtual TSharedPtr<FDisplayClusterMessage> ProcessMessage(const TSharedPtr<FDisplayClusterMessage>& Message)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("This type of message is not supported"));
		return nullptr;
	}

	// Pass a json object to a concrete implementation
	virtual TSharedPtr<FJsonObject> ProcessJson(const TSharedPtr<FJsonObject>& Message)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("This type of message is not supported"));
		return nullptr;
	}
};
