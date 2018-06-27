// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ImageTrackerComponent.h"
#include "ImageTrackerRunnable.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "AppEventHandler.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_image_tracking.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

class FImageTrackerImpl : public MagicLeap::IAppEventHandler
{
public:
	FImageTrackerImpl(UImageTrackerComponent* InOwner)
	: Owner(InOwner)
	, State(EState::WaitingForAppInit)
#if WITH_MLSDK
	, Target(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
#if WITH_EDITOR
	, TextureBeforeEdit(nullptr)
#endif
	, PrevTextureTarget(nullptr)
	{
#if WITH_MLSDK
		OldTrackingStatus.status = MLImageTrackerTargetStatus_Ensure32Bits;
#endif //WITH_MLSDK
	};

	virtual ~FImageTrackerImpl()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(Target))
		{
			checkf(ImageTrackerRunnable.IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			MLResult Result = MLImageTrackerRemoveTarget(ImageTrackerRunnable->GetHandle(), Target);
			UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("MLImageTrackerRemoveTarget failed with error %d."), Result);
			Target = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
		// Calling this explicitly to make the chain of destruction more obvious,
		// ie a potential call to FImageTrackerRunnable's destructor right here.
		// On the lumin platform this call will take place inside the destruction
		// worker thread.
		ImageTrackerRunnable.Reset();

		// need this call to prevent the runnable form persisting with a ref count of 1 (due to Appframework's shared pointer instance).
		if (GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
		{
			static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFramework().RefreshImageTrackerRunnableReferences();
		}

	}

	void SetTargetAsync(const FString& InName, bool bInIsStationary, float InLongerDimension, UTexture2D* InTargetTexture)
	{
#if WITH_MLSDK
		checkf(ImageTrackerRunnable.IsValid(), TEXT("Invalid image tracker!"));
		FTrackerMessage CreateTargetMsg;
		CreateTargetMsg.TaskType = FTrackerMessage::TaskType::TryCreateTarget;
		CreateTargetMsg.Requester = this;
		CreateTargetMsg.PrevTarget = Target;
		CreateTargetMsg.TargetName = InName;
		MLImageTrackerTargetSettings TargetSettings;
		TargetSettings.longer_dimension = InLongerDimension;
		TargetSettings.is_stationary = bInIsStationary;
		CreateTargetMsg.TargetSettings = TargetSettings;
		CreateTargetMsg.TargetImageTexture = InTargetTexture;
		ImageTrackerRunnable->IncomingMessages.Enqueue(CreateTargetMsg);
#endif //WITH_MLSDK
	}

	void Tick()
	{
#if WITH_MLSDK
		if (!IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
		{
			return;
		}

		switch (State)
		{
		case WaitingForAppInit:
		{
			checkf(GEngine, TEXT("[FCameraCaptureRunnable::Get()] GEngine is null!"));
			FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFramework();

			if (!AppFramework.IsInitialized())
			{
				UE_LOG(LogMagicLeap, Warning, TEXT("[UImageTrackerComponent] AppFramework not initialized."));
			}
			else
			{
				State = NoTarget;
				ImageTrackerRunnable = AppFramework.GetImageTrackerRunnable();
			}

			break;
		}
		case NoTarget:
		{
			if (Owner->TargetImageTexture == nullptr)
			{
				UE_LOG(LogMagicLeap, Warning, TEXT("[UImageTrackerComponent] ImageTracker: No image selected to track."));
			}
			else if (Owner->TargetImageTexture->GetPixelFormat() != EPixelFormat::PF_R8G8B8A8 && Owner->TargetImageTexture->GetPixelFormat() != EPixelFormat::PF_B8G8R8A8)
			{
				State = TargetCreationError;
				UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] ImageTracker: Unsupported pixel format encountered!"));
			}
			else
			{
				State = CreatingTarget;
				if (Owner->Name.Len() == 0) Owner->Name = Owner->GetName();
				const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();
				SetTargetAsync(Owner->Name, Owner->bIsStationary, Owner->LongerDimension / AppFramework.GetWorldToMetersScale(), Owner->TargetImageTexture);
			}

			break;
		}
		case CreatingTarget:
		{
			FTrackerMessage OutMsg;
			if (ImageTrackerRunnable->OutgoingMessages.Peek(OutMsg))
			{
				if (OutMsg.Requester == this)
				{
					ImageTrackerRunnable->OutgoingMessages.Pop();
					if (OutMsg.TaskType == FTrackerMessage::TargetCreateSucceeded)
					{
						State = Tracking;
						Data = OutMsg.Data;
						Target = OutMsg.Target;
						PrevTextureTarget = Owner->TargetImageTexture;
						Owner->OnSetImageTargetSucceeded.Broadcast();
					}
					else
					{
						State = TargetCreationError;
						Owner->OnSetImageTargetFailed.Broadcast();
					}
				}
			}

			break;
		}
		case TargetCreationError:
		{
			CheckForNewTarget();
			break;
		}
		case Tracking:
		{
			checkf(ImageTrackerRunnable.IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			MLImageTrackerTargetResult TrackingStatus;
			MLResult Result = MLImageTrackerGetTargetResult(ImageTrackerRunnable->GetHandle(), Target, &TrackingStatus);
			if (Result != MLResult_Ok)
			{
				TrackingStatus.status = MLImageTrackerTargetStatus_NotTracked;
			}

			if (TrackingStatus.status == MLImageTrackerTargetStatus_NotTracked)
			{
				if (OldTrackingStatus.status != MLImageTrackerTargetStatus_NotTracked)
				{
					Owner->OnImageTargetLost.Broadcast();
				}
			}
			else
			{
				EFailReason FailReason = EFailReason::None;
				FTransform Pose = FTransform::Identity;
				const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();
				if (AppFramework.GetTransform(Data.coord_frame_target, Pose, FailReason))
				{
					Pose.SetRotation(MagicLeap::ToUERotator(Pose.GetRotation()));
					if (TrackingStatus.status == MLImageTrackerTargetStatus_Unreliable)
					{
						FVector LastTrackedLocation = Owner->GetComponentLocation();
						FRotator LastTrackedRotation = Owner->GetComponentRotation();
						if (Owner->bUseUnreliablePose)
						{
							Owner->SetRelativeLocationAndRotation(Pose.GetTranslation(), Pose.Rotator());
						}
						// Developer can choose whether to use this unreliable pose or not.
						Owner->OnImageTargetUnreliableTracking.Broadcast(LastTrackedLocation, LastTrackedRotation, Pose.GetTranslation(), Pose.Rotator());
					}
					else
					{
						Owner->SetRelativeLocationAndRotation(Pose.GetTranslation(), Pose.Rotator());
						if (OldTrackingStatus.status != MLImageTrackerTargetStatus_Tracked)
						{
							Owner->OnImageTargetFound.Broadcast();
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
					if (OldTrackingStatus.status != MLImageTrackerTargetStatus_NotTracked)
					{
						Owner->OnImageTargetLost.Broadcast();
					}
				}
			}

			OldTrackingStatus = TrackingStatus;
			CheckForNewTarget();
			break;
		}
		}
#endif //WITH_MLSDK
	}

	void CheckForNewTarget()
	{
		if (Owner->TargetImageTexture != PrevTextureTarget)
		{
			State = NoTarget;
		}
	}

public:
	enum EState
	{
		WaitingForAppInit,
		NoTarget,
		CreatingTarget,
		TargetCreationError,
		Tracking,
	};

	UImageTrackerComponent* Owner;
	EState State;
	TSharedPtr<FImageTrackerRunnable, ESPMode::ThreadSafe> ImageTrackerRunnable;
#if WITH_MLSDK
	MLHandle Target;
	MLImageTrackerTargetStaticData Data;
	MLImageTrackerTargetResult OldTrackingStatus;
#endif //WITH_MLSDK
#if WITH_EDITOR
	UTexture2D* TextureBeforeEdit;
#endif
	UTexture2D* PrevTextureTarget;
};

UImageTrackerComponent::UImageTrackerComponent()
: Impl(new FImageTrackerImpl(this))
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
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Impl->Tick();
}

void UImageTrackerComponent::SetTargetAsync(UTexture2D* NewTextureTarget)
{
	if (!NewTextureTarget || (NewTextureTarget->GetPixelFormat() == EPixelFormat::PF_R8G8B8A8 || NewTextureTarget->GetPixelFormat() == EPixelFormat::PF_B8G8R8A8))
	{
		Impl->PrevTextureTarget = TargetImageTexture;
		TargetImageTexture = NewTextureTarget;
	}
	else
	{
		UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent::SetTarget()] ImageTracker: Unsupported pixel format encountered!"));
	}
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
			Impl->PrevTextureTarget = TargetImageTexture;
			TargetImageTexture = Impl->TextureBeforeEdit;
			UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Cannot set texture %s as it uses an invalid pixel format!  Valid formats are R8B8G8A8 or B8G8R8A8"), *TargetImageTexture->GetName());
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR

void UImageTrackerFunctionLibrary::SetMaxSimultaneousTargets(int32 MaxSimultaneousTargets)
{
	FTrackerMessage SetMaxTargetsMsg;
	SetMaxTargetsMsg.TaskType = FTrackerMessage::TaskType::SetMaxTargets;
	SetMaxTargetsMsg.MaxTargets = MaxSimultaneousTargets < 1 ? 1 : static_cast<uint32>(MaxSimultaneousTargets);
	checkf(GEngine, TEXT("[UImageTrackerFunctionLibrary::SetMaxSimultaneousTargets()] GEngine is null!"));
	FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFramework();
	checkf(AppFramework.IsInitialized(), TEXT("[UImageTrackerFunctionLibrary::SetMaxSimultaneousTargets()] AppFramework not yet initialized!"));
	AppFramework.GetImageTrackerRunnable()->IncomingMessages.Enqueue(SetMaxTargetsMsg);
}

void UImageTrackerFunctionLibrary::EnableImageTracking(bool bEnable)
{
	FTrackerMessage EnableMsg;
	EnableMsg.TaskType = FTrackerMessage::TaskType::SetEnabled;
	EnableMsg.bEnable = bEnable;
	checkf(GEngine, TEXT("[UImageTrackerFunctionLibrary::EnableImageTracking()] GEngine is null!"));
	FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFramework();
	checkf(AppFramework.IsInitialized(), TEXT("[UImageTrackerFunctionLibrary::EnableImageTracking()] AppFramework not yet initialized!"));
	AppFramework.GetImageTrackerRunnable()->IncomingMessages.Enqueue(EnableMsg);
}