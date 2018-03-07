// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"
#include "MessageHandler/RecordingMessageHandler.h"

class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;

class REMOTESESSION_API FRemoteSessionInputChannel : public IRemoteSessionChannel, public IRecordingMessageHandlerWriter
{
public:

	FRemoteSessionInputChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection);

	~FRemoteSessionInputChannel();

	virtual void Tick(const float InDeltaTime) override;

	virtual void RecordMessage(const TCHAR* MsgName, const TArray<uint8>& Data) override;

	void OnRemoteMessage(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch & Dispatch);

	void SetPlaybackWindow(TWeakPtr<SWindow> InWindow, TWeakPtr<FSceneViewport> InViewport);

	static FString StaticType();
	virtual FString GetType() const override { return StaticType(); }

protected:

	TWeakPtr<FGenericApplicationMessageHandler> DefaultHandler;

	TSharedPtr<FRecordingMessageHandler> RecordingHandler;

	TSharedPtr<FRecordingMessageHandler> PlaybackHandler;

	TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> Connection;

	ERemoteSessionChannelMode Role;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle MessageCallbackHandle;
};