// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	static TWeakPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> Get()
	{
		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FImageTrackerEngineInterface());
		}
		// Because this is a singleton, when running in ZI, consecutive VRPreview launches would fail
		// because the tracker would be destroyed but created only on the very first launch, when
		// FImageTrackerEngineInterface was constructed.
		// Thus, we need to create the tracker separately.
#if WITH_MLSDK
		if (!MLHandleIsValid(Instance->GetHandle()))
		{
			Instance->CreateTracker();
		}
#endif //WITH_MLSDK

		return Instance;
	}

#if WITH_MLSDK
	MLHandle GetHandle() const
	{
		return ImageTracker;
	}
#endif //WITH_MLSDK

private:
	FImageTrackerEngineInterface()
#if WITH_MLSDK
	: ImageTracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	{
		CreateTracker();
	}

	void CreateTracker()
	{
#if WITH_MLSDK
		UE_LOG(LogMagicLeap, Display, TEXT("[FImageTrackerEngineInterface] Creating Image Tracker"));
		FMemory::Memset(&Settings, 0, sizeof(Settings));
		bool bResult = MLImageTrackerInitSettings(&Settings);

		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Could not initialize image tracker settings mofo"));
		}

		ImageTracker = MLImageTrackerCreate(&Settings);

		if (!MLHandleIsValid(ImageTracker))
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Could not create Image tracker."));
		}
#endif //WITH_MLSDK
	}	

	void OnAppPause() override
	{
#if WITH_MLSDK
		bWasSystemEnabledOnPause = Settings.enable_image_tracking;

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
				Settings.enable_image_tracking = false;

				if (!MLImageTrackerUpdateSettings(ImageTracker, &Settings))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Failed to disable image tracker on application pause."));
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
				Settings.enable_image_tracking = true;

				if (!MLImageTrackerUpdateSettings(ImageTracker, &Settings))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Failed to re-enable image tracker on application resume."));
				}
				else
				{
					UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Image tracker re-enabled on application resume."));
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
			bool bResult = MLImageTrackerDestroy(ImageTracker);

			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Error destroying image tracker."));
			}

			ImageTracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}

#if WITH_MLSDK
	MLHandle ImageTracker;
	MLImageTrackerSettings Settings;
#endif //WITH_MLSDK
	static TSharedPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> Instance;
};

TSharedPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> FImageTrackerEngineInterface::Instance;

struct FTrackerMessage
{
	enum TaskType
	{
		None,
		Create,
		ReportStatus,
	};

	TaskType TaskType;
	FString TargetName;
	UTexture2D* TargetImageTexture;
#if WITH_MLSDK
	MLImageTrackerTargetSettings TargetSettings;
	MLImageTrackerTargetResult TrackingStatus;
#endif //WITH_MLSDK

	FTrackerMessage()
	: TaskType(TaskType::None)
	{}
};

class FImageTrackerImpl : public FRunnable, public MagicLeap::IAppEventHandler
{
public:
	FImageTrackerImpl()
		: bHasTarget(false)
#if WITH_EDITOR
		, TextureBeforeEdit(nullptr)
#endif
#if WITH_MLSDK
		, Target(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
		, Thread(nullptr)
		, StopTaskCounter(0)
		, bIsTracking(false)
	{
#if WITH_MLSDK
		OldTrackingStatus.status = MLImageTrackerTargetStatus_Ensure32Bits;
#endif //WITH_MLSDK

#if PLATFORM_LUMIN
		Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
		Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
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
		while (StopTaskCounter.GetValue() == 0)
		{
			if (IncomingMessages.Dequeue(CurrentMessage))
			{
				DoTasks();
			}

			FPlatformProcess::Sleep(0.5f);
		}

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
		MLImageTrackerTargetSettings TargetSettings;
		TargetSettings.longer_dimension = InLongerDimension;
		TargetSettings.is_stationary = bInIsStationary;
		FTrackerMessage CreateTargetMsg;
		CreateTargetMsg.TaskType = FTrackerMessage::TaskType::Create;
		CreateTargetMsg.TargetName = InName;
		CreateTargetMsg.TargetSettings = TargetSettings;
		CreateTargetMsg.TargetImageTexture = InTargetTexture;
		IncomingMessages.Enqueue(CreateTargetMsg);
#endif //WITH_MLSDK
	}
	
public:
	TQueue<FTrackerMessage, EQueueMode::Spsc> OutgoingMessages;
	bool bHasTarget;
#if WITH_EDITOR
	UTexture2D* TextureBeforeEdit;
#endif
#if WITH_MLSDK
	MLHandle Target;
	MLImageTrackerTargetStaticData Data;
	MLImageTrackerTargetResult OldTrackingStatus;
#endif //WITH_MLSDK

public:
	TWeakPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> ImageTracker;
	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TQueue<FTrackerMessage, EQueueMode::Spsc> IncomingMessages;
	FTrackerMessage CurrentMessage;
	bool bIsTracking;

	void DoTasks()
	{
		switch (CurrentMessage.TaskType)
		{
		case FTrackerMessage::TaskType::None: break;
		case FTrackerMessage::TaskType::Create: SetTarget(); break;
		case FTrackerMessage::TaskType::ReportStatus: break;
		}
	}

	void SetTarget()
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(Target))
		{
			CurrentMessage.TargetSettings.name = TCHAR_TO_UTF8(*CurrentMessage.TargetName);
			FTexture2DMipMap& Mip = CurrentMessage.TargetImageTexture->PlatformData->Mips[0];
			const unsigned char* PixelData = static_cast<const unsigned char*>(Mip.BulkData.Lock(LOCK_READ_ONLY));
			checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			Target = MLImageTrackerAddTargetFromArray(ImageTracker.Pin()->GetHandle(), &CurrentMessage.TargetSettings, PixelData, CurrentMessage.TargetImageTexture->GetSurfaceWidth(), CurrentMessage.TargetImageTexture->GetSurfaceHeight(), MLImageTrackerImageFormat_RGBA);
			Mip.BulkData.Unlock();

			if (!MLHandleIsValid(Target))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerImpl] Could not create Image Target."));
				return;
			}

			// [3] Cache all the static data for this target.
			checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			if (!MLImageTrackerGetTargetStaticData(ImageTracker.Pin()->GetHandle(), Target, &Data))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerImpl] Could not get the static data for the Image Target."));
				return;
			}

			bIsTracking = true;
		}
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
	, bTick(true)
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
		Impl->AsyncDestroy();
		Impl = nullptr;
	}
#else
	delete Impl;
	Impl = nullptr;
#endif // PLATFORM_LUMIN	
}

void UImageTrackerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if WITH_MLSDK
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
		bTick = false;
	}

	if (!bTick) return;

	if (!Impl->bHasTarget)
	{
		if (Name.Len() == 0) Name = GetName();
		Impl->SetTargetAsync(Name, bIsStationary, LongerDimension / AppFramework.GetWorldToMetersScale(), TargetImageTexture);
	}

	if (Impl->bIsTracking)
	{
		checkf(Impl->ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));

		MLImageTrackerTargetResult TrackingStatus;
		if (!MLImageTrackerGetTargetResult(Impl->ImageTracker.Pin()->GetHandle(), Impl->Target, &TrackingStatus))
		{
			TrackingStatus.status = MLImageTrackerTargetStatus_NotTracked;
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
				if (AppFramework.GetTransform(Impl->Data.coord_frame_target, Pose, FailReason))
				{
				Pose.SetRotation(MagicLeap::ToUERotator(Pose.GetRotation()));
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
			TargetImageTexture = Impl->TextureBeforeEdit;
			UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Cannot set texture %s as it uses an invalid pixel format!  Valid formats are R8B8G8A8 or B8G8R8A8"), *TargetImageTexture->GetName());
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
