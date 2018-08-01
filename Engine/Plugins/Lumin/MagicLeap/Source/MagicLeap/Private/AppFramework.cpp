// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppFramework.h"
#include "MagicLeapHMD.h"
#include "GameFramework/WorldSettings.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_snapshot.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

TArray<MagicLeap::IAppEventHandler*> FAppFramework::EventHandlers;
FCriticalSection FAppFramework::EventHandlersCriticalSection;
MagicLeap::FAsyncDestroyer* FAppFramework::AsyncDestroyer = nullptr;

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

	{
		FScopeLock Lock(&EventHandlersCriticalSection);
	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppStartup();
	}
	}

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
	}
#endif //WITH_MLSDK
}

void FAppFramework::ApplicationPauseDelegate()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("+++++++ AppFramework APP PAUSE ++++++"));

	FlushRenderingCommands();

	FMagicLeapHMD * hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;

	if (hmd && hmd->GetActiveCustomPresent())
	{
		hmd->GetActiveCustomPresent()->Reset();
	}

	if (GEngine)
	{
		saved_max_fps_ = GEngine->GetMaxFPS();
		GEngine->SetMaxFPS(0.0f);
	}

	FScopeLock Lock(&EventHandlersCriticalSection);
	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppPause();
	}
}

void FAppFramework::ApplicationResumeDelegate()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("+++++++ MLContext APP RESUME ++++++"));
	if (GEngine)
	{
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

FTrackingFrame* FAppFramework::GetCurrentFrame() const
{
	FMagicLeapHMD* hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	return hmd ? hmd->GetCurrentFrame() : nullptr;
}

FTrackingFrame* FAppFramework::GetOldFrame() const
{
	FMagicLeapHMD* hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	return hmd ? hmd->GetOldFrame() : nullptr;
}

FVector2D FAppFramework::GetFieldOfView() const
{
	// TODO Pass correct values when graphics provides them through API.
	return FVector2D(80.0f, 60.0f);
}

bool FAppFramework::GetDeviceResolution(FVector2D& OutResolution) const
{
#if WITH_MLSDK
	// JMC: The size of the back buffer should not change, rather the internal
	// viewport should render to a sub region of the back buffer.
	// This will happen automatically once the render pipeline draws directly
	// to our provided color buffer.
	/*const FTrackingFrame *frame = GetOldFrame();

	if (frame && frame->RenderInfoArray.num_virtual_cameras > 0)
	{
		const float width = frame->RenderInfoArray.viewport.w * 2.0f;
		const float height = frame->RenderInfoArray.viewport.h;
		OutResolution = FVector2D(width, height);
		return true;
	}*/
#endif //WITH_MLSDK

	OutResolution = FVector2D(1280.0f * 2.0f, 960.0f);
	return true;
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
	FTrackingFrame* frame = GetCurrentFrame();
	return frame ? frame->RawPose : FTransform::Identity;
}

#if WITH_MLSDK
bool FAppFramework::GetTransform(const MLCoordinateFrameUID& Id, FTransform& OutTransform, EFailReason& OutReason) const
{
	FTrackingFrame* frame = GetCurrentFrame();
	if (frame == nullptr)
	{
		OutReason = EFailReason::InvalidTrackingFrame;
		return false;
	}

	MLTransform transform = MagicLeap::kIdentityTransform;
	if (MLSnapshotGetTransform(frame->Snapshot, &Id, &transform))
	{
		OutTransform = MagicLeap::ToFTransform(transform, GetWorldToMetersScale());
		if (OutTransform.ContainsNaN())
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLSnapshotGetTransform() returned an invalid transform with NaNs."));
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
	OutReason = EFailReason::CallFailed;
	return false;
}
#endif //WITH_MLSDK

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

void FAppFramework::AsyncDestroy(MagicLeap::IAppEventHandler* InEventHandler)
{
	AsyncDestroyer->AddRaw(InEventHandler);
}