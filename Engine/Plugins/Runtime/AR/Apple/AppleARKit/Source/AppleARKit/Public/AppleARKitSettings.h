// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ARTrackable.h"
#include "Engine/TimecodeProvider.h"

#include "AppleImageUtilsTypes.h"
#include "AppleARKitTimecodeProvider.h"

#include "AppleARKitSettings.generated.h"

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARFaceTrackingFileWriterType : uint8
{
	/** Disables creation of a file writer */
	None,
	/** Comma delimited file, one row per captured frame */
	CSV,
	/** JSON object array, one frame object per captured frame */
	JSON
};


UCLASS(Config=Engine, defaultconfig)
class APPLEARKIT_API UAppleARKitSettings :
	public UObject
{
	GENERATED_BODY()

public:
	UAppleARKitSettings()
		: bEnableLiveLinkForFaceTracking(false)
		, bFaceTrackingWriteEachFrame(false)
		, FaceTrackingFileWriterType(EARFaceTrackingFileWriterType::None)
		, bShouldWriteCameraImagePerFrame(false)
		, WrittenCameraImageScale(1.f)
		, WrittenCameraImageQuality(85)
		, LiveLinkPublishingPort(11111)
		, DefaultFaceTrackingLiveLinkSubjectName(FName("iPhoneXFaceAR"))
		, DefaultFaceTrackingDirection(EARFaceTrackingDirection::FaceRelative)
		, bAdjustThreadPrioritiesDuringARSession(false)
		, GameThreadPriorityOverride(47)
		, RenderThreadPriorityOverride(45)
		, ARKitTimecodeProvider(TEXT("/Script/AppleARKit.AppleARKitTimecodeProvider"))
	{
	}

	/** Whether to publish face blend shapes to LiveLink or not */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	bool bEnableLiveLinkForFaceTracking;

	/** Whether to publish each frame or when the "FaceAR WriteCurveFile */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	bool bFaceTrackingWriteEachFrame;

	/** The type of face AR publisher that writes to disk to create */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	EARFaceTrackingFileWriterType FaceTrackingFileWriterType;

	/** Whether to publish the camera image each frame */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	bool bShouldWriteCameraImagePerFrame;

	/** The scale to write the images at. Used to reduce data footprint */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	float WrittenCameraImageScale;

	/** The quality setting to generate the jpeg images at. Defaults to 85, which is "high quality". Lower values reduce data footprint */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	int32 WrittenCameraImageQuality;

	/** Defaults to none. Use Right when in portrait mode */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	ETextureRotationDirection WrittenCameraImageRotation;

	/** The port to use when listening/sending LiveLink face blend shapes via the network */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	int32 LiveLinkPublishingPort;

	/** The default name to use when publishing face tracking name */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	FName DefaultFaceTrackingLiveLinkSubjectName;

	/** The default tracking to use when tracking face blend shapes (face relative or mirrored). Defaults to face relative */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	EARFaceTrackingDirection DefaultFaceTrackingDirection;

	/** Whether to adjust thread priorities during an AR session or not */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	bool bAdjustThreadPrioritiesDuringARSession;

	/** The game thread priority to change to when an AR session is running, default is 47 */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	int32 GameThreadPriorityOverride;
	
	/** The render thread priority to change to when an AR session is running, default is 45 */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	int32 RenderThreadPriorityOverride;

	/**
	 * Used to specify the timecode provider to use when identifying when an update occurred.
	 * Useful when using external timecode generators to sync multiple devices/machines
	 */
	UPROPERTY(Config, EditAnywhere, Category="AR Settings")
	FString ARKitTimecodeProvider;

	static UTimecodeProvider* GetTimecodeProvider();
};
