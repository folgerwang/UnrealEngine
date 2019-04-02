// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VivoxVoiceChat.h"

class FIOSVivoxVoiceChat : public FVivoxVoiceChat
{
public:
	FIOSVivoxVoiceChat();
	virtual ~FIOSVivoxVoiceChat();

	// ~Begin IVoiceChat Interface
	virtual bool Initialize() override;
	virtual bool Uninitialize() override;
	virtual FDelegateHandle StartRecording(const FOnVoiceChatRecordSamplesAvailableDelegate::FDelegate& Delegate) override;
	virtual void StopRecording(FDelegateHandle Handle) override;
	// ~End IVoiceChat Interface

protected:
	// ~Begin DebugClientApiEventHandler Interface
	virtual void onConnectCompleted(const VivoxClientApi::Uri& Server) override;
	virtual void onDisconnected(const VivoxClientApi::Uri& Server, const VivoxClientApi::VCSStatus& Status) override;
	// ~End DebugClientApiEventHandler Interface

private:
	void OnVoiceChatConnectComplete(const FVoiceChatResult& Result);
	void OnVoiceChatDisconnectComplete(const FVoiceChatResult& Result);

	void HandleApplicationWillEnterBackground();
	void HandleApplicationHasEnteredForeground();

	void Reconnect();

	FDelegateHandle ApplicationWillEnterBackgroundHandle;
	FDelegateHandle ApplicationDidEnterForegroundHandle;

	UIBackgroundTaskIdentifier BGTask;
	bool bDisconnectInBackground;
	bool bInBackground;
	bool bShouldReconnect;

	bool bIsRecording;
};
