// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackChannel/Transport/IBackChannelConnection.h"
#include "HAL/ThreadSafeBool.h"

class FSocket;

/**
* BackChannelClient implementation.
*
*/
class BACKCHANNEL_API FBackChannelConnection : public IBackChannelConnection, public TSharedFromThis<FBackChannelConnection>
{
public:

	FBackChannelConnection();
	~FBackChannelConnection();

	/* Start connecting to the specified port for incoming connections. Use WaitForConnection to check status. */
	virtual bool Connect(const TCHAR* InEndPoint) override;

	/* Start listening on the specified port for incoming connections. Use WaitForConnection to accept one. */
	virtual bool Listen(const int16 Port) override;

	/* Close the connection */
	virtual void Close() override;

	/* Waits for an icoming or outgoing connection to be made */
	virtual bool WaitForConnection(double InTimeout, TFunction<bool(TSharedRef<IBackChannelConnection>)> InDelegate) override;

	/* Attach this connection to the provided socket */
	bool Attach(FSocket* InSocket);

	/* Send data over our connection. The number of bytes sent is returned */
	virtual int32 SendData(const void* InData, const int32 InSize) override;

	/* Read data from our remote connection. The number of bytes received is returned */
	virtual int32 ReceiveData(void* OutBuffer, const int32 BufferSize) override;

	/* Return our current connection state */
	virtual bool IsConnected() const override;

	/* Returns true if this connection is currently listening for incoming connections */
	virtual bool IsListening() const override;

	/* Return a string describing this connection */
	virtual FString	GetDescription() const override;

	/* Return the underlying socket (if any) for this connection */
	virtual FSocket* GetSocket() override { return Socket; }

	/* Todo - Proper stats */
	uint32	GetPacketsReceived() const override;

private:

	void					CloseWithError(const TCHAR* Error, FSocket* InSocket=nullptr);

	FThreadSafeBool			IsAttemptingConnection;
	FCriticalSection		SocketMutex;
	FSocket*				Socket;
	bool					IsListener;
	uint32					PacketsReceived;
};
