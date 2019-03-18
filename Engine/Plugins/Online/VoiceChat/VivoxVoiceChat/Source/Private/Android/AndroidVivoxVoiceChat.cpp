// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AndroidVivoxVoiceChat.h"

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "VxcJNI.h" 
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

TUniquePtr<FVivoxVoiceChat> CreateVivoxObject()
{
	return MakeUnique<FAndroidVivoxVoiceChat>();
}

static bool bVivoxInitialized = false;

FAndroidVivoxVoiceChat::FAndroidVivoxVoiceChat()
	: bDisconnectInBackground(true)
	, bInBackground(false)
	, bShouldReconnect(false)
{
}

FAndroidVivoxVoiceChat::~FAndroidVivoxVoiceChat()
{
}

bool FAndroidVivoxVoiceChat::Initialize()
{
	if (!bVivoxInitialized)
	{
		// hopefully this is early enough; we don't have a way add into JNI_OnLoad in AndroidJNI.cpp
		vx_jni_set_java_vm(GJavaVM);

		// do not call any Vivox SDK functions before this
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			static jmethodID InitVivoxMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Vivox_Init", "()V", false);
			if (InitVivoxMethod == 0)
			{
				UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Failed to find AndroidThunkJava_Vivox_Init. Vivox voice chat will not work."));
				return false;
			}

			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, InitVivoxMethod);
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
				UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Exception encountered calling AndroidThunkJava_Vivox_Init. Vivox voice chat will not work."));
				return false;
			}

			bVivoxInitialized = true;
		}
		else
		{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Unable to get Java environment. Vivox voice chat will not work."));
			return false;
		}
	}

	bool bReturn = FVivoxVoiceChat::Initialize();
	if (bReturn)
	{
		GConfig->GetBool(TEXT("VoiceChat.Vivox"), TEXT("bDisconnectInBackground"), bDisconnectInBackground, GEngineIni);

		if (!ApplicationWillEnterBackgroundHandle.IsValid())
		{
			ApplicationWillEnterBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FAndroidVivoxVoiceChat::HandleApplicationWillEnterBackground);
		}
		if (!ApplicationDidEnterForegroundHandle.IsValid())
		{
			ApplicationDidEnterForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FAndroidVivoxVoiceChat::HandleApplicationHasEnteredForeground);
		}
	}

	bInBackground = false;
	bShouldReconnect = false;

	return bReturn;
}

bool FAndroidVivoxVoiceChat::Uninitialize()
{
	if (ApplicationWillEnterBackgroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(ApplicationWillEnterBackgroundHandle);
		ApplicationWillEnterBackgroundHandle.Reset();
	}
	if (ApplicationDidEnterForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ApplicationDidEnterForegroundHandle);
		ApplicationDidEnterForegroundHandle.Reset();
	}

	return FVivoxVoiceChat::Uninitialize();
}

void FAndroidVivoxVoiceChat::OnVoiceChatConnectComplete(const FVoiceChatResult& Result)
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

void FAndroidVivoxVoiceChat::OnVoiceChatDisconnectComplete(const FVoiceChatResult& Result)
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
}

void FAndroidVivoxVoiceChat::HandleApplicationWillEnterBackground()
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("OnApplicationWillEnterBackgroundDelegate"));

	bInBackground = true;

	if (IsConnected() && bDisconnectInBackground)
	{
		Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateRaw(this, &FAndroidVivoxVoiceChat::OnVoiceChatDisconnectComplete));
	}

	VivoxClientConnection.EnteredBackground();
}

void FAndroidVivoxVoiceChat::HandleApplicationHasEnteredForeground()
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("OnApplicationHasEnteredForegoundDelegate"));

	VivoxClientConnection.WillEnterForeground();

	bInBackground = false;

	if (bShouldReconnect)
	{
		Reconnect();
	}
}

void FAndroidVivoxVoiceChat::Reconnect()
{
	Connect(FOnVoiceChatConnectCompleteDelegate::CreateRaw(this, &FAndroidVivoxVoiceChat::OnVoiceChatConnectComplete));
	bShouldReconnect = false;
}
