// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionRole.h"

class IBackChannelConnection;
class FRecordingMessageHandler;
class FFrameGrabber;
class IImageWrapper;
class FRemoteSessionInputChannel;

class FRemoteSessionHost : public FRemoteSessionRole, public TSharedFromThis<FRemoteSessionHost>
{
public:

	FRemoteSessionHost(int32 InQuality, int32 InFramerate);
	~FRemoteSessionHost();

	bool StartListening(const uint16 Port);

	void SetScreenSharing(const bool bEnabled);

	void SetConsumeInput(const bool bConsume);

	virtual void Tick(float DeltaTime) override;

protected:

	bool	ProcessIncomingConnection(TSharedRef<IBackChannelConnection> NewConnection);

	TSharedPtr<IBackChannelConnection> Listener;

	int32		Quality;
	int32		Framerate;
};