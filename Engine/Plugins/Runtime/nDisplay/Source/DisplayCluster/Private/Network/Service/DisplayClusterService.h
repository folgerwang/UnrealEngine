// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterServer.h"
#include "Sockets.h"

class FDisplayClusterSession;
struct FIPv4Endpoint;


/**
 * Abstract DisplayCluster server
 */
class FDisplayClusterService
	: public FDisplayClusterServer
{
public:
	FDisplayClusterService(const FString& name, const FString& addr, const int32 port);

public:
	static bool IsClusterIP(const FIPv4Endpoint& ep);

protected:
	virtual bool IsConnectionAllowed(FSocket* pSock, const FIPv4Endpoint& ep) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void NotifySessionOpen(FDisplayClusterSession* pSession) override;
	virtual void NotifySessionClose(FDisplayClusterSession* pSession) override;
};

