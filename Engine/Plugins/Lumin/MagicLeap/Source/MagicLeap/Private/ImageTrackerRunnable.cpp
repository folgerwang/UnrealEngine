// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "ImageTrackerRunnable.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN

#if WITH_MLSDK
ML_INCLUDES_START
//#include <ml_image_tracking.h>
#include <ml_perception.h>
#include <ml_snapshot.h>
#include <ml_head_tracking.h>
#include <ml_coordinate_frame_uid.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

FImageTrackerRunnable::FImageTrackerRunnable()
#if WITH_MLSDK
: MagicLeap::IAppEventHandler({ MLPrivilegeID_CameraCapture })
, ImageTracker(ML_INVALID_HANDLE)
, Thread(nullptr)
#else
: Thread(nullptr)
#endif //WITH_MLSDK
, StopTaskCounter(0)
, RetryCreateTrackerWaitTime(0.5f)
{
#if WITH_MLSDK
	{
		FScopeLock Lock(&SettingsMutex);
		MLResult Result = MLImageTrackerInitSettings(&Settings);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("MLImageTrackerInitSettings failed with error %d."), Result);
	}
#endif //WITH_MLSDK
#if PLATFORM_LUMIN
	Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
	Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
}

FImageTrackerRunnable::~FImageTrackerRunnable()
{
	StopTaskCounter.Increment();

	if (nullptr != Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

#if WITH_MLSDK
MLHandle FImageTrackerRunnable::GetHandle() const
{
	return ImageTracker;
}
#endif //WITH_MLSDK

void FImageTrackerRunnable::SetEnabled(bool bEnable)
{
#if WITH_MLSDK
	FScopeLock Lock(&SettingsMutex);
	if (bEnable != Settings.enable_image_tracking)
	{
		Settings.enable_image_tracking = bEnable;
		FTrackerMessage UpdateSettingsMsg;
		UpdateSettingsMsg.TaskType = FTrackerMessage::TaskType::UpdateSettings;
		IncomingMessages.Enqueue(UpdateSettingsMsg);
	}
#endif //WITH_MLSDK
}

bool FImageTrackerRunnable::GetEnabled()
{
#if WITH_MLSDK
	FScopeLock Lock(&SettingsMutex);
	return Settings.enable_image_tracking;
#else
	return false;
#endif // WITH_MLSDK
}

void FImageTrackerRunnable::SetMaxSimultaneousTargets(int32 MaxTargets)
{
#if WITH_MLSDK
	FScopeLock Lock(&SettingsMutex);
	int32 ValidMaxTargets = MaxTargets < 1 ? 1 : MaxTargets;
	if (ValidMaxTargets != Settings.max_simultaneous_targets)
	{
		Settings.max_simultaneous_targets = static_cast<uint32>(ValidMaxTargets);
		FTrackerMessage UpdateSettingsMsg;
		UpdateSettingsMsg.TaskType = FTrackerMessage::TaskType::UpdateSettings;
		IncomingMessages.Enqueue(UpdateSettingsMsg);
	}
#endif //WITH_MLSDK
}

int32 FImageTrackerRunnable::GetMaxSimultaneousTargets()
{
#if WITH_MLSDK
	FScopeLock Lock(&SettingsMutex);
	return Settings.max_simultaneous_targets;
#else
	return 0;
#endif // WITH_MLSDK
}

uint32 FImageTrackerRunnable::Run()
{
	while (StopTaskCounter.GetValue() == 0)
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(ImageTracker))
		{
			if (GetPrivilegeStatus(MLPrivilegeID_CameraCapture) == MagicLeap::EPrivilegeState::Granted)
			{
				UE_LOG(LogMagicLeap, Display, TEXT("[FImageTrackerRunnable] Attempting to create image tracker."));
				FScopeLock Lock(&SettingsMutex);
				MLResult Result = MLImageTrackerCreate(&Settings, &ImageTracker);
				if (Result != MLResult_Ok)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("MLImageTrackerCreate failed with error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
					FPlatformProcess::Sleep(RetryCreateTrackerWaitTime);
				}
			}
			else
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerRunnable] Failed to create image tracker due to lack of privilege!"));
			}
		}
		else if (IncomingMessages.Dequeue(CurrentMessage))
		{
			switch (CurrentMessage.TaskType)
			{
			case FTrackerMessage::None: checkf(false, TEXT("Invalid incoming task 'FTrackerMessage::None'!"));	break;
			case FTrackerMessage::Pause: TryPause(); break;
			case FTrackerMessage::Resume: TryResume(); break;
			case FTrackerMessage::UpdateSettings: UpdateTrackerSettings(); break;
			case FTrackerMessage::TryCreateTarget: SetTarget(); break;
			case FTrackerMessage::TargetCreateFailed: checkf(false, TEXT("Invalid incoming task 'FTrackerMessage::TargetCreateFailed'!"));	break;
			case FTrackerMessage::TargetCreateSucceeded: checkf(false, TEXT("Invalid incoming task 'FTrackerMessage::TargetCreateSucceeded'!"));	break;
			}
		}
#endif //WITH_MLSDK
		FPlatformProcess::Sleep(0.5f);
	}

	return 0;
}

void FImageTrackerRunnable::OnAppPause()
{
	IAppEventHandler::OnAppPause();
	FTrackerMessage PauseMsg;
	PauseMsg.TaskType = FTrackerMessage::TaskType::Pause;
	IncomingMessages.Enqueue(PauseMsg);
}

void FImageTrackerRunnable::OnAppResume()
{
	IAppEventHandler::OnAppResume();
	FTrackerMessage ResumeMsg;
	ResumeMsg.TaskType = FTrackerMessage::TaskType::Resume;
	IncomingMessages.Enqueue(ResumeMsg);
}

void FImageTrackerRunnable::TryPause()
{
#if WITH_MLSDK
	{
		FScopeLock Lock(&SettingsMutex);
		bWasSystemEnabledOnPause = Settings.enable_image_tracking;
	}

	if (!bWasSystemEnabledOnPause)
	{
		UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Image tracking was not enabled at time of application pause."));
	}
	else
	{
		if (!MLHandleIsValid(ImageTracker))
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Image tracker was invalid on application pause."));
		}
		else
		{
			FScopeLock Lock(&SettingsMutex);
			Settings.enable_image_tracking = false;
			MLResult Result = MLImageTrackerUpdateSettings(ImageTracker, &Settings);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Failed to disable image tracker on application pause due to error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
			}
			else
			{
				UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Image tracker paused until app resumes."));
			}
		}
	}
#endif //WITH_MLSDK
}

void FImageTrackerRunnable::TryResume()
{
#if WITH_MLSDK
	if (!bWasSystemEnabledOnPause)
	{
		UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerRunnable] Not resuming image tracker as it was not enabled at time of application pause."));
	}
	else
	{
		if (!MLHandleIsValid(ImageTracker))
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerRunnable] Image tracker was invalid on application resume."));
		}
		else 
		{
			{
				FScopeLock Lock(&SettingsMutex);
				Settings.enable_image_tracking = true;
			}

			if (GetPrivilegeStatus(MLPrivilegeID_CameraCapture) == MagicLeap::EPrivilegeState::Granted)
			{
				FScopeLock Lock(&SettingsMutex);
				MLResult Result = MLImageTrackerUpdateSettings(ImageTracker, &Settings);
				if (Result != MLResult_Ok)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerRunnable] Failed to re-enable image tracker on application resume due to error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				}
				else
				{
					UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerRunnable] Image tracker re-enabled on application resume."));
				}
			}
			else
			{
				UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerRunnable] Image tracking failed to resume due to lack of privilege!"));
			}
		}
	}
#endif //WITH_MLSDK
}

void FImageTrackerRunnable::OnAppShutDown()
{
#if WITH_MLSDK
	if (MLHandleIsValid(ImageTracker))
	{
		MLResult Result = MLImageTrackerDestroy(ImageTracker);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("MLImageTrackerDestroy failed with error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
		ImageTracker = ML_INVALID_HANDLE;
	}
#endif //WITH_MLSDK
}

void FImageTrackerRunnable::SetTarget()
{
#if WITH_MLSDK
	if (MLHandleIsValid(CurrentMessage.PrevTarget))
	{
		MLResult Result = MLImageTrackerRemoveTarget(ImageTracker, CurrentMessage.PrevTarget);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("MLImageTrackerRemoveTarget failed with error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
	}

	FTrackerMessage TargetCreateMsg;
	TargetCreateMsg.Requester = CurrentMessage.Requester;

	CurrentMessage.TargetSettings.name = TCHAR_TO_UTF8(*CurrentMessage.TargetName);
	TArray<uint8*> MipPointers;
	MipPointers.AddZeroed(CurrentMessage.TargetImageTexture->GetNumMips());
	CurrentMessage.TargetImageTexture->GetMipData(0, (void**)MipPointers.GetData());
	const int32 ImageWidth = CurrentMessage.TargetImageTexture->GetSizeX();
	const int32 ImageHeight = CurrentMessage.TargetImageTexture->GetSizeY();
	MLHandle Target = ML_INVALID_HANDLE;
	if (MipPointers.Num() > 0 && MipPointers[0] != nullptr)
	{
		MLResult Result = MLImageTrackerAddTargetFromArray(
			ImageTracker,
			&CurrentMessage.TargetSettings,
			MipPointers[0],
			ImageWidth,
			ImageHeight,
			MLImageTrackerImageFormat_RGBA,
			&Target);

		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLImageTrackerAddTargetFromArray for %s failed with error %s."), *CurrentMessage.TargetName, UTF8_TO_TCHAR(MLGetResultString(Result)));
			TargetCreateMsg.TaskType = FTrackerMessage::TaskType::TargetCreateFailed;
			OutgoingMessages.Enqueue(TargetCreateMsg);
			return;
		}

		// [3] Cache all the static data for this target.
		MLImageTrackerTargetStaticData Data;
		FMemory::Memset(&Data, 0, sizeof(MLImageTrackerTargetStaticData));
		Result = MLImageTrackerGetTargetStaticData(ImageTracker, Target, &Data);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLImageTrackerGetTargetStaticData failed with error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
			TargetCreateMsg.TaskType = FTrackerMessage::TaskType::TargetCreateFailed;
			OutgoingMessages.Enqueue(TargetCreateMsg);
			return;
		}

		TargetCreateMsg.TaskType = FTrackerMessage::TaskType::TargetCreateSucceeded;
		TargetCreateMsg.Target = Target;
		TargetCreateMsg.Data = Data;
		OutgoingMessages.Enqueue(TargetCreateMsg);
	}
	else
	{
		UE_LOG(LogMagicLeap, Error, TEXT("Failed to get texture bulk data for image target %s. Image will not be tracked."), *CurrentMessage.TargetName);
	}

	for (int32 i = 0; i < MipPointers.Num(); ++i)
	{
		if (MipPointers[i] != nullptr)
		{
			FMemory::Free(MipPointers[i]);
			MipPointers[i] = nullptr;
		}
	}
#endif //WITH_MLSDK
}

void FImageTrackerRunnable::UpdateTrackerSettings()
{
#if WITH_MLSDK
	// If the tracker has already been created, update the setttings
	// If the tracker has not been created, this cached Setting will be used whenever MLImageTrackerCreate() is called.
	if (MLHandleIsValid(ImageTracker))
	{
		FScopeLock Lock(&SettingsMutex);
		MLResult Result = MLImageTrackerUpdateSettings(ImageTracker, &Settings);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("MLImageTrackerUpdateSettings failed with error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
	}
#endif //WITH_MLSDK
}