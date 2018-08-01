// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	Instigates a capture image to file task on a separate thread.
	@brief The newly created jpeg file will have an automatically generated name which is guaranteed
		   to be unique.  The user should register event handlers for both the success and fail events.
		   Upon completion, a successful operation will provide the file path of the newly created jpeg
		   to the FCameraCaptureImgToFileSuccess event handler.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool CaptureImageToFileAsync();

  /** 
    Instigates a capture image to memory task on a speparate thread.
	@brief The user should register event handlers for both the success and fail events.  Upon completion,
		   a successful operation will provide a dynamically generated texture containing the captured
		   image to the FCameraCaptureImgToTextureSuccess event handler.
    @note The generated texture will be garbage collected when this class is destroyed.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool CaptureImageToTextureAsync();

  /** 
    Instigates a capture video to file task on a separate thread.
	@brief The newly created mp4 file will have an automatically generated name which is guaranteed
		   to be unique.  The user should register event handlers for both the success and fail events.
		   Upon completion, a successful operation will provide the file path of the saved mp4 file to the
		   FCameraCaptureVidToFileSuccess event handler.
	@param VideoLength The length in seconds of the footage to be captured.
  */
  UFUNCTION(BlueprintCallable, Category = "CameraCapture|MagicLeap")
  bool CaptureVideoToFileAsync(float VideoLength);

  /**
	Retrieves a handle to the current preview buffer.
	@note This call is thread safe.
	@return An MLHandle to the current preview buffer (can be invalid).
  */
  static int64 GetPreviewHandle();

public:
  /**
    Delegate used to pass log messages from the capture worker thread to the instigating blueprint.
	@note This is useful if the user wishes to have log messages in 3D space.
    @param LogMessage A string containing the log message.
  */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraCaptureLogMessage, FString, LogMessage);

  /**
    Delegate used to notify the instigating blueprint of a capture image to file success.
    @param FilePath A string containing the file path to the newly created jpeg.
  */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraCaptureImgToFileSuccess, FString, FilePath);

  /** Delegate used to notify the instigating blueprint of a capture image to file failure. */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCameraCaptureImgToFileFailure);

  /**
    Delegate used to pass the successfully captured image back to the instigating blueprint.
    @note The captured texture will be cleaned up when this component is destroyed.
    @param CaptureTexture A UTexture2D containing the captured image.
  */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraCaptureImgToTextureSuccess, UTexture2D*, CaptureTexture);

  /** Delegate used to notify the instigating blueprint of a capture image to texture failure. */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCameraCaptureImgToTextureFailure);

  /** Delegate used to notify the instigating blueprint of a capture video to file success.

    @param FilePath A string containing the path to the newly created mp4.
  */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraCaptureVidToFileSuccess, FString, FilePath);

  /** Delegate used to notify the instigating blueprint of a capture video to file failure. */
  DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCameraCaptureVidToFileFailure);

  /** Activated when a log message is generated on the capture thread. */
  FCameraCaptureLogMessage& OnCaptureLogMessage();

  /** Activated when an image is successfully captured to file on the capture thread. */
  FCameraCaptureImgToFileSuccess& OnCaptureImgToFileSuccess();

  /** Activated when an image fails to capture to file on the capture thread. */
  FCameraCaptureImgToFileFailure& OnCaptureImgToFileFailure();

  /** Activated when an image is successfully captured to texture on the capture thread. */
  FCameraCaptureImgToTextureSuccess& OnCaptureImgToTextureSuccess();

  /** Activated when an image fails to capture to texture on the capture thread. */
  FCameraCaptureImgToTextureFailure& OnCaptureImgToTextureFailure();

  /** Activated when a video is successfully captured to file on the capture thread. */
  FCameraCaptureVidToFileSuccess& OnCaptureVidToFileSuccess();

  /** Activated when a video is fails to capture to file on the capture thread. */
  FCameraCaptureVidToFileFailure& OnCaptureVidToFileFailure();  

private:
  class FImageCaptureImpl *Impl;
  bool bCapturing;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureLogMessage CaptureLogMessage;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureImgToFileSuccess CaptureImgToFileSuccess;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureImgToFileFailure CaptureImgToFileFailure;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureImgToTextureSuccess CaptureImgToTextureSuccess;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureImgToTextureFailure CaptureImgToTextureFailure;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureVidToFileSuccess CaptureVidToFileSuccess;

  UPROPERTY(BlueprintAssignable, Category = "CameraCapture|MagicLeap", meta = (AllowPrivateAccess = true))
  FCameraCaptureVidToFileFailure CaptureVidToFileFailure;

private:
  void Log(const FString& LogMessage);
};

DECLARE_LOG_CATEGORY_EXTERN(LogCameraCapture, Verbose, All);