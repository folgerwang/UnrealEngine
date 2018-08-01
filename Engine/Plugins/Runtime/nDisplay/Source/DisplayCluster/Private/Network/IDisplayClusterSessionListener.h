// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterMessage.h"

class FDisplayClusterSession;


/**
 * TCP session listener interface
 */
struct IDisplayClusterSessionListener
{
	virtual ~IDisplayClusterSessionListener()
	{ }

	virtual void NotifySessionOpen(FDisplayClusterSession* pSession)
	{ }

	virtual void NotifySessionClose(FDisplayClusterSession* pSession)
	{ }

	// Pass a message to a concrete implementation
	virtual FDisplayClusterMessage::Ptr ProcessMessage(FDisplayClusterMessage::Ptr msg) = 0;
};

