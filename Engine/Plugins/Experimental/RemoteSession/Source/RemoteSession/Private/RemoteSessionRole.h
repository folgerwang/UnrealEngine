// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSession/RemoteSessionRole.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "Tickable.h"



class FRemoteSessionRole : public IRemoteSessionRole, FRunnable
{
public:

	virtual ~FRemoteSessionRole();

	virtual void Close();

	virtual bool IsConnected() const;

	virtual void Tick( float DeltaTime );

	virtual TSharedPtr<IRemoteSessionChannel> GetChannel(const FString& Type) override;

	void			SetReceiveInBackground(bool bValue);

protected:

	void			StartBackgroundThread();
	void			StopBackgroundThread();

	uint32			Run();

protected:
	
	TSharedPtr<IBackChannelConnection>	Connection;

	TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> OSCConnection;

	TArray<TSharedPtr<IRemoteSessionChannel>> Channels;
	
	FThreadSafeBool			ThreadExitRequested;
	FThreadSafeBool			ThreadRunning;

};