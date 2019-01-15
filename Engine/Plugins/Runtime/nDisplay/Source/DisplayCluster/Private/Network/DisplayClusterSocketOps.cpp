// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/DisplayClusterSocketOps.h"

#include "DisplayClusterConstants.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#include "SocketSubsystem.h"
#include "Containers/UnrealString.h"


FDisplayClusterSocketOps::FDisplayClusterSocketOps(FSocket* InSocket) :
	Socket(InSocket)
{
	DataBuffer.Reserve(DisplayClusterConstants::net::MessageBufferSize);
}


FDisplayClusterSocketOps::~FDisplayClusterSocketOps()
{
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
}


bool FDisplayClusterSocketOps::SendMsg(const TSharedPtr<FDisplayClusterMessage>& Message)
{
	FScopeLock lock(&GetSyncObj());

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - sending message: %s"), *GetName(), *Message->ToString());

	if (!IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s not connected"), *GetName());
		return false;
	}

	// Prepare the data buffer
	DataBuffer.Empty(DisplayClusterConstants::net::MessageBufferSize);
	DataBuffer.AddZeroed(sizeof(FDisplayClusterMessageHeader));
	FMemoryWriter MemoryWriter(DataBuffer);

	// Reserve space for message header
	MemoryWriter.Seek(sizeof(FDisplayClusterMessageHeader));

	// Serialize the message body
	if (!Message->Serialize(MemoryWriter))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s couldn't serialize a message"), *GetName());
		return false;
	}

	// Initialize the message header
	FDisplayClusterMessageHeader MessageHeader;
	const int32 MessageLength = DataBuffer.Num();
	MessageHeader.Length = static_cast<uint16>(MessageLength & 0xFFFF) - sizeof(FDisplayClusterMessageHeader);
	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("Outgoing message body length %d"), MessageHeader.Length);

	// Fill packet header with message data length
	FMemory::Memcpy(DataBuffer.GetData(), &MessageHeader, sizeof(FDisplayClusterMessageHeader));

	// Send the header
	if (!SendChunk(DataBuffer, MessageLength, FString("send-msg")))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("Couldn't send a message"));
		return false;
	}

	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("Message sent"));

	return true;
}

TSharedPtr<FDisplayClusterMessage> FDisplayClusterSocketOps::RecvMsg()
{
	FScopeLock lock(&GetSyncObj());

	if (!IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - not connected"), *GetName());
		return nullptr;
	}

	// Read message header
	if (!RecvChunk(DataBuffer, sizeof(FDisplayClusterMessageHeader), FString("recv-msg-chunk-header")))
	{
		return nullptr;
	}

	// Ok. Now we can extract header data
	FDisplayClusterMessageHeader MessageHeader;
	FMemory::Memcpy(&MessageHeader, DataBuffer.GetData(), sizeof(FDisplayClusterMessageHeader));

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - message header received: %s"), *GetName(), *MessageHeader.ToString());
	check(MessageHeader.Length > 0);

	// Read message body
	if (!RecvChunk(DataBuffer, MessageHeader.Length, FString("recv-msg-chunk-body")))
	{
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - message body received"), *GetName());

	// Deserialize message from buffer
	TSharedPtr<FDisplayClusterMessage> Message(new FDisplayClusterMessage());
	FMemoryReader ar = FMemoryReader(DataBuffer, false);
	if (!Message->Deserialize(ar))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s couldn't deserialize a message"), *GetName());
		return nullptr;
	}

	// Succeeded
	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("%s - received a message: %s"), *GetName(), *Message->ToString());
	return Message;
}

bool FDisplayClusterSocketOps::SendJson(const TSharedPtr<FJsonObject>& Message)
{
	FScopeLock lock(&GetSyncObj());

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - sending json"), *GetName());

	if (!IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s not connected"), *GetName());
		return false;
	}

	// Prepare the buffer
	DataBuffer.Empty(DisplayClusterConstants::net::MessageBufferSize);

	// Serialize the message
	FString OutputString;
	TSharedRef< TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Message.ToSharedRef(), JsonWriter);
	const int MessageLength = OutputString.Len();
	FMemory::Memcpy(DataBuffer.GetData() + sizeof(FDisplayClusterMessageHeader), TCHAR_TO_ANSI(*OutputString), MessageLength);

	// Initialize message header
	FDisplayClusterMessageHeader MessageHeader;
	MessageHeader.Length = static_cast<uint16>(MessageLength & 0xFFFF);
	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("Outgoing json body length %d"), MessageHeader.Length);

	// Fill packet header with message data length
	FMemory::Memcpy(DataBuffer.GetData(), &MessageHeader, sizeof(FDisplayClusterMessageHeader));

	// Send message
	if (SendChunk(DataBuffer, MessageLength + sizeof(FDisplayClusterMessageHeader), FString("send-json")))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("Json sent"));
	}
	else
	{
		// We don't care if send operation failed. A customer may not receive any responses or remote socket already closed.
		UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("Couldn't send header chunk"));
	}

	return true;
}

TSharedPtr<FJsonObject> FDisplayClusterSocketOps::RecvJson()
{
	FScopeLock lock(&GetSyncObj());

	if (!IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - not connected"), *GetName());
		return nullptr;
	}

	// Read message header
	if (!RecvChunk(DataBuffer, sizeof(FDisplayClusterMessageHeader), FString("recv-json-chunk-header")))
	{
		return nullptr;
	}

	// Ok. Now we can extract header data
	FDisplayClusterMessageHeader MessageHeader;
	FMemory::Memcpy(&MessageHeader, DataBuffer.GetData(), sizeof(FDisplayClusterMessageHeader));

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - json header received: %s"), *GetName(), *MessageHeader.ToString());
	check(MessageHeader.Length > 0);

	// Read message body
	if (!RecvChunk(DataBuffer, MessageHeader.Length, FString("recv-json-chunk-body")))
	{
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - json body received"), *GetName());

	// I couldn't make BytesToString work so I manually copy the data and put zero-terminator in the end. This approach works.
	const int MessageLength = DataBuffer.Num();
	TUniquePtr<char> CharBuffer = TUniquePtr<char>(new char[MessageLength + 1]);
	FMemory::Memcpy(CharBuffer.Get(), DataBuffer.GetData(), MessageLength);
	CharBuffer.Get()[MessageLength] = 0;
	const FString InputString(CharBuffer.Get());
	TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(InputString);

	// Now we deserialize a string to Json object
	TSharedPtr<FJsonObject> Message(new FJsonObject());
	if (!FJsonSerializer::Deserialize(JsonReader, Message))
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s couldn't deserialize a message"), *GetName());
		return nullptr;
	}

	// Succeeded
	UE_LOG(LogDisplayClusterNetworkMsg, Verbose, TEXT("%s - received a json message: %s"), *GetName(), *InputString);
	return Message;
}

bool FDisplayClusterSocketOps::RecvChunk(TArray<uint8>& ChunkBuffer, const int32 ChunkSize, const FString& ChunkName)
{
	int32 bytesReadAll = 0;
	int32 bytesReadNow = 0;
	int32 bytesReadLeft = 0;
	const int32 bytesAll = ChunkSize;
	ChunkBuffer.Empty(DisplayClusterConstants::net::MessageBufferSize);

	// Receive message header at first
	while (bytesReadAll < bytesAll)
	{
		// Read data
		bytesReadLeft = bytesAll - bytesReadAll;
		if (!Socket->Recv(ChunkBuffer.GetData() + bytesReadAll, bytesReadLeft, bytesReadNow))
		{
			UE_LOG(LogDisplayClusterNetwork, Log, TEXT("%s - %s recv failed. It seems the client has disconnected."), *GetName(), *ChunkName);
			return false;
		}

		// Check amount of read data
		if (bytesReadNow <= 0 || bytesReadNow > bytesReadLeft)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - %s recv failed - read wrong amount of bytes: %d"), *GetName(), *ChunkName, bytesReadNow);
			return false;
		}

		bytesReadAll += bytesReadNow;
		UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - %s received %d bytes, left %d bytes"), *GetName(), *ChunkName, bytesReadNow, bytesAll - bytesReadAll);

		// Convergence check
		if (bytesReadAll > bytesAll)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - %s convergence fail: overall received %d of %d"), *GetName(), *ChunkName, bytesReadAll, bytesAll);
			return false;
		}
	}

	// Update array length (amount of bytes as array elements)
	ChunkBuffer.SetNumUninitialized(bytesReadAll);

	// Operation succeeded
	return true;
}

bool FDisplayClusterSocketOps::SendChunk(const TArray<uint8>& ChunkBuffer, const int32 ChunkSize, const FString& ChunkName)
{
	int32 BytesWriteAll = 0;
	int32 BytesWriteNow = 0;
	int32 BytesWriteLeft = 0;

	while (BytesWriteAll < ChunkSize)
	{
		BytesWriteLeft = ChunkSize - BytesWriteAll;
		
		// Send data
		if (!Socket->Send(ChunkBuffer.GetData() + BytesWriteAll, BytesWriteLeft, BytesWriteNow))
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - couldn't send a message (length=%d)"), *GetName(), ChunkSize);
			return false;
		}

		// Check amount of sent bytes
		if (BytesWriteNow <= 0 || BytesWriteNow > BytesWriteLeft)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - sent wrong amount of bytes: %d of %d left"), *GetName(), BytesWriteNow, BytesWriteLeft);
			return false;
		}

		BytesWriteAll += BytesWriteNow;
		UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - sent %d bytes, left %d bytes"), *GetName(), BytesWriteNow, ChunkSize - BytesWriteAll);

		// Convergence check
		if (BytesWriteAll > ChunkSize)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - convergence failed: overall sent %d of %d"), *GetName(), BytesWriteAll, ChunkSize);
			return false;
		}
	}

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - message sent"), *GetName());

	return true;
}
