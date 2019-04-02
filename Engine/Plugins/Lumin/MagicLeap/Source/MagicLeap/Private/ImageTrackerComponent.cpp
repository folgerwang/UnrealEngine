// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ImageTrackerComponent.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "AppEventHandler.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_image_tracking.h>
#include <ml_perception.h>
#include <ml_snapshot.h>
#include <ml_head_tracking.h>
#include <ml_coordinate_frame_uid.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

class FImageTrackerEngineInterface : public MagicLeap::IAppEventHandler
{
public:
	static TWeakPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> Get(bool bCreateTracker = true)
	{
		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FImageTrackerEngineInterface());
		}

		if (bCreateTracker)
		{
			// needs to be a separate call instead of being embedded in the runnable constructor so it works in consequetive VRPreview calls.
			Instance->CreateTracker();
		}

		return Instance;
	}

#if WITH_MLSDK
	MLHandle GetHandle() const
	{
		FScopeLock Lock(&TrackerMutex);
		return ImageTracker;
	}

	uint32 GetMaxSimultaneousTargets() const
	{
		FScopeLock Lock(&TrackerMutex);
		return Settings.max_simultaneous_targets;
	}

	void SetMaxSimultaneousTargets(uint32 NewNumTargets)
	{
		FScopeLock Lock(&TrackerMutex);
		Settings.max_simultaneous_targets = NewNumTargets;
		UpdateImageTrackerSettings();
	}

	bool GetImageTrackerEnabled() const
	{
		FScopeLock Lock(&TrackerMutex);
		return Settings.enable_image_tracking && MLHandleIsValid(ImageTracker);
	}

	void SetImageTrackerEnabled(bool bEnabled)
	{
		FScopeLock Lock(&TrackerMutex);
		Settings.enable_image_tracking = bEnabled;
		// TODO: this should be async.
		UpdateImageTrackerSettings();
	}
#endif //WITH_MLSDK

private:
	FImageTrackerEngineInterface()
#if WITH_MLSDK
	: MagicLeap::IAppEventHandler({ MLPrivilegeID_CameraCapture })
	, ImageTracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	{
#if WITH_MLSDK
		FMemory::Memset(&Settings, 0, sizeof(Settings));
		MLResult Result = MLImageTrackerInitSettings(&Settings);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Could not initialize image tracker settings mofo"));
		}
#endif //WITH_MLSDK
	}

	void CreateTracker()
	{
#if WITH_MLSDK
		FScopeLock Lock(&TrackerMutex);
		if (!MLHandleIsValid(ImageTracker))
		{
			UE_LOG(LogMagicLeap, Display, TEXT("[FImageTrackerEngineInterface] Creating Image Tracker"));

			if (MagicLeap::EPrivilegeState::Granted == GetPrivilegeStatus(MLPrivilegeID_CameraCapture))
			{
				MLImageTrackerCreate(&Settings, &ImageTracker);

				if (!MLHandleIsValid(ImageTracker))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Could not create Image tracker."));
				}
			}
		}
#endif //WITH_MLSDK
	}	

	void OnAppPause() override
	{
#if WITH_MLSDK
		{
			FScopeLock Lock(&TrackerMutex);
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
				FScopeLock Lock(&TrackerMutex);
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

	void OnAppResume() override
	{
#if WITH_MLSDK
		if (!bWasSystemEnabledOnPause)
		{
			UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Not resuming image tracker as it was not enabled at time of application pause."));
		}
		else
		{
			if (!MLHandleIsValid(ImageTracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Image tracker was invalid on application resume."));
			}
			else
			{
				if (GetPrivilegeStatus(MLPrivilegeID_CameraCapture) == MagicLeap::EPrivilegeState::Granted)
				{
					FScopeLock Lock(&TrackerMutex);
					Settings.enable_image_tracking = true;
					// TODO: this should be async
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

	void OnAppShutDown() override
	{
#if WITH_MLSDK
		if (MLHandleIsValid(ImageTracker))
		{
			MLResult Result = MLImageTrackerDestroy(ImageTracker);

			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Error destroying image tracker."));
			}

			ImageTracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}

	void UpdateImageTrackerSettings()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(ImageTracker))
		{
			if (GetPrivilegeStatus(MLPrivilegeID_CameraCapture) == MagicLeap::EPrivilegeState::Granted)
			{
				FScopeLock Lock(&TrackerMutex);
				MLResult Result = MLImageTrackerUpdateSettings(ImageTracker, &Settings);
				if (Result != MLResult_Ok)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerRunnable] Failed to update image tracker settings due to error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				}
			}
			else
			{
				UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerRunnable] Image tracking settings failed to updatee due to lack of privilege!"));
			}
		}
#endif //WITH_MLSDK
	}

	mutable FCriticalSection TrackerMutex;
	static TSharedPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> Instance;

#if WITH_MLSDK
	MLImageTrackerSettings Settings;
private:
	MLHandle ImageTracker;
#endif //WITH_MLSDK
};

TSharedPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> FImageTrackerEngineInterface::Instance;

class FImageTrackerImpl : public FRunnable, public MagicLeap::IAppEventHandler
{
public:
	FImageTrackerImpl()
		: bHasTarget(false)
		, bIsTracking(false)
#if WITH_EDITOR
		, TextureBeforeEdit(nullptr)
#endif
#if WITH_MLSDK
		, Target(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
		, Thread(nullptr)
		, StopTaskCounter(0)
	{
#if WITH_MLSDK
		OldTrackingStatus.status = MLImageTrackerTargetStatus_Ensure32Bits;
#endif //WITH_MLSDK
	};

	virtual ~FImageTrackerImpl()
	{
		StopTaskCounter.Increment();

		if (nullptr != Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}

#if WITH_MLSDK
		if (MLHandleIsValid(Target))
		{
			checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			MLImageTrackerRemoveTarget(ImageTracker.Pin()->GetHandle(), Target);
			Target = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK

		ImageTracker.Reset();
	}

	virtual uint32 Run() override
	{
#if WITH_MLSDK
		while (StopTaskCounter.GetValue() == 0)
		{
			if (!ImageTracker.IsValid())
			{
				ImageTracker = FImageTrackerEngineInterface::Get();
			}

			if (MLHandleIsValid(ImageTracker.Pin()->GetHandle()))
			{
				if (IncomingMessages.Dequeue(CurrentMessage))
				{
					DoTasks();
				}
			}

			FPlatformProcess::Sleep(0.5f);
		}

#endif //WITH_MLSDK
		return 0;
	}

	void SetTargetAsync(const FString& InName, bool bInIsStationary, float InLongerDimension, UTexture2D* InTargetTexture)
	{
#if WITH_MLSDK
		if (!ImageTracker.IsValid())
		{
			ImageTracker = FImageTrackerEngineInterface::Get();
		}

		bHasTarget = true;
		bIsTracking = false;
		MLImageTrackerTargetSettings TargetSettings;
		TargetSettings.longer_dimension = InLongerDimension;
		TargetSettings.is_stationary = bInIsStationary;
		FTrackerMessage CreateTargetMsg;
		CreateTargetMsg.Requester = this;
		CreateTargetMsg.TaskType = FTrackerMessage::TaskType::TryCreateTarget;
		CreateTargetMsg.TargetName = InName;
		CreateTargetMsg.TargetSettings = TargetSettings;
		CreateTargetMsg.TargetImageTexture = InTargetTexture;

		if (Thread == nullptr)
		{
			StopTaskCounter.Reset();
#if PLATFORM_LUMIN
			Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
			Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
		}

		IncomingMessages.Enqueue(CreateTargetMsg);
#endif //WITH_MLSDK
	}

	bool TryGetResult(FTrackerMessage& OutMsg)
	{
#if PLATFORM_LUMIN
		if (OutgoingMessages.Peek(OutMsg))
		{
			if (OutMsg.Requester == this)
			{
				OutgoingMessages.Pop();
				return true;
			}
		}
#endif //PLATFORM_LUMIN
		return false;
	}
	
public:
	bool bHasTarget;
	bool bIsTracking;
#if WITH_EDITOR
	UTexture2D* TextureBeforeEdit;
#endif
#if WITH_MLSDK
	MLHandle Target;
	MLImageTrackerTargetStaticData Data;
	MLImageTrackerTargetResult OldTrackingStatus;
#endif //WITH_MLSDK
	TWeakPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> ImageTracker;
	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TQueue<FTrackerMessage, EQueueMode::Spsc> IncomingMessages;
	TQueue<FTrackerMessage, EQueueMode::Spsc> OutgoingMessages;
	FTrackerMessage CurrentMessage;
	FCriticalSection DataMutex;

	void DoTasks()
	{
		switch (CurrentMessage.TaskType)
		{
		case FTrackerMessage::TaskType::None: break;
		case FTrackerMessage::TaskType::TryCreateTarget: SetTarget(); break;
		//case FTrackerMessage::TaskType::ReportStatus: break;
		}
	}

	void SetTarget()
	{
#if WITH_MLSDK
		FScopeLock Lock(&DataMutex);

		if (MLHandleIsValid(Target))
		{
			MLImageTrackerRemoveTarget(ImageTracker.Pin()->GetHandle(), Target);
		}

		UE_LOG(LogMagicLeap, Warning, TEXT("SetTarget for %s"), *CurrentMessage.TargetName);
		FTrackerMessage TargetCreateMsg;
		TargetCreateMsg.Requester = CurrentMessage.Requester;

		CurrentMessage.TargetSettings.name = TCHAR_TO_UTF8(*CurrentMessage.TargetName);
		FTexture2DMipMap& Mip = CurrentMessage.TargetImageTexture->PlatformData->Mips[0];
		const unsigned char* PixelData = static_cast<const unsigned char*>(Mip.BulkData.Lock(LOCK_READ_ONLY));
		checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
		MLImageTrackerAddTargetFromArray(ImageTracker.Pin()->GetHandle(), &CurrentMessage.TargetSettings, PixelData, CurrentMessage.TargetImageTexture->GetSurfaceWidth(), CurrentMessage.TargetImageTexture->GetSurfaceHeight(), MLImageTrackerImageFormat_RGBA, &Target);
		Mip.BulkData.Unlock();

		if (!MLHandleIsValid(Target))
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerImpl] Could not create Image Target."));
			TargetCreateMsg.TaskType = FTrackerMessage::TaskType::TargetCreateFailed;
			OutgoingMessages.Enqueue(TargetCreateMsg);
			return;
		}

		// [3] Cache all the static data for this target.
		checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
		MLResult Result = MLImageTrackerGetTargetStaticData(ImageTracker.Pin()->GetHandle(), Target, &Data);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerImpl] Could not get the static data for the Image Target."));
			TargetCreateMsg.TaskType = FTrackerMessage::TaskType::TargetCreateFailed;
			OutgoingMessages.Enqueue(TargetCreateMsg);
			return;
		}

		UE_LOG(LogMagicLeap, Log, TEXT("SetTarget successfully set for %s"), *CurrentMessage.TargetName);
		TargetCreateMsg.TaskType = FTrackerMessage::TaskType::TargetCreateSucceeded;
		TargetCreateMsg.Target = Target;
		TargetCreateMsg.Data = Data;
		OutgoingMessages.Enqueue(TargetCreateMsg);
#endif //WITH_MLSDK
	}

public:
#if WITH_MLSDK
	inline MLHandle Tracker()
	{
		if (!ImageTracker.IsValid())
		{
			return ML_INVALID_HANDLE;
		}
		checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
		return ImageTracker.Pin()->GetHandle();
	}
#endif //WITH_MLSDK
};

UImageTrackerComponent::UImageTrackerComponent()
	: Impl(new FImageTrackerImpl())
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bAutoActivate = true;
}

UImageTrackerComponent::~UImageTrackerComponent()
{
#if PLATFORM_LUMIN
	if (Impl)
	{
		if (!Impl->AsyncDestroy())
		{
			delete Impl;
		}
		Impl = nullptr;
	}
#else
	delete Impl;
	Impl = nullptr;
#endif // PLATFORM_LUMIN	
}

void UImageTrackerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if WITH_MLSDK && PLATFORM_LUMIN
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!(IMagicLeapPlugin::Get().IsMagicLeapHMDValid()))
	{
		return;
	}

	const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();

	if (!AppFramework.IsInitialized())
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("[UImageTrackerComponent] AppFramework not initialized."));
		return;
	}

	if (TargetImageTexture == nullptr)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("ImageTracker: No image selected to track."));
		return;
	}

	if (TargetImageTexture->GetPixelFormat() != EPixelFormat::PF_R8G8B8A8 && TargetImageTexture->GetPixelFormat() != EPixelFormat::PF_B8G8R8A8)
	{
		UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] ImageTracker: Unsupported pixel format encountered!"));
		return;
	}

	if (!Impl->bHasTarget)
	{
		if (Name.Len() == 0) Name = GetName();
		Impl->SetTargetAsync(Name, bIsStationary, LongerDimension / AppFramework.GetWorldToMetersScale(), TargetImageTexture);
	}

	FTrackerMessage TrackingResult;
	if (Impl->TryGetResult(TrackingResult))
	{
		if (TrackingResult.TaskType == FTrackerMessage::TaskType::TargetCreateSucceeded)
		{
			OnSetImageTargetSucceeded.Broadcast();
			Impl->bIsTracking = true;
		}
		else if(TrackingResult.TaskType == FTrackerMessage::TaskType::TargetCreateFailed)
		{
			OnSetImageTargetFailed.Broadcast();
			Impl->bIsTracking = false;
		}
	}

	if (Impl->bIsTracking)
	{
		checkf(Impl->ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
		MLImageTrackerTargetResult TrackingStatus;
		{
			FScopeLock Lock(&Impl->DataMutex);
			MLResult Result = MLImageTrackerGetTargetResult(Impl->ImageTracker.Pin()->GetHandle(), Impl->Target, &TrackingStatus);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Warning, TEXT("MLImageTrackerGetTargetResult failed due to error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
				TrackingStatus.status = MLImageTrackerTargetStatus_NotTracked;
			}
		}

		if (TrackingStatus.status == MLImageTrackerTargetStatus_NotTracked)
		{
			if (Impl->OldTrackingStatus.status != MLImageTrackerTargetStatus_NotTracked)
			{
				OnImageTargetLost.Broadcast();
			}
		}
		else
		{
			EFailReason FailReason = EFailReason::None;
			FTransform Pose = FTransform::Identity;
			bool bHasTransform = false;
			{
				FScopeLock Lock(&Impl->DataMutex);
				bHasTransform = AppFramework.GetTransform(Impl->Data.coord_frame_target, Pose, FailReason);
			}
			if (bHasTransform)
			{
				Pose.ConcatenateRotation(FQuat(FVector(0, 0, 1), PI));
				if (TrackingStatus.status == MLImageTrackerTargetStatus_Unreliable)
				{
					FVector LastTrackedLocation = GetComponentLocation();
					FRotator LastTrackedRotation = GetComponentRotation();
					if (bUseUnreliablePose)
					{
						this->SetRelativeLocationAndRotation(Pose.GetTranslation(), Pose.Rotator());
					}
					// Developer can choose whether to use this unreliable pose or not.
					OnImageTargetUnreliableTracking.Broadcast(LastTrackedLocation, LastTrackedRotation, Pose.GetTranslation(), Pose.Rotator());
				}
				else
				{
					this->SetRelativeLocationAndRotation(Pose.GetTranslation(), Pose.Rotator());
					if (Impl->OldTrackingStatus.status != MLImageTrackerTargetStatus_Tracked)
					{
						OnImageTargetFound.Broadcast();
					}
				}
			}
			else
			{
				if (FailReason == EFailReason::NaNsInTransform)
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] NaNs in image tracker target transform."));
				}
				TrackingStatus.status = MLImageTrackerTargetStatus_NotTracked;
				if (Impl->OldTrackingStatus.status != MLImageTrackerTargetStatus_NotTracked)
				{
					OnImageTargetLost.Broadcast();
				}
			}
		}

		Impl->OldTrackingStatus = TrackingStatus;
	}
#endif //WITH_MLSDK
}

bool UImageTrackerComponent::SetTargetAsync(UTexture2D* ImageTarget)
{
	if (ImageTarget == nullptr)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("[UImageTrackerComponent] ImageTarget is NULL!."));
		return false;
	}

	const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();

	if (!AppFramework.IsInitialized())
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("[UImageTrackerComponent] AppFramework not initialized."));
		return false;
	}

	if (ImageTarget && !(ImageTarget->GetPixelFormat() == EPixelFormat::PF_R8G8B8A8 || ImageTarget->GetPixelFormat() == EPixelFormat::PF_B8G8R8A8))
	{
		UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Cannot set texture %s as it uses an invalid pixel format!  Valid formats are R8B8G8A8 or B8G8R8A8"), *ImageTarget->GetName());
		return false;
	}

	if (ImageTarget == TargetImageTexture)
	{
		UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Skipped setting %s as it is already being used as the current image target"), *ImageTarget->GetName());
		return false;
	}

	TargetImageTexture = ImageTarget;
	Impl->SetTargetAsync(Name, bIsStationary, LongerDimension / AppFramework.GetWorldToMetersScale(), TargetImageTexture);
	return true;
}



#if WITH_EDITOR
void UImageTrackerComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	if ((PropertyAboutToChange != nullptr) && (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UImageTrackerComponent, TargetImageTexture)))
	{
		Impl->TextureBeforeEdit = TargetImageTexture;
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UImageTrackerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageTrackerComponent, TargetImageTexture))
	{
		if (TargetImageTexture && !(TargetImageTexture->GetPixelFormat() == EPixelFormat::PF_R8G8B8A8 || TargetImageTexture->GetPixelFormat() == EPixelFormat::PF_B8G8R8A8))
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Cannot set texture %s as it uses an invalid pixel format!  Valid formats are R8B8G8A8 or B8G8R8A8"), *TargetImageTexture->GetName());
			TargetImageTexture = Impl->TextureBeforeEdit;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UImageTrackerFunctionLibrary::SetMaxSimultaneousTargets(int32 MaxSimultaneousTargets)
{
#if WITH_MLSDK
	FImageTrackerEngineInterface::Get(false).Pin()->SetMaxSimultaneousTargets(MaxSimultaneousTargets);
#endif //WITH_MLSDK
}

int32 UImageTrackerFunctionLibrary::GetMaxSimultaneousTargets()
{
#if WITH_MLSDK
	return FImageTrackerEngineInterface::Get(false).Pin()->GetMaxSimultaneousTargets();
#else
	return 0;
#endif //WITH_MLSDK
}

void UImageTrackerFunctionLibrary::EnableImageTracking(bool bEnable)
{
#if WITH_MLSDK
	FImageTrackerEngineInterface::Get(false).Pin()->SetImageTrackerEnabled(bEnable);
#endif //WITH_MLSDK
}

bool UImageTrackerFunctionLibrary::IsImageTrackingEnabled()
{
#if WITH_MLSDK
	return FImageTrackerEngineInterface::Get(false).Pin()->GetImageTrackerEnabled();
#else
	return false;
#endif //WITH_MLSDK
}
