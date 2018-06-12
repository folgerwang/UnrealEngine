// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "DisplayClusterMessage.h"


/**
 * Socket operations (base class for client and server)
 */
class FDisplayClusterSocketOps
{
public:
	FDisplayClusterSocketOps(FSocket* pSock);
	virtual ~FDisplayClusterSocketOps();

public:
	virtual bool SendMsg(const FDisplayClusterMessage::Ptr& msg);
	virtual FDisplayClusterMessage::Ptr RecvMsg();

	inline FSocket* GetSocket() const
	{ return Socket; }

	inline bool IsOpen() const
	{ return (Socket && (Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected)); }

	// Provides with net unit name
	virtual FString GetName() const = 0;

protected:
	// Provides with a synchronization object for underlying operations (message send/recv)
	inline FCriticalSection& GetSyncObj() const
	{ return InternalsSyncScope; }

private:
	bool RecvChunk(int32 chunkSize, TArray<uint8>& chunkBuffer, const FString& chunkName = FString("DataChunk"));

private:
	struct FDisplayClusterMessageHeader
	{
		int16 length;

		FString ToString()
		{ return FString::Printf(TEXT("<length=%d>"), length); }

	};

private:
	// Socket
	FSocket* Socket = nullptr;
	// Data buffer for incoming and outgoing messages
	TArray<uint8> DataBuffer;
	// Access sync object
	mutable FCriticalSection InternalsSyncScope;
};

