// Copyright 2017 Google Inc.

#include "GoogleARCoreCameraImage.h"

#include "GoogleARCoreAPI.h"

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#if PLATFORM_USED_NDK_VERSION_INTEGER >= NDK_IMAGE_VERSION_INTEGER
#include "media/NdkImage.h"
#endif

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
#if PLATFORM_ANDROID && (PLATFORM_USED_NDK_VERSION_INTEGER >= NDK_IMAGE_VERSION_INTEGER)
	if (NdkImage)
	{
		AImage_getWidth(NdkImage, &Width);
	}
#endif
	return Width;
}

int32 UGoogleARCoreCameraImage::GetHeight() const
{
	int32_t Height = 0;
#if PLATFORM_ANDROID && (PLATFORM_USED_NDK_VERSION_INTEGER >= NDK_IMAGE_VERSION_INTEGER)
	if (NdkImage)
	{
		AImage_getHeight(NdkImage, &Height);
	}
#endif
	return Height;
}


int32 UGoogleARCoreCameraImage::GetPlaneCount() const
{
	int32_t PlaneCount = 0;
#if PLATFORM_ANDROID && (PLATFORM_USED_NDK_VERSION_INTEGER >= NDK_IMAGE_VERSION_INTEGER)
	if (NdkImage)
	{
		AImage_getNumberOfPlanes(NdkImage, &PlaneCount);
	}
#endif
	return PlaneCount;
}

uint8 *UGoogleARCoreCameraImage::GetPlaneData(
	int32 Plane, int32 &PixelStride,
	int32 &RowStride, int32 &DataLength)
{
	uint8_t *PlaneData = nullptr;
#if PLATFORM_ANDROID && (PLATFORM_USED_NDK_VERSION_INTEGER >= NDK_IMAGE_VERSION_INTEGER)
	AImage_getPlanePixelStride(NdkImage, Plane, &PixelStride);
	AImage_getPlaneRowStride(NdkImage, Plane, &RowStride);
	AImage_getPlaneData(
		NdkImage, Plane,
		&PlaneData, &DataLength);
#endif
	return PlaneData;
}
