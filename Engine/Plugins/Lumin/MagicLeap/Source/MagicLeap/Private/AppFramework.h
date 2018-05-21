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

#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START

#include <ml_api.h>

#if PLATFORM_LUMIN
#include <vulkan.h>
#include <ml_graphics.h>
#else
#include <ml_graphics.h>
#endif // PLATFORM_LUMIN

#include <ml_coordinate_frame_uid.h>

ML_INCLUDES_END
#endif  //WITH_MLSDK

#include "CoreMinimal.h"
#include "AppEventHandler.h"
#include "AsyncDestroyer.h"

class FMagicLeapHMD;
struct FTrackingFrame;

enum class EFailReason : uint8
{
	None,
	InvalidTrackingFrame,
	NaNsInTransform,
	CallFailed
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

	/** Returns @true if we retrieved the device's resolution. Otherwise, it uses the default resolution. */
	bool GetDeviceResolution(FVector2D& out_resolution) const;
	FTransform GetDisplayCenterTransform() const { return FTransform::Identity; }; // HACK
	uint32 GetViewportCount() const;

	float GetWorldToMetersScale() const;
	FTransform GetCurrentFrameUpdatePose() const;
#if WITH_MLSDK
	bool GetTransform(const MLCoordinateFrameUID& Id, FTransform& OutTransform, EFailReason& OutReason) const;
#endif //WITH_MLSDK

	static void AddEventHandler(MagicLeap::IAppEventHandler* InEventHandler);
	static void RemoveEventHandler(MagicLeap::IAppEventHandler* InEventHandler);
	static void AsyncDestroy(MagicLeap::IAppEventHandler* InEventHandler);

private:
	FTrackingFrame* GetCurrentFrame() const;
	FTrackingFrame* GetOldFrame() const;

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
};

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeap, Log, All);