// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSocketOps.h"

#include "DisplayClusterConstants.h"
#include "SocketSubsystem.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterSocketOps::FDisplayClusterSocketOps(FSocket* pSock) :
	Socket(pSock)
{
	DataBuffer.Reserve(DisplayClusterConstants::net::MessageBufferSize);
}


FDisplayClusterSocketOps::~FDisplayClusterSocketOps()
{
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
}

FDisplayClusterMessage::Ptr FDisplayClusterSocketOps::RecvMsg()
{
	FScopeLock lock(&GetSyncObj());

	if (!IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - not connected"), *GetName());
		return nullptr;
	}

	// Read message header
	if (!RecvChunk(sizeof(FDisplayClusterMessageHeader), DataBuffer, FString("header-chunk")))
	{
		return nullptr;
	}

	// Ok. Now we can extract header data
	FDisplayClusterMessageHeader msgHeader;
	FMemory::Memcpy(&msgHeader, DataBuffer.GetData(), sizeof(FDisplayClusterMessageHeader));

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - message header received: %s"), *GetName(), *msgHeader.ToString());
	check(msgHeader.length > 0);

	// Read message body
	if (!RecvChunk(msgHeader.length, DataBuffer, FString("body-chunk")))
	{
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - message body received"), *GetName());

	FDisplayClusterMessage::Ptr msg(new FDisplayClusterMessage());
	FMemoryReader ar = FMemoryReader(DataBuffer, false);

	// Deserialize message from buffer
	if (!msg->Deserialize(ar))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s couldn't deserialize a message"), *GetName());
		return nullptr;
	}

	// Succeeded
	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("%s - received a message: %s"), *GetName(), *msg->ToString());
	return msg;
}

bool FDisplayClusterSocketOps::RecvChunk(int32 chunkSize, TArray<uint8>& chunkBuffer, const FString& chunkName)
{
	int32 bytesReadAll = 0;
	int32 bytesReadNow = 0;
	int32 bytesReadLeft = 0;
	const int32 bytesAll = chunkSize;
	chunkBuffer.Empty(DisplayClusterConstants::net::MessageBufferSize);

	// Receive message header at first
	while (bytesReadAll < bytesAll)
	{
		// Read data
		bytesReadLeft = bytesAll - bytesReadAll;
		if (!Socket->Recv(chunkBuffer.GetData(), bytesReadLeft, bytesReadNow))
		{
			UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s - %s recv failed - socket error. Cluster integrity disturbed."), *GetName(), *chunkName);
			return false;
		}

		// Check amount of read data
		if (bytesReadNow <= 0 || bytesReadNow > bytesReadLeft)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - %s recv failed - read wrong amount of bytes: %d"), *GetName(), *chunkName, bytesReadNow);
			return false;
		}

		bytesReadAll += bytesReadNow;
		UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - %s received %d bytes, left %d bytes"), *GetName(), *chunkName, bytesReadNow, bytesAll - bytesReadAll);

		// Convergence check
		if (bytesReadAll > bytesAll)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - %s convergence fail: overall received %d of %d"), *GetName(), *chunkName, bytesReadAll, bytesAll);
			return false;
		}
	}

	// Update array length (amount of bytes as array elements)
	chunkBuffer.SetNumUninitialized(bytesReadAll);

	// Operation succeeded
	return true;
}

bool FDisplayClusterSocketOps::SendMsg(const FDisplayClusterMessage::Ptr& msg)
{
	FScopeLock lock(&GetSyncObj());

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - sending message: %s"), *GetName(), *msg->ToString());

	if (!IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s not connected"), *GetName());
		return false;
	}

	// Prepare output buffer
	DataBuffer.Empty(DisplayClusterConstants::net::MessageBufferSize);
	DataBuffer.AddZeroed(sizeof(FDisplayClusterMessageHeader));
	FMemoryWriter memoryWriter(DataBuffer);

	// Reserve space for message header
	memoryWriter.Seek(sizeof(FDisplayClusterMessageHeader));

	// Serialize the message
	if (!msg->Serialize(memoryWriter))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s couldn't serialize a message"), *GetName());
		return false;
	}

	// Check bounds
	const int32 msgLength = DataBuffer.Num();
	if (msgLength > DisplayClusterConstants::net::SocketBufferSize)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("Outgoing message length exceeds buffer limit: length=%d > limit=%d"), msgLength, DisplayClusterConstants::net::SocketBufferSize);
		return false;
	}

	// Initialize message header
	FDisplayClusterMessageHeader msgHeader;
	msgHeader.length = static_cast<int16>(msgLength & 0x7FFF) - sizeof(FDisplayClusterMessageHeader);
	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("Outgoing message body length %d"), msgHeader.length);

	// Fill packet header with message data length
	FMemory::Memcpy(DataBuffer.GetData(), &msgHeader, sizeof(FDisplayClusterMessageHeader));

	int32 bytesWriteAll  = 0;
	int32 bytesWriteNow  = 0;
	int32 bytesWriteLeft = 0;

	while (bytesWriteAll < msgLength)
	{
		bytesWriteLeft = msgLength - bytesWriteAll;

		// Send data
		if (!Socket->Send(DataBuffer.GetData() + bytesWriteAll, bytesWriteLeft, bytesWriteNow))
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - couldn't send a message (length=%d)"), *GetName(), msgLength);
			return false;
		}

		// Check amount of sent bytes
		if (bytesWriteNow <= 0 || bytesWriteNow > bytesWriteLeft)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - sent wrong amount of bytes: %d of %d left"), *GetName(), bytesWriteNow, bytesWriteLeft);
			return false;
		}

		bytesWriteAll += bytesWriteNow;
		UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - sent %d bytes, left %d bytes"), *GetName(), bytesWriteNow, msgLength - bytesWriteAll);

		// Convergence check
		if (bytesWriteAll > msgLength)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - convergence failed: overall sent %d of %d"), *GetName(), bytesWriteAll, msgLength);
			return false;
		}
	}

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - message sent"), *GetName());

	return true;
}

