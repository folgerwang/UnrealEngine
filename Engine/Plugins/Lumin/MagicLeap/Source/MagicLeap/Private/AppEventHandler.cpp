// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AppEventHandler.h"
#include "AppFramework.h"
#include "Engine/Engine.h"
#include "MagicLeapHMD.h"

namespace MagicLeap
{
#if WITH_MLSDK
	const TCHAR* MLPrivilegeToString(MLPrivilegeID PrivilegeID)
	{
		#define PRIV_TO_STR_CASE(x) case x: { return UTF8_TO_TCHAR((#x)); }
		switch (PrivilegeID)
		{
		PRIV_TO_STR_CASE(MLPrivilegeID_Invalid)
		PRIV_TO_STR_CASE(MLPrivilegeID_AudioRecognizer)
		PRIV_TO_STR_CASE(MLPrivilegeID_BatteryInfo)
		PRIV_TO_STR_CASE(MLPrivilegeID_CameraCapture)
		PRIV_TO_STR_CASE(MLPrivilegeID_WorldReconstruction)
		PRIV_TO_STR_CASE(MLPrivilegeID_InAppPurchase)
		PRIV_TO_STR_CASE(MLPrivilegeID_AudioCaptureMic)
		PRIV_TO_STR_CASE(MLPrivilegeID_DrmCertificates)
		PRIV_TO_STR_CASE(MLPrivilegeID_Occlusion)
		PRIV_TO_STR_CASE(MLPrivilegeID_LowLatencyLightwear)
		PRIV_TO_STR_CASE(MLPrivilegeID_Internet)
		PRIV_TO_STR_CASE(MLPrivilegeID_IdentityRead)
		PRIV_TO_STR_CASE(MLPrivilegeID_BackgroundDownload)
		PRIV_TO_STR_CASE(MLPrivilegeID_BackgroundUpload)
		PRIV_TO_STR_CASE(MLPrivilegeID_MediaDrm)
		PRIV_TO_STR_CASE(MLPrivilegeID_Media)
		PRIV_TO_STR_CASE(MLPrivilegeID_MediaMetadata)
		PRIV_TO_STR_CASE(MLPrivilegeID_PowerInfo)
		PRIV_TO_STR_CASE(MLPrivilegeID_LocalAreaNetwork)
		PRIV_TO_STR_CASE(MLPrivilegeID_VoiceInput)
		PRIV_TO_STR_CASE(MLPrivilegeID_Documents)
		PRIV_TO_STR_CASE(MLPrivilegeID_ConnectBackgroundMusicService)
		PRIV_TO_STR_CASE(MLPrivilegeID_RegisterBackgroundMusicService)
		PRIV_TO_STR_CASE(MLPrivilegeID_PwFoundObjRead)
		PRIV_TO_STR_CASE(MLPrivilegeID_NormalNotificationsUsage)
		PRIV_TO_STR_CASE(MLPrivilegeID_MusicService)
		PRIV_TO_STR_CASE(MLPrivilegeID_ControllerPose)
		PRIV_TO_STR_CASE(MLPrivilegeID_ScreensProvider)
		PRIV_TO_STR_CASE(MLPrivilegeID_GesturesSubscribe)
		PRIV_TO_STR_CASE(MLPrivilegeID_GesturesConfig)
		default: UE_LOG(LogMagicLeap, Error, TEXT("Unmapped privilege %d"), static_cast<int32>(PrivilegeID));
		}

		return UTF8_TO_TCHAR("");
	}

	IAppEventHandler::IAppEventHandler(const TArray<MLPrivilegeID>& InRequiredPrivilegeIDs)
	: OnAppShutDownHandler(nullptr)
	, OnAppTickHandler(nullptr)
	, OnAppPauseHandler(nullptr)
	, OnAppResumeHandler(nullptr)
	, bAllPrivilegesInSync(false)
	, bWasSystemEnabledOnPause(false)
	{
		RequiredPrivileges.Reserve(InRequiredPrivilegeIDs.Num());
		for (MLPrivilegeID RequiredPrivilegeID : InRequiredPrivilegeIDs)
		{
			RequiredPrivileges.Add(RequiredPrivilegeID, FRequiredPrivilege(RequiredPrivilegeID));
		}

		FAppFramework::AddEventHandler(this);
	}
#endif // WITH_MLSDK

	IAppEventHandler::IAppEventHandler()
	: bAllPrivilegesInSync(true)
	, bWasSystemEnabledOnPause(false)
	{
		FAppFramework::AddEventHandler(this);
	}

	IAppEventHandler::~IAppEventHandler()
	{
		FAppFramework::RemoveEventHandler(this);
	}

#if WITH_MLSDK
	EPrivilegeState IAppEventHandler::GetPrivilegeStatus(MLPrivilegeID PrivilegeID, bool bBlocking/* = true*/)
	{
		FScopeLock Lock(&CriticalSection);
		FRequiredPrivilege& RequiredPrivilege = RequiredPrivileges[PrivilegeID];
		if (RequiredPrivilege.State == EPrivilegeState::NotYetRequested)
		{
			if (bBlocking)
			{
				MLResult Result = MLPrivilegesRequestPrivilege(PrivilegeID);
				switch (Result)
				{
				case MLPrivilegesResult_Granted:
				{
					RequiredPrivilege.State = EPrivilegeState::Granted;
					UE_LOG(LogMagicLeap, Log, TEXT("Privilege '%s' was granted."), MLPrivilegeToString(RequiredPrivilege.PrivilegeID));
				}
				break;
				case MLPrivilegesResult_Denied:
				{
					RequiredPrivilege.State = EPrivilegeState::Denied;
					UE_LOG(LogMagicLeap, Log, TEXT("Privilege '%s' was denied."), MLPrivilegeToString(RequiredPrivilege.PrivilegeID));
				}
				break;
				default:
				{
					UE_LOG(LogMagicLeap, Error, TEXT("MLPrivilegesRequestPrivilege() failed with error %s"), UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)));
				}
				}
			}
			else
			{
				MLPrivilegesRequestPrivilegeAsync(PrivilegeID, &RequiredPrivilege.PrivilegeRequest);
				RequiredPrivilege.State = EPrivilegeState::Pending;
			}
		}

		return RequiredPrivilege.State;
	}
#endif // WITH_MLSDK

	void IAppEventHandler::OnAppShutDown()
	{
		if (OnAppShutDownHandler)
		{
			OnAppShutDownHandler();
		}
	}

	void IAppEventHandler::OnAppTick()
	{
#if WITH_MLSDK
		FScopeLock Lock(&CriticalSection);
		if (bAllPrivilegesInSync)
		{
			return;
		}

		bAllPrivilegesInSync = true;

		for(auto& ItRequiredPrivilege : RequiredPrivileges)
		{
			FRequiredPrivilege& RequiredPrivilege = ItRequiredPrivilege.Value;
			if (RequiredPrivilege.State == EPrivilegeState::NotYetRequested)
			{
				bAllPrivilegesInSync = false;
				continue;
			}

			if (RequiredPrivilege.State == EPrivilegeState::Granted || RequiredPrivilege.State == EPrivilegeState::Denied)
			{
				continue;
			}

			if (RequiredPrivilege.State == EPrivilegeState::Pending)
			{
				MLResult Result = MLPrivilegesRequestPrivilegeTryGet(RequiredPrivilege.PrivilegeRequest);
				switch (Result)
				{
				case MLPrivilegesResult_Granted:
				{
					RequiredPrivilege.State = EPrivilegeState::Granted;
					UE_LOG(LogMagicLeap, Log, TEXT("Privilege '%s' was granted."), MLPrivilegeToString(RequiredPrivilege.PrivilegeID));
				}
				break;
				case MLPrivilegesResult_Denied:
				{
					RequiredPrivilege.State = EPrivilegeState::Denied;
					UE_LOG(LogMagicLeap, Log, TEXT("Privilege '%s' was denied."), MLPrivilegeToString(RequiredPrivilege.PrivilegeID));
				}
				break;
				case MLResult_Pending:
				{
					bAllPrivilegesInSync = false;
				}
				break;
				default:
				{
					bAllPrivilegesInSync = false;
					UE_LOG(LogMagicLeap, Error, TEXT("MLPrivilegesRequestPrivilegeTryGet() failed with error %s"), UTF8_TO_TCHAR(MLPrivilegesGetResultString(Result)));
				}
				}
			}
		}
#endif // WITH_MLSDK

		if (OnAppTickHandler)
		{
			OnAppTickHandler();
		}
	}

	void IAppEventHandler::OnAppPause()
	{
		if (OnAppPauseHandler)
		{
			OnAppPauseHandler();
		}
	}

	void IAppEventHandler::OnAppResume()
	{
#if WITH_MLSDK
		FScopeLock Lock(&CriticalSection);
		bAllPrivilegesInSync = false;
		for (auto& ItRequiredPrivilege : RequiredPrivileges)
		{
			ItRequiredPrivilege.Value.State = EPrivilegeState::NotYetRequested;
		}
#endif // WITH_MLSDK

		if (OnAppResumeHandler)
		{
			OnAppResumeHandler();
		}
	}

	bool IAppEventHandler::AsyncDestroy()
	{
		return FAppFramework::AsyncDestroy(this);
	}
}