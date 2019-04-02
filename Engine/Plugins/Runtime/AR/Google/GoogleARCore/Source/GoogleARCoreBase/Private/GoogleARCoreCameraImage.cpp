// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreCameraImage.h"

#include "GoogleARCoreAPI.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Ndk/NdkImageAPI.h"
#endif

UGoogleARCoreCameraImage::~UGoogleARCoreCameraImage()
{
	Release();
}

void UGoogleARCoreCameraImage::Release()
{
#if PLATFORM_ANDROID
	if (ArImage)
	{
		NdkImage = nullptr;
		ArImage_release(ArImage);
		ArImage = nullptr;
	}
#endif
}

int32 UGoogleARCoreCameraImage::GetWidth() const
{
	int32_t Width = 0;
#if PLATFORM_ANDROID
	if (NdkImage)
	{
		AImage_getWidth_dynamic(NdkImage, &Width);
	}
#endif
	return Width;
}

int32 UGoogleARCoreCameraImage::GetHeight() const
{
	int32_t Height = 0;
#if PLATFORM_ANDROID
	if (NdkImage)
	{
		AImage_getHeight_dynamic(NdkImage, &Height);
	}
#endif
	return Height;
}


int32 UGoogleARCoreCameraImage::GetPlaneCount() const
{
	int32_t PlaneCount = 0;
#if PLATFORM_ANDROID
	if (NdkImage)
	{
		AImage_getNumberOfPlanes_dynamic(NdkImage, &PlaneCount);
	}
#endif
	return PlaneCount;
}

uint8 *UGoogleARCoreCameraImage::GetPlaneData(
	int32 Plane, int32 &PixelStride,
	int32 &RowStride, int32 &DataLength)
{
	uint8_t *PlaneData = nullptr;
#if PLATFORM_ANDROID
	AImage_getPlanePixelStride_dynamic(NdkImage, Plane, &PixelStride);
	AImage_getPlaneRowStride_dynamic(NdkImage, Plane, &RowStride);
	AImage_getPlaneData_dynamic(
		NdkImage, Plane,
		&PlaneData, &DataLength);
#endif
	return PlaneData;
}
