// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterMessage.h"
#include "DisplayClusterSocketOps.h"

#include "DisplayClusterConstants.h"


/**
 * TCP client
 */
class FDisplayClusterClient
	: protected FDisplayClusterSocketOps
{
public:
	FDisplayClusterClient(const FString& name);
	virtual ~FDisplayClusterClient();

public:
	// Connects to a server
	bool Connect(const FString& addr, const int32 port, const int32 triesAmount = DisplayClusterConstants::net::ClientConnectTriesAmount, const float delay = DisplayClusterConstants::net::ClientConnectRetryDelay);
	// Terminates current connection
	void Disconnect();

	virtual bool SendMsg(const FDisplayClusterMessage::Ptr& msg) override final;
	virtual FDisplayClusterMessage::Ptr RecvMsg() override final;

	FDisplayClusterMessage::Ptr SendRecvMsg(const FDisplayClusterMessage::Ptr& msg);

	virtual FString GetName() const override final
	{ return Name; }

	inline bool IsConnected() const
	{ return IsOpen(); }

protected:
	// Creates client socket
	FSocket* CreateSocket(const FString& name, const int32 bufSize = DisplayClusterConstants::net::SocketBufferSize);

private:
	// Client name
	const FString Name;
};

