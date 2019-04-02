// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/Engine.h"
#include "MagicLeapCameraTypes.generated.h"

USTRUCT()
struct FMagicLeapCameraDummyStruct { GENERATED_BODY() };

/** Delegate used to notify the initiating blueprint when the camera connect task has completed. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FCameraConnect, const bool, bSuccess);

/** Delegate used to notify the initiating blueprint when the camera disonnect task has completed. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FCameraDisconnect, const bool, bSuccess);

/**
   Delegate used to notify the initiating blueprint when a capture image to file task has completed.
   @note Although this signals the task as complete, it may have failed or been cancelled.
   @param bSuccess True if the task succeeded, false otherwise.
   @param FilePath A string containing the file path to the newly created jpeg.
*/
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCameraCaptureImgToFile, const bool, bSuccess, const FString&, FilePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCameraCaptureImgToFileMulti, const bool, bSuccess, const FString&, FilePath);

/**
	Delegate used to pass the captured image back to the initiating blueprint.
	@note The captured texture will remain in memory for the lifetime of the calling application (if the task succeeds).
	@param bSuccess True if the task succeeded, false otherwise.
	@param CaptureTexture A UTexture2D containing the captured image.
*/
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCameraCaptureImgToTexture, const bool, bSuccess, UTexture2D*, CaptureTexture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCameraCaptureImgToTextureMulti, const bool, bSuccess, UTexture2D*, CaptureTexture);

/**
	Delegate used to notify the initiating blueprint of the result of a request to begin recording video.
	@note Although this signals the task as complete, it may have failed or been cancelled.
	@param bSuccess True if the task succeeded, false otherwise.
*/
DECLARE_DYNAMIC_DELEGATE_OneParam(FCameraStartRecording, const bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraStartRecordingMulti, const bool, bSuccess);

/**
	Delegate used to notify the initiating blueprint of the result of a request to stop recording video.
	@note Although this signals the task as complete, it may have failed or been cancelled.
	@param bSuccess True if the task succeeded, false otherwise.
	@param FilePath A string containing the path to the newly created mp4.
*/
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCameraStopRecording, const bool, bSuccess, const FString&, FilePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCameraStopRecordingMulti, const bool, bSuccess, const FString&, FilePath);

/**
	Delegate used to pass log messages from the capture worker thread to the initiating blueprint.
	@note This is useful if the user wishes to have log messages in 3D space.
	@param LogMessage A string containing the log message.
*/
DECLARE_DYNAMIC_DELEGATE_OneParam(FCameraLogMessage, const FString&, LogMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraLogMessageMulti, const FString&, LogMessage);
