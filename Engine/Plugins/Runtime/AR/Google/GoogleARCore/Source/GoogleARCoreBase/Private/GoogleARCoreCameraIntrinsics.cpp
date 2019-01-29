// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreCameraIntrinsics.h"

#if PLATFORM_ANDROID
#include "arcore_c_api.h"
#endif

#include "GoogleARCoreAPI.h"

UGoogleARCoreCameraIntrinsics::~UGoogleARCoreCameraIntrinsics()
{
#if PLATFORM_ANDROID
	if (NativeCameraIntrinsics)
	{
		ArCameraIntrinsics_destroy(NativeCameraIntrinsics);
	}
#endif
}

void UGoogleARCoreCameraIntrinsics::GetFocalLength(float &OutFX, float &OutFY)
{
#if PLATFORM_ANDROID
	TSharedPtr<FGoogleARCoreSession> SessionPtr = Session.Pin();
	if (NativeCameraIntrinsics && SessionPtr.IsValid())
	{
		ArCameraIntrinsics_getFocalLength(
			SessionPtr->GetHandle(), NativeCameraIntrinsics,
			&OutFX, &OutFY);
	}
#endif
}

void UGoogleARCoreCameraIntrinsics::GetPrincipalPoint(float &OutCX, float &OutCY)
{
#if PLATFORM_ANDROID
	TSharedPtr<FGoogleARCoreSession> SessionPtr = Session.Pin();
	if (NativeCameraIntrinsics && SessionPtr.IsValid())
	{
		ArCameraIntrinsics_getPrincipalPoint(
			SessionPtr->GetHandle(), NativeCameraIntrinsics,
			&OutCX, &OutCY);
	}
#endif
}

void UGoogleARCoreCameraIntrinsics::GetImageDimensions(int32 &OutWidth, int32 &OutHeight)
{
#if PLATFORM_ANDROID
	TSharedPtr<FGoogleARCoreSession> SessionPtr = Session.Pin();
	if (NativeCameraIntrinsics && SessionPtr.IsValid())
	{
		ArCameraIntrinsics_getImageDimensions(
			SessionPtr->GetHandle(), NativeCameraIntrinsics,
			&OutWidth, &OutHeight);
	}
#endif
}
