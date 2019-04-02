// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "CameraCaptureComponent.generated.h"

/**
  The CameraCaptureComponent provides access to and maintains state for camera capture functionality.
  The connection to the device's camera is managed internally.  Users of this component
  are able to asynchronously capture camera images and footage to file.  Alternatively,
  a camera image can be captured directly to texture.  The user need only make the relevant
  asynchronous call and then register the appropriate success/fail event handlers for the
  operation's completion.
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UCameraCaptureComponent : public UActorComponent
{
  GENERATED_BODY()

public:
  UCameraCaptureComponent();
  virtual ~UCameraCaptureComponent();

  /** Intializes the asynchronous capture system. */
  void BeginPlay() override;
  
  /** Polls for and handles incoming messages from the asynchronous capture system */
  virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

  /**
   Delegate used to notify the initiating blueprint when a capture image to file task has completed.
   @note Although this signals the task as complete, it may have failed or been cancelled.
   @param bSuccess True if the task succeeded, false otherwise.
   @param FilePath A string containing the file path to the newly created jpeg.
 */
  DECLARE_DYNAMIC_DELEGATE_TwoParams(FCameraCaptureImgToFile, bool, bSuccess, const FString&, FilePath);

  /**
	Delegate used to pass the captured image back to the initiating blueprint.
	@note The captured texture will remain in memory for the lifetime of the calling application (if the task succeeds).
	@param bSuccess True if the task succeeded, false otherwise.
	@param CaptureTexture A UTexture2D containing the captured image.
  */
  DECLARE_DYNAMIC_DELEGATE_TwoParams(FCameraCaptureImgToTexture, bool, bSuccess, UTexture2D*, CaptureTexture);

  /**
	Delegate used to notify the initiating blueprint of the result of a request to begin recording video.
	@note Although this signals the task as complete, it may have failed or been cancelled.
	@param bSuccess True if the task succeeded, false otherwise.
  */
  DECLARE_DYNAMIC_DELEGATE_OneParam(FCameraCaptureStartRecording, bool, bSuccess);

  /**
	Delegate used to notify the initiating blueprint of the result of a request to stop recording video.
	@note Although this signals the task as complete, it may have failed or been cancelled.
	@param bSuccess True if the task succeeded, false otherwise.
	@param FilePath A string containing the path to the newly created mp4.
  */
  DECLARE_DYNAMIC_DELEGATE_TwoParams(FCameraCaptureStopRecording, bool, bSuccess, const FString&, FilePath);

  /**
	Initiates a capture image to file task on a separate thread.
	@brief The newly created jpeg file will have an automatically generated name which is guaranteed
		   to be unique.  Upon completion, a successful operation will provide the file path of the newly
		   created jpeg to the FCameraCaptureImgToFile event handler.
	@param ResultDelegate The delegate to be notified once the camera image has been saved to a jpeg file.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool CaptureImageToFileAsync(const FCameraCaptureImgToFile& ResultDelegate);

  /** 
    Initiates a capture image to memory task on a speparate thread.
	@brief The user should register event handlers for both the success and fail events.  Upon completion,
		   a successful operation will provide a dynamically generated texture containing the captured
		   image to the FCameraCaptureImgToTextureSuccess event handler.
    @note The generated texture will be garbage collected when this app is destroyed.
	@param ResultDelegate The delegate to be notified once the camera image has been saved to a texture.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool CaptureImageToTextureAsync(const FCameraCaptureImgToTexture& ResultDelegate);

  /** 
    Initiates the capturing of video/audio data on a separate thread.
	@note The system will continue to record video until StopRecordingVideo is called.
	@param ResultDelegate The delegate to be notified once the recording has begun or failed to begin.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool StartRecordingVideoAsync(const FCameraCaptureStartRecording& ResultDelegate);

  /**
	Stops the recording and saves the video/audio data to an mp4 file.
	@note The newly created mp4 file will have an automatically generated name which is guaranteed
		  to be unique.
	@param ResultDelegate The delegate to be notified once the video/audio data has been saved to an mp4 file.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool StopRecordingVideoAsync(const FCameraCaptureStopRecording& ResultDelegate);

  /**
	Gets the capture state of the component.
	@return True if the component is currently capturing, false otherwise.
  */
  UFUNCTION(BlueprintPure, Category = "CameraCapture|MagicLeap")
  bool IsCapturing() const;

  /**
	Retrieves a handle to the current preview buffer.
	@note This call is thread safe.
	@return An MLHandle to the current preview buffer (can be invalid).
  */
  static int64 GetPreviewHandle();

public:
  /**
    Delegate used to pass log messages from the capture worker thread to the initiating blueprint.
	@note This is useful if the user wishes to have log messages in 3D space.
    @param LogMessage A string containing the log message.
  */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraCaptureLogMessage, FString, LogMessage);

private:
	class FCameraCaptureImpl *Impl;
	UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
	FCameraCaptureLogMessage CaptureLogMessage;
	FCameraCaptureImgToFile CaptureImgToFileResult;
	FCameraCaptureImgToTexture CaptureImgToTextureResult;
	FCameraCaptureStartRecording StartRecordingResult;
	FCameraCaptureStopRecording StopRecordingResult;

private:
  void Log(const FString& LogMessage);
};

DECLARE_LOG_CATEGORY_EXTERN(LogCameraCapture, Verbose, All);