// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/ThreadSafeBool.h"
#include "Misc/ScopeLock.h"

#include "Utils.h"

class FStreamer;
class FPixelStreamingInputDevice;
class FSocket;

// encapsulates TCP connection to WebRTC Proxy
// accepts a single connection from WebRTC Proxy, in a loop, accepts a new one once the previous disconnected
// allows sending data to the connection
// runs an internal thread for receiving data, deserialises "Proxy -> UE4" protocol messages and calls 
// appropriate handlers from that internal thread
class FProxyConnection final
{
private:
	FProxyConnection(const FProxyConnection&) = delete;
	FProxyConnection& operator=(const FProxyConnection&) = delete;

public:
	FProxyConnection(const FString& IP, uint16 Port, FStreamer& Streamer);
	~FProxyConnection();

	void Run(const FString& IP, uint16 Port);
	bool Send(const uint8* Data, uint32 Size);

private:
	bool AcceptConnection(const FString& IP, uint16 Port);
	void DestroyConnection();
	
	void InitReceiveHandlers();
	void Receive();

private:
	FStreamer& Streamer;
	FPixelStreamingInputDevice& InputDevice;

	// socket obj and its ptr is modified only from the internal thread but is used from an external thread
	// to send data. This lock protects sending to the socket to avoid concurrent modification. 
	// It's not needed for receiving from the socket because it happens in the same thread as modifications.
	FCriticalSection SocketMt;
	FSocket* Socket;

	FCriticalSection ListenerMt;
	FSocket* Listener;

	// handlers for different type of messages received from network
	TArray<TFunction<bool()>> ReceiveHandlers;

	FThreadSafeBool ExitRequested;
	// should be the last thing declared, otherwise the thread func can access other members that are not
	// initialised yet
	FThread Thread;
};

