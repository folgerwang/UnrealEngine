// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AppFramework.h"
#include "MagicLeapHMD.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#include "RenderingThread.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END
#include "IMagicLeapModule.h"

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_snapshot.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

TArray<MagicLeap::IAppEventHandler*> FAppFramework::EventHandlers;
FCriticalSection FAppFramework::EventHandlersCriticalSection;
MagicLeap::FAsyncDestroyer* FAppFramework::AsyncDestroyer = nullptr;
TMap<FName, IMagicLeapModule*> FAppFramework::RegisteredModules;


FAppFramework::FAppFramework()
{}

FAppFramework::~FAppFramework()
{
	Shutdown();
}

void FAppFramework::Startup()
{
	base_dirty_ = false;

#if WITH_MLSDK
	base_coordinate_frame_.data[0] = 0;
	base_coordinate_frame_.data[1] = 0;
#endif //WITH_MLSDK

	base_position_ = FVector::ZeroVector;
	base_orientation_ = FQuat::Identity;

	// Register application lifecycle delegates
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FAppFramework::ApplicationPauseDelegate);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FAppFramework::ApplicationResumeDelegate);

	AsyncDestroyer = new MagicLeap::FAsyncDestroyer();

	bInitialized = true;

	saved_max_fps_ = 0.0f;
}

void FAppFramework::Shutdown()
{
	bInitialized = false;

	delete AsyncDestroyer;
	AsyncDestroyer = nullptr;
}

void FAppFramework::BeginUpdate()
{
#if WITH_MLSDK
	if (bInitialized)
	{
		if (base_dirty_)
		{
			MLTransform Transform = MagicLeap::kIdentityTransform;
			Transform.position = MagicLeap::ToMLVector(-base_position_, GetWorldToMetersScale());
			Transform.rotation = MagicLeap::ToMLQuat(base_orientation_.Inverse());

			base_coordinate_frame_.data[0] = 0;
			base_coordinate_frame_.data[1] = 0;
			base_position_ = FVector::ZeroVector;
			base_orientation_ = FQuat::Identity;
			base_dirty_ = false;
		}

		FScopeLock Lock(&EventHandlersCriticalSection);
		for (auto EventHandler : EventHandlers)
		{
			EventHandler->OnAppTick();
		}
	}
#endif //WITH_MLSDK
}

void FAppFramework::ApplicationPauseDelegate()
{
	UE_LOG(LogMagicLeap, Log, TEXT("+++++++ ML AppFramework APP PAUSE ++++++"));

	if (GEngine)
	{
		saved_max_fps_ = GEngine->GetMaxFPS();
		// MaxFPS = 0 means uncapped. So we set it to something trivial like 10 to keep network connections alive.
		GEngine->SetMaxFPS(10.0f);

		APlayerController* PlayerController = GEngine->GetFirstLocalPlayerController(GWorld);
		if (PlayerController != nullptr)
		{
			PlayerController->SetPause(true);
		}
	}

	FScopeLock Lock(&EventHandlersCriticalSection);
	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppPause();
	}

	// Pause rendering
	FMagicLeapHMD * const HMD = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	if (HMD)
	{
		HMD->PauseRendering(true);
	}
}

void FAppFramework::ApplicationResumeDelegate()
{
	UE_LOG(LogMagicLeap, Log, TEXT("+++++++ ML AppFramework APP RESUME ++++++"));

	// Resume rendering
	// Resume rendering
	FMagicLeapHMD * const HMD = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	if (HMD)
	{
		HMD->PauseRendering(false);
	}

	if (GEngine)
	{
		APlayerController* PlayerController = GEngine->GetFirstLocalPlayerController(GWorld);
		if (PlayerController != nullptr)
		{
			PlayerController->SetPause(false);
		}

		GEngine->SetMaxFPS(saved_max_fps_);
	}

	FScopeLock Lock(&EventHandlersCriticalSection);
	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppResume();
	}
}

void FAppFramework::OnApplicationShutdown()
{
	FScopeLock Lock(&EventHandlersCriticalSection);
	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppShutDown();
	}
}

#if WITH_MLSDK
void FAppFramework::SetBaseCoordinateFrame(MLCoordinateFrameUID InBaseCoordinateFrame)
{
	base_coordinate_frame_ = InBaseCoordinateFrame;
	base_dirty_ = true;
}
#endif //WITH_MLSDK

void FAppFramework::SetBasePosition(const FVector& InBasePosition)
{
	base_position_ = InBasePosition;
	base_dirty_ = true;
}

void FAppFramework::SetBaseOrientation(const FQuat& InBaseOrientation)
{
	base_orientation_ = InBaseOrientation;
	base_dirty_ = true;
}

void FAppFramework::SetBaseRotation(const FRotator& InBaseRotation)
{
	base_orientation_ = InBaseRotation.Quaternion();
	base_dirty_ = true;
}

const FTrackingFrame* FAppFramework::GetCurrentFrame() const
{
	FMagicLeapHMD* hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	return hmd ? &(hmd->GetCurrentFrame()) : nullptr;
}

const FTrackingFrame* FAppFramework::GetOldFrame() const
{
	FMagicLeapHMD* hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	return hmd ? &(hmd->GetOldFrame()) : nullptr;
}

FVector2D FAppFramework::GetFieldOfView() const
{
	// TODO Pass correct values when graphics provides them through API.
	return FVector2D(80.0f, 60.0f);
}

uint32 FAppFramework::GetViewportCount() const
{
#if WITH_MLSDK
	const FTrackingFrame *frame = GetOldFrame();
	return frame ? frame->RenderInfoArray.num_virtual_cameras : 2;
#else
	return 1;
#endif //WITH_MLSDK
}

float FAppFramework::GetWorldToMetersScale() const
{
	const FTrackingFrame *frame = GetCurrentFrame();

	// If the frame is not ready, return the default system scale.
	if (!frame)
	{
		return GWorld ? GWorld->GetWorldSettings()->WorldToMeters : 100.0f;
	}

	return frame->WorldToMetersScale;
}

FTransform FAppFramework::GetCurrentFrameUpdatePose() const
{
	const FTrackingFrame* frame = GetCurrentFrame();
	return frame ? frame->RawPose : FTransform::Identity;
}

#if WITH_MLSDK
bool FAppFramework::GetTransform(const MLCoordinateFrameUID& Id, FTransform& OutTransform, EFailReason& OutReason) const
{
	const FTrackingFrame* frame = GetCurrentFrame();
	if (frame == nullptr)
	{
		OutReason = EFailReason::InvalidTrackingFrame;
		return false;
	}

	MLTransform transform = MagicLeap::kIdentityTransform;
	MLResult Result = MLSnapshotGetTransform(frame->Snapshot, &Id, &transform);
	if (Result == MLResult_Ok)
	{
		OutTransform = MagicLeap::ToFTransform(transform, GetWorldToMetersScale());
		if (OutTransform.ContainsNaN())
		{
			OutReason = EFailReason::NaNsInTransform;
			return false;
		}
		// Unreal crashes if the incoming quaternion is not normalized.
		if (!OutTransform.GetRotation().IsNormalized())
		{
			FQuat rotation = OutTransform.GetRotation();
			rotation.Normalize();
			OutTransform.SetRotation(rotation);
		}
		OutReason = EFailReason::None;
		return true;
	}
	else if (Result == MLSnapshotResult_PoseNotFound)
	{
		OutReason = EFailReason::PoseNotFound;
	}
	else
	{
		OutReason = EFailReason::CallFailed;
	}

	return false;
}
#endif //WITH_MLSDK

TSharedPtr<FCameraCaptureRunnable, ESPMode::ThreadSafe> FAppFramework::GetCameraCaptureRunnable()
{
	if (!CameraCaptureRunnable.IsValid())
	{
		CameraCaptureRunnable = MakeShared<FCameraCaptureRunnable, ESPMode::ThreadSafe>();
	}
	return CameraCaptureRunnable;
}

void FAppFramework::RefreshCameraCaptureRunnableReferences()
{
	// a reference count of 1 is a self reference
	if (CameraCaptureRunnable.GetSharedReferenceCount() == 1)
	{
		CameraCaptureRunnable.Reset();
	}
}

TSharedPtr<FImageTrackerRunnable, ESPMode::ThreadSafe> FAppFramework::GetImageTrackerRunnable()
{
	if (!ImageTrackerRunnable.IsValid())
	{
		ImageTrackerRunnable = MakeShared<FImageTrackerRunnable, ESPMode::ThreadSafe>();
	}

	return ImageTrackerRunnable;
}

void FAppFramework::RefreshImageTrackerRunnableReferences()
{
	// a reference count of 1 is a self reference
	if (ImageTrackerRunnable.GetSharedReferenceCount() == 1)
	{
		ImageTrackerRunnable.Reset();
	}
}

void FAppFramework::AddEventHandler(MagicLeap::IAppEventHandler* EventHandler)
{
	FScopeLock Lock(&EventHandlersCriticalSection);
	EventHandlers.Add(EventHandler);
}

void FAppFramework::RemoveEventHandler(MagicLeap::IAppEventHandler* EventHandler)
{
	FScopeLock Lock(&EventHandlersCriticalSection);
	EventHandlers.Remove(EventHandler);
}

bool FAppFramework::AsyncDestroy(MagicLeap::IAppEventHandler* InEventHandler)
{
	if (AsyncDestroyer != nullptr)
	{
		AsyncDestroyer->AddRaw(InEventHandler);
		return true;
	}

	return false;
}

void FAppFramework::RegisterMagicLeapModule(IMagicLeapModule* InModule)
{
	checkf(RegisteredModules.Find(InModule->GetName()) == nullptr, TEXT("MagicLeapModule %s has already been registered!"), *InModule->GetName().ToString());
	RegisteredModules.Add(InModule->GetName(), InModule);
}

void FAppFramework::UnregisterMagicLeapModule(IMagicLeapModule* InModule)
{
	RegisteredModules.Remove(InModule->GetName());
}

IMagicLeapModule* FAppFramework::GetMagicLeapModule(FName InName)
{
	IMagicLeapModule** MagicLeapModule = RegisteredModules.Find(InName);
	return MagicLeapModule ? *MagicLeapModule : nullptr;
}