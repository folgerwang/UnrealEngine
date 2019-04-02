// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IOSVivoxVoiceChat.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "IOS/IOSAppDelegate.h"

TUniquePtr<FVivoxVoiceChat> CreateVivoxObject()
{
	return MakeUnique<FIOSVivoxVoiceChat>();
}

FIOSVivoxVoiceChat::FIOSVivoxVoiceChat()
	: BGTask(UIBackgroundTaskInvalid)
	, bDisconnectInBackground(true)
	, bInBackground(false)
	, bShouldReconnect(false)
	, bIsRecording(false)
{
}

FIOSVivoxVoiceChat::~FIOSVivoxVoiceChat()
{
}

bool FIOSVivoxVoiceChat::Initialize()
{
	bool bResult = FVivoxVoiceChat::Initialize();

	if (bResult)
	{
		GConfig->GetBool(TEXT("VoiceChat.Vivox"), TEXT("bDisconnectInBackground"), bDisconnectInBackground, GEngineIni);
		
		if (!ApplicationWillEnterBackgroundHandle.IsValid())
		{
			ApplicationWillEnterBackgroundHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FIOSVivoxVoiceChat::HandleApplicationWillEnterBackground);
		}
		if (!ApplicationDidEnterForegroundHandle.IsValid())
		{
			ApplicationDidEnterForegroundHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FIOSVivoxVoiceChat::HandleApplicationHasEnteredForeground);
		}
	}

	bInBackground = false;
	bShouldReconnect = false;
	bIsRecording = false;

	return bResult;
}

bool FIOSVivoxVoiceChat::Uninitialize()
{
	if (ApplicationWillEnterBackgroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(ApplicationWillEnterBackgroundHandle);
		ApplicationWillEnterBackgroundHandle.Reset();
	}
	if (ApplicationDidEnterForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(ApplicationDidEnterForegroundHandle);
		ApplicationDidEnterForegroundHandle.Reset();
	}

	return FVivoxVoiceChat::Uninitialize();
}

FDelegateHandle FIOSVivoxVoiceChat::StartRecording(const FOnVoiceChatRecordSamplesAvailableDelegate::FDelegate& Delegate)
{
	FPlatformMisc::EnableVoiceChat(true);
	return FVivoxVoiceChat::StartRecording(Delegate);
}

void FIOSVivoxVoiceChat::StopRecording(FDelegateHandle Handle)
{
	FVivoxVoiceChat::StopRecording(Handle);
	if (ConnectionState < EConnectionState::Connecting)
	{
		FPlatformMisc::EnableVoiceChat(false);
	}
}

void FIOSVivoxVoiceChat::onConnectCompleted(const VivoxClientApi::Uri& Server)
{
	FPlatformMisc::EnableVoiceChat(true);
	FVivoxVoiceChat::onConnectCompleted(Server);
}

void FIOSVivoxVoiceChat::onDisconnected(const VivoxClientApi::Uri& Server, const VivoxClientApi::VCSStatus& Status)
{
	FVivoxVoiceChat::onDisconnected(Server, Status);
	if (!bIsRecording)
	{
		FPlatformMisc::EnableVoiceChat(false);
	}
}

void FIOSVivoxVoiceChat::OnVoiceChatConnectComplete(const FVoiceChatResult& Result)
{
	if (Result.bSuccess)
	{
		OnVoiceChatReconnectedDelegate.Broadcast();
	}
	else
	{
		OnVoiceChatDisconnectedDelegate.Broadcast(Result);
	}
}

void FIOSVivoxVoiceChat::OnVoiceChatDisconnectComplete(const FVoiceChatResult& Result)
{
	if (bInBackground)
	{
		bShouldReconnect = true;
	}
	else if (IsInitialized())
	{
		// disconnect complete delegate fired after entering foreground
		Reconnect();
	}
	
	if (BGTask != UIBackgroundTaskInvalid)
	{
		UIApplication* App = [UIApplication sharedApplication];
		[App endBackgroundTask:BGTask];
		BGTask = UIBackgroundTaskInvalid;
	}
}

void FIOSVivoxVoiceChat::HandleApplicationWillEnterBackground()
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("OnApplicationWillEnterBackgroundDelegate"));

	if (IsConnected() && bDisconnectInBackground)
	{
		if (BGTask != UIBackgroundTaskInvalid)
		{
			UIApplication* App = [UIApplication sharedApplication];
			[App endBackgroundTask : BGTask];
			BGTask = UIBackgroundTaskInvalid;
		}
		UIApplication* App = [UIApplication sharedApplication];
		BGTask = [App beginBackgroundTaskWithName : @"VivoxDisconnect" expirationHandler : ^{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Disconnect operation never completed"));

			[App endBackgroundTask : BGTask];
			BGTask = UIBackgroundTaskInvalid;
		}];

		Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateRaw(this, &FIOSVivoxVoiceChat::OnVoiceChatDisconnectComplete));
	}
	else
	{
		bShouldReconnect = false;
	}

	VivoxClientConnection.EnteredBackground();
}

void FIOSVivoxVoiceChat::HandleApplicationHasEnteredForeground()
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("OnApplicationHasEnteredForegoundDelegate"));

	VivoxClientConnection.WillEnterForeground();

	if (BGTask != UIBackgroundTaskInvalid)
	{
		UIApplication* App = [UIApplication sharedApplication];
		[App endBackgroundTask : BGTask];
		BGTask = UIBackgroundTaskInvalid;
	}
	if (bShouldReconnect)
	{
		Reconnect();
	}
}

void FIOSVivoxVoiceChat::Reconnect()
{
	Connect(FOnVoiceChatConnectCompleteDelegate::CreateRaw(this, &FIOSVivoxVoiceChat::OnVoiceChatConnectComplete));
	bShouldReconnect = false;
}
