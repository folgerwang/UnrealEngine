// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "HAL/Runnable.h"
#include "Delegates/DelegateCombinations.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "DisplayClusterConstants.h"


/**
 * TCP connection listener
 */
class FDisplayClusterTcpListener
	: public FRunnable
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, TOnConnectionAcceptedDelegate, FSocket*, const FIPv4Endpoint&)

public:
	FDisplayClusterTcpListener(const FString& InName);
	~FDisplayClusterTcpListener();

public:

	bool StartListening(const FString& InAddr, const int32 InPort);
	bool StartListening(const FIPv4Endpoint& InEP);
	void StopListening();

	bool IsActive() const;

	inline TOnConnectionAcceptedDelegate& OnConnectionAccepted()
	{ return OnConnectionAcceptedDelegate; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRunnable
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	// Socket name
	FString Name;
	// Listening socket
	FSocket* SocketObj = nullptr;
	// Listening endpoint
	FIPv4Endpoint Endpoint;
	// Holds the thread object
	FRunnableThread* ThreadObj;
	// Sync access
	FCriticalSection InternalsSyncScope;
	// Listening state
	bool bIsListening = false;

private:
	// Holds a delegate to be invoked when an incoming connection has been accepted.
	TOnConnectionAcceptedDelegate OnConnectionAcceptedDelegate;
};
