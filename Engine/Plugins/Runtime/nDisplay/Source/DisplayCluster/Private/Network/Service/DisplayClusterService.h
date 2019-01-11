// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterServer.h"
#include "Sockets.h"

class FDisplayClusterSessionBase;
struct FIPv4Endpoint;


/**
 * Abstract DisplayCluster service
 */
class FDisplayClusterService
	: public FDisplayClusterServer
{
public:
	FDisplayClusterService(const FString& InName, const FString& InAddr, const int32 InPort);

public:
	static bool IsClusterIP(const FIPv4Endpoint& InEP);

protected:
	virtual bool IsConnectionAllowed(FSocket* InSocket, const FIPv4Endpoint& InEP) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void NotifySessionOpen(FDisplayClusterSessionBase* InSession) override;
	virtual void NotifySessionClose(FDisplayClusterSessionBase* InSession) override;
};

