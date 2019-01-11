// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/Session/IDisplayClusterSessionListener.h"
#include "Network/Session/DisplayClusterSessionBase.h"
#include "Network/DisplayClusterTcpListener.h"


struct FIPv4Endpoint;


/**
 * TCP server
 */
class FDisplayClusterServer
	: public IDisplayClusterSessionListener
{
public:
	FDisplayClusterServer(const FString& InName, const FString& InAddr, const int32 InPort);
	virtual ~FDisplayClusterServer();

public:
	// Start server
	virtual bool Start();
	// Stop server
	virtual void Shutdown();

	// Returns current server state
	bool IsRunning();

	// Server name
	inline const FString& GetName() const
	{ return Name; }

	// Server addr
	inline const FString& GetAddr() const
	{ return Address; }

	// Server port
	inline const int32& GetPort() const
	{ return Port; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void NotifySessionOpen(FDisplayClusterSessionBase* InSession) override;
	virtual void NotifySessionClose(FDisplayClusterSessionBase* InSession) override;
	
protected:
	// Ask concrete server implementation if connection is allowed
	virtual bool IsConnectionAllowed(FSocket* InSock, const FIPv4Endpoint& InEP)
	{ return true; }

	// Allow to specify custom session class
	virtual FDisplayClusterSessionBase* CreateSession(FSocket* InSock, const FIPv4Endpoint& InEP) = 0;

private:
	// Handles incoming connections
	bool ConnectionHandler(FSocket* InSock, const FIPv4Endpoint& InEP);

private:
	// Server data
	const FString Name;
	const FString Address;
	const int32   Port;

	// Simple server state
	bool bIsRunning = false;
	// Socket listener
	FDisplayClusterTcpListener Listener;
	// Sync access
	FCriticalSection InternalsSyncScope;

	// Active sessions
	TArray<FDisplayClusterSessionBase*> Sessions;
};

