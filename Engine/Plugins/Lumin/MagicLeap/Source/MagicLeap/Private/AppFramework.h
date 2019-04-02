// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START

#include <ml_api.h>
#include <ml_coordinate_frame_uid.h>

ML_INCLUDES_END
#endif  //WITH_MLSDK

#include "CoreMinimal.h"
#include "AppEventHandler.h"
#include "AsyncDestroyer.h"
#include "CameraCaptureRunnable.h"
#include "ImageTrackerRunnable.h"

class FMagicLeapHMD;
class IMagicLeapModule;
struct FTrackingFrame;

enum class EFailReason : uint8
{
	None,
	InvalidTrackingFrame,
	NaNsInTransform,
	CallFailed,
	PoseNotFound
};

class MAGICLEAP_API FAppFramework
{
public:
	FAppFramework();
	~FAppFramework();

	void Startup();
	void Shutdown();

	void BeginUpdate();

	bool IsInitialized() const { return bInitialized; }

	void ApplicationPauseDelegate();
	void ApplicationResumeDelegate();
	void OnApplicationShutdown();

#if WITH_MLSDK
	void SetBaseCoordinateFrame(MLCoordinateFrameUID InBaseCoordinateFrame);
	MLCoordinateFrameUID GetBaseCoordinateFrame() const { return base_coordinate_frame_; }
#endif //WITH_MLSDK

	void SetBasePosition(const FVector& InBasePosition);
	FVector GetBasePosition() const { return base_position_; }

	void SetBaseOrientation(const FQuat& InBaseOrientation);
	FQuat GetBaseOrientation() const { return base_orientation_; }

	void SetBaseRotation(const FRotator& InBaseRotation);
	FRotator GetBaseRotation() const { return FRotator(base_orientation_); }

	FVector2D GetFieldOfView() const;

	FTransform GetDisplayCenterTransform() const { return FTransform::Identity; }; // HACK
	uint32 GetViewportCount() const;

	float GetWorldToMetersScale() const;
	FTransform GetCurrentFrameUpdatePose() const;
#if WITH_MLSDK
	bool GetTransform(const MLCoordinateFrameUID& Id, FTransform& OutTransform, EFailReason& OutReason) const;
#endif //WITH_MLSDK

	TSharedPtr<FCameraCaptureRunnable, ESPMode::ThreadSafe> GetCameraCaptureRunnable();
	void RefreshCameraCaptureRunnableReferences();
	TSharedPtr<FImageTrackerRunnable, ESPMode::ThreadSafe> GetImageTrackerRunnable();
	void RefreshImageTrackerRunnableReferences();

	static void AddEventHandler(MagicLeap::IAppEventHandler* InEventHandler);
	static void RemoveEventHandler(MagicLeap::IAppEventHandler* InEventHandler);
	static bool AsyncDestroy(MagicLeap::IAppEventHandler* InEventHandler);

	static void RegisterMagicLeapModule(IMagicLeapModule* InModule);
	static void UnregisterMagicLeapModule(IMagicLeapModule* InModule);
	static IMagicLeapModule* GetMagicLeapModule(FName InName);

private:
	const FTrackingFrame* GetCurrentFrame() const;
	const FTrackingFrame* GetOldFrame() const;

	bool bInitialized = false;

#if WITH_MLSDK
	MLCoordinateFrameUID base_coordinate_frame_;
#endif //WITH_MLSDK

	FVector base_position_;
	FQuat base_orientation_;
	bool base_dirty_;

	float saved_max_fps_;

	static TArray<MagicLeap::IAppEventHandler*> EventHandlers;
	static FCriticalSection EventHandlersCriticalSection;
	static MagicLeap::FAsyncDestroyer* AsyncDestroyer;
	TSharedPtr<FCameraCaptureRunnable, ESPMode::ThreadSafe> CameraCaptureRunnable;
	TSharedPtr<FImageTrackerRunnable, ESPMode::ThreadSafe> ImageTrackerRunnable;
	static TMap<FName, IMagicLeapModule*> RegisteredModules;
};

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeap, Log, All);