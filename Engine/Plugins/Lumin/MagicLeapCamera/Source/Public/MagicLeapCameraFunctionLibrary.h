// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapCameraTypes.h"
#include "MagicLeapCameraFunctionLibrary.generated.h"

/**
  The MagicLeapCameraFunctionLibrary provides access to the camera capture functionality.
  Users of this library are able to asynchronously capture camera images and footage to file.
  Alternatively, a camera image can be captured directly to texture.  The user need only make
  the relevant asynchronous call and then register the appropriate event handlers for the
  operation's completion.
*/
UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPCAMERA_API UMagicLeapCameraFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
		Establishes a connection with the device's camera.
		@note A connection will be made automatically upon the first capture call if this is
			  not called first.  Calling this function manually allows the developer to control
			  when privilege notifications for this plugin will be activated (if application
			  is being used for the first time).
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool CameraConnect(const FCameraConnect& ResultDelegate);

	/** 
		Disconnects from the device's camera.
		@note This function must be called before the application terminates (if the camera has
			  been connected to).  Failure to do so will result in the camera connection remaining
			  open (and the camera icon remaining on screen).
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool CameraDisconnect(const FCameraDisconnect& ResultDelegate);

	/**
		Initiates a capture image to file task on a separate thread.
		@brief The newly created jpeg file will have an automatically generated name which is guaranteed
			   to be unique.  Upon completion, a successful operation will provide the file path of the newly
			   created jpeg to the FCameraCaptureImgToFile event handler.
		@param ResultDelegate The delegate to be notified once the camera image has been saved to a jpeg file.
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool CaptureImageToFileAsync(const FCameraCaptureImgToFile& ResultDelegate);

	/**
		Initiates a capture image to memory task on a speparate thread.
		@brief The user should register event handlers for both the success and fail events.  Upon completion,
			   a successful operation will provide a dynamically generated texture containing the captured
			   image to the FCameraCaptureImgToTextureSuccess event handler.
		@note The generated texture will be garbage collected when this app is destroyed.
		@param ResultDelegate The delegate to be notified once the camera image has been saved to a texture.
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool CaptureImageToTextureAsync(const FCameraCaptureImgToTexture& ResultDelegate);

	/**
		Initiates the capturing of video/audio data on a separate thread.
		@note The system will continue to record video until StopRecordingVideo is called.
		@param ResultDelegate The delegate to be notified once the recording has begun or failed to begin.
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool StartRecordingAsync(const FCameraStartRecording& ResultDelegate);

	/**
		Stops the recording and saves the video/audio data to an mp4 file.
		@note The newly created mp4 file will have an automatically generated name which is guaranteed
			  to be unique.
		@param ResultDelegate The delegate to be notified once the video/audio data has been saved to an mp4 file.
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool StopRecordingAsync(const FCameraStopRecording& ResultDelegate);

	/**
		Sets the delegate by which the system can pass log messages back to the calling blueprint.
		@param LogDelegate The delegate by which the system will return log messages to the calling blueprint.
		@return True if the call succeeds, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Camera Function Library | MagicLeap")
	static bool SetLogDelegate(const FCameraLogMessage& LogDelegate);

	/**
		Gets the capture state of the component.
		@return True if the component is currently capturing, false otherwise.
	*/
	UFUNCTION(BlueprintPure, Category = "Camera Function Library | MagicLeap")
	static bool IsCapturing();
};
