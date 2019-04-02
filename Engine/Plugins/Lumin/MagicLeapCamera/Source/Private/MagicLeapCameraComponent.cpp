// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapCameraComponent.h"
#include "MagicLeapCameraPlugin.h"

void UMagicLeapCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	GET_MAGIC_LEAP_CAMERA_PLUGIN()->IncUserCount();
	GET_MAGIC_LEAP_CAMERA_PLUGIN()->SetLogDelegate(OnLogMessage);
}

void UMagicLeapCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GET_MAGIC_LEAP_CAMERA_PLUGIN()->DecUserCount();
	Super::EndPlay(EndPlayReason);
}

bool UMagicLeapCameraComponent::CaptureImageToFileAsync()
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->CaptureImageToFileAsync(OnCaptureImgToFile);
}

bool UMagicLeapCameraComponent::CaptureImageToTextureAsync()
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->CaptureImageToTextureAsync(OnCaptureImgToTexture);
}

bool UMagicLeapCameraComponent::StartRecordingAsync()
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->StartRecordingAsync(OnStartRecording);
}

bool UMagicLeapCameraComponent::StopRecordingAsync()
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->StopRecordingAsync(OnStopRecording);
}

bool UMagicLeapCameraComponent::IsCapturing() const
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->IsCapturing();
}
