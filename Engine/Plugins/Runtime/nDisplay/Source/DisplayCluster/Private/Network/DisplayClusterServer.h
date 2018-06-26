// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterSessionListener.h"

#include "DisplayClusterSession.h"
#include "DisplayClusterTcpListener.h"


struct FIPv4Endpoint;


/**
 * TCP server
 */
class FDisplayClusterServer
	: public IDisplayClusterSessionListener
{
public:
	FDisplayClusterServer(const FString& name, const FString& addr, const int32 port);
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
	virtual void NotifySessionOpen(FDisplayClusterSession* pSession) override;
	virtual void NotifySessionClose(FDisplayClusterSession* pSession) override;
	virtual FDisplayClusterMessage::Ptr ProcessMessage(FDisplayClusterMessage::Ptr msg) = 0;

protected:
	// Ask concrete server implementation if connection is allowed
	virtual bool IsConnectionAllowed(FSocket* pSock, const FIPv4Endpoint& ep)
	{ return true; }

private:
	// Handles incoming connections
	bool ConnectionHandler(FSocket* pSock, const FIPv4Endpoint& ep);

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
	TArray<TUniquePtr<FDisplayClusterSession>> Sessions;
};

