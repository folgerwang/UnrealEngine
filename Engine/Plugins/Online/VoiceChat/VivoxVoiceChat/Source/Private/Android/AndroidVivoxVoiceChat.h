// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VivoxVoiceChat.h"

class FAndroidVivoxVoiceChat : public FVivoxVoiceChat
{
public:
	FAndroidVivoxVoiceChat();
	virtual ~FAndroidVivoxVoiceChat();

	// ~Begin IVoiceChat Interface
	virtual bool Initialize() override;
	virtual bool Uninitialize() override;
	// ~End IVoiceChat Interface

private:
	void OnVoiceChatConnectComplete(const FVoiceChatResult& Result);
	void OnVoiceChatDisconnectComplete(const FVoiceChatResult& Result);

	void HandleApplicationWillEnterBackground();
	void HandleApplicationHasEnteredForeground();

	void Reconnect();

	FDelegateHandle ApplicationWillEnterBackgroundHandle;
	FDelegateHandle ApplicationDidEnterForegroundHandle;

	bool bDisconnectInBackground;
	bool bInBackground;
	bool bShouldReconnect;
};
