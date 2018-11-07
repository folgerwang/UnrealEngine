// Copyright 2018 Google Inc.

#pragma once

#include "Engine/EngineBaseTypes.h"

#include "GoogleARCoreTypes.h"
#include "GoogleARCoreCameraIntrinsics.generated.h"

#if PLATFORM_ANDROID
#include "arcore_c_api.h"
#endif

class FGoogleARCoreSession;

/**
 * An object wrapping the ArCameraIntrinsics data from the ARCore SDK.
 */
UCLASS(BlueprintType)
class UGoogleARCoreCameraIntrinsics : public UObject
{
	GENERATED_BODY()
public:

	virtual ~UGoogleARCoreCameraIntrinsics();

	/**
	 * Get the focal length in pixels.
	 *
	 * @param OutFX  The focal length on the X axis.
	 * @param OutFY  The focal length on the Y axis.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|CameraIntrinsics")
	void GetFocalLength(float &OutFX, float &OutFY);

	/**
	 * Get the principal point in pixels.
	 *
	 * @param OutCX  The principle point on the X axis.
	 * @param OutCY  The principle point the Y axis.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|CameraIntrinsics")
	void GetPrincipalPoint(float &OutCX, float &OutCY);

	/**
	 * Get the image's width and height in pixels.
	 *
	 * @param OutWidth   The width.
	 * @param OutHeight  The height.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|CameraIntrinsics")
	void GetImageDimensions(int32 &OutWidth, int32 &OutHeight);

private:
#if PLATFORM_ANDROID
	ArCameraIntrinsics *NativeCameraIntrinsics = nullptr;
	TWeakPtr<FGoogleARCoreSession> Session;
	friend class FGoogleARCoreSession;
	friend class FGoogleARCoreFrame;
#endif
};

