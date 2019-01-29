// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapCameraFunctionLibrary.h"
#include "MagicLeapCameraPlugin.h"

bool UMagicLeapCameraFunctionLibrary::CameraConnect(const FCameraConnect& ResultDelegate)
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->CameraConnect(ResultDelegate);
}

bool UMagicLeapCameraFunctionLibrary::CameraDisconnect(const FCameraDisconnect& ResultDelegate)
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->CameraDisconnect(ResultDelegate);
}

bool UMagicLeapCameraFunctionLibrary::CaptureImageToFileAsync(const FCameraCaptureImgToFile& InResultDelegate)
{
	FCameraCaptureImgToFileMulti ResultDelegate;
	ResultDelegate.Add(InResultDelegate);
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->CaptureImageToFileAsync(ResultDelegate);
}

bool UMagicLeapCameraFunctionLibrary::CaptureImageToTextureAsync(const FCameraCaptureImgToTexture& InResultDelegate)
{
	FCameraCaptureImgToTextureMulti ResultDelegate;
	ResultDelegate.Add(InResultDelegate);
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->CaptureImageToTextureAsync(ResultDelegate);
}

bool UMagicLeapCameraFunctionLibrary::StartRecordingAsync(const FCameraStartRecording& InResultDelegate)
{
	FCameraStartRecordingMulti ResultDelegate;
	ResultDelegate.Add(InResultDelegate);
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->StartRecordingAsync(ResultDelegate);
}

bool UMagicLeapCameraFunctionLibrary::StopRecordingAsync(const FCameraStopRecording& InResultDelegate)
{
	FCameraStopRecordingMulti ResultDelegate;
	ResultDelegate.Add(InResultDelegate);
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->StopRecordingAsync(ResultDelegate);
}

bool UMagicLeapCameraFunctionLibrary::SetLogDelegate(const FCameraLogMessage& InLogDelegate)
{
	FCameraLogMessageMulti LogDelegate;
	LogDelegate.Add(InLogDelegate);
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->SetLogDelegate(LogDelegate);
}

bool UMagicLeapCameraFunctionLibrary::IsCapturing()
{
	return GET_MAGIC_LEAP_CAMERA_PLUGIN()->IsCapturing();
}
