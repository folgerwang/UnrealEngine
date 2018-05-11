// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitTextures.h"

UAppleARKitTextureCameraImage::UAppleARKitTextureCameraImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if PLATFORM_MAC || PLATFORM_IOS
	, CameraImage(nullptr)
#endif
{
	ExternalTextureGuid = FGuid::NewGuid();
}

FTextureResource* UAppleARKitTextureCameraImage::CreateResource()
{
	// @todo joeg -- hook this up for rendering
	return nullptr;
}

void UAppleARKitTextureCameraImage::BeginDestroy()
{
#if PLATFORM_MAC || PLATFORM_IOS
	if (CameraImage != nullptr)
	{
		CFRelease(CameraImage);
		CameraImage = nullptr;
	}
#endif
	Super::BeginDestroy();
}

#if SUPPORTS_ARKIT_1_0

void UAppleARKitTextureCameraImage::Init(float InTimestamp, CVPixelBufferRef InCameraImage)
{
	// Handle the case where this UObject is being reused
	if (CameraImage != nullptr)
	{
		CFRelease(CameraImage);
		CameraImage = nullptr;
	}

	if (InCameraImage != nullptr)
	{
		Timestamp = InTimestamp;
		CameraImage = InCameraImage;
		CFRetain(CameraImage);
		Size.X = CVPixelBufferGetWidth(CameraImage);
		Size.Y = CVPixelBufferGetHeight(CameraImage);
	}
}

#endif

UAppleARKitTextureCameraDepth::UAppleARKitTextureCameraDepth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if PLATFORM_MAC || PLATFORM_IOS
	, CameraDepth(nullptr)
#endif
{
	ExternalTextureGuid = FGuid::NewGuid();
}

FTextureResource* UAppleARKitTextureCameraDepth::CreateResource()
{
	// @todo joeg -- hook this up for rendering
	return nullptr;
}

void UAppleARKitTextureCameraDepth::BeginDestroy()
{
#if PLATFORM_MAC || PLATFORM_IOS
	if (CameraDepth != nullptr)
	{
		CFRelease(CameraDepth);
		CameraDepth = nullptr;
	}
#endif
	Super::BeginDestroy();
}

#if SUPPORTS_ARKIT_1_0

void UAppleARKitTextureCameraDepth::Init(float InTimestamp, AVDepthData* InCameraDepth)
{
// @todo joeg -- finish this
	Timestamp = InTimestamp;
}

#endif
