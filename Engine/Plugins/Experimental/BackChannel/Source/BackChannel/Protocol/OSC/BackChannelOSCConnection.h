// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCDispatch.h"
#include "HAL/ThreadSafeBool.h"

class IBackChannelConnection;
class FBackChannelOSCPacket;

/**
 *	A class that wraps an existing BackChannel connection and provides an OSC-focused interface and
 *	a background thread. Incoming messages are received on a background thread and queued until 
 *	DispatchMessages() is called. Outgoing messages are sent immediately
 */
class BACKCHANNEL_API FBackChannelOSCConnection : FRunnable
{
public:

	FBackChannelOSCConnection(TSharedRef<IBackChannelConnection> InConnection);

	~FBackChannelOSCConnection();

public:

	bool StartReceiveThread();

	// Begin public FRunnable overrides
	virtual void Stop() override;
	// end public FRunnable overrides
	
	/* Returns our connection state as determined by the underlying BackChannel connection */
	bool IsConnected() const;

	/* Returns true if running in the background */
	bool IsThreaded() const;

	void ReceivePackets(const float MaxTime = 0);

	/* Send the provided OSC packet */
	bool SendPacket(FBackChannelOSCPacket& Packet);

	/* Bind a delegate to a message address */
	FDelegateHandle AddMessageHandler(const TCHAR* Path, FBackChannelDispatchDelegate::FDelegate Delegate);
	
	/* Remove a delegate handle */
	void RemoveMessageHandler(const TCHAR* Path, FDelegateHandle& InHandle);

	/* Set options for the specified message path */
	void SetMessageOptions(const TCHAR* Path, int32 MaxQueuedMessages);

	FString GetDescription();

protected:
	// Begin protected FRunnable overrides
	virtual uint32 Run() override;
	// End protected FRunnable overrides

	bool SendPacketData(const void* Data, const int32 DataLen);

	int32 GetMessageLimitForPath(const TCHAR* Path);

	int32 GetMessageCountForPath(const TCHAR* Path);

	void RemoveMessagesWithPath(const TCHAR* Path, const int32 Num = 0);

	void ReceiveData(const float MaxTime = 0);

	/* Dispatch all queued messages */
	void DispatchMessages();


protected:

	TSharedPtr<IBackChannelConnection>  Connection;

	FBackChannelOSCDispatch				DispatchMap;

	TArray<TSharedPtr<FBackChannelOSCPacket>> ReceivedPackets;

	TMap<FString, int32> MessageLimits;

	FThreadSafeBool		ExitRequested;
	FThreadSafeBool		IsRunning;

	FCriticalSection	ReceiveMutex;
	FCriticalSection	SendMutex;

	double				LastReceiveTime;
	double				LastSendTime;
	double				PingTime;
	bool				HasErrorState;

	int32				ReceivedDataSize;
	int32				ExpectedDataSize;
	TArray<uint8>		ReceiveBuffer;

};
