// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

#include "Network/DisplayClusterMessage.h"

class FJsonObject;


/**
 * Socket operations (base class for client and server)
 */
class FDisplayClusterSocketOps
{
public:
	FDisplayClusterSocketOps(FSocket* InSocket);
	virtual ~FDisplayClusterSocketOps();

protected:
	virtual bool SendMsg(const TSharedPtr<FDisplayClusterMessage>& Message);
	virtual TSharedPtr<FDisplayClusterMessage> RecvMsg();

	virtual bool SendJson(const TSharedPtr<FJsonObject>& Message);
	virtual TSharedPtr<FJsonObject> RecvJson();

protected:
	inline bool IsOpen() const
	{ return (Socket && (Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected)); }

	// Provides with net unit name
	virtual FString GetName() const = 0;

protected:
	// Provides with a synchronization object
	inline FCriticalSection& GetSyncObj() const
	{ return CritSecInternals; }

	inline FSocket* GetSocket() const
	{ return Socket; }

	bool RecvChunk(TArray<uint8>& ChunkBuffer, const int32 ChunkSize, const FString& ChunkName = FString("ReadDataChunk"));
	bool SendChunk(const TArray<uint8>& ChankBuffer, const int32 ChunkSize, const FString& ChunkName = FString("WriteDataChunk"));

private:
	struct FDisplayClusterMessageHeader
	{
		uint16 Length;

		FString ToString()
		{
			return FString::Printf(TEXT("<length=%u>"), Length);
		}
	};

	// Socket
	FSocket* Socket = nullptr;
	
	// Sync access to internals
	mutable FCriticalSection CritSecInternals;

	// Read/write buffer
	TArray<uint8> DataBuffer;
};
