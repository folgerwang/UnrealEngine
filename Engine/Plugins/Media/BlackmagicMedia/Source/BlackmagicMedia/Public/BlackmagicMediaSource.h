// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSynchronizableMediaSource.h"

#include "BlackmagicMediaFinder.h"
#include "Misc/FrameRate.h"

#include "BlackmagicMediaSource.generated.h"

/**
 * Available capture style for sources.
 */
UENUM(BlueprintType)
enum class EBlackmagicMediaCaptureStyle : uint8
{
	Video,
	AudioVideo,
};

/**
 * Available number of audio channel supported by UE4 & Capture card.
 */
UENUM(BlueprintType)
enum class EBlackmagicMediaAudioChannel : uint8
{
	Stereo2,
	Surround8,
};

/**
 * Media source description for Blackmagic.
 */
UCLASS(BlueprintType)
class BLACKMAGICMEDIA_API UBlackmagicMediaSource : public UBaseMediaSource
{
	GENERATED_BODY()

public:
	/**
	 * The input name of the source to be played".
	 * This combines the device ID, and the input.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Blackmagic, AssetRegistrySearchable)
	FBlackmagicMediaPort MediaPort;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Blackmagic, AssetRegistrySearchable)
	FBlackmagicMediaModeInput MediaMode;

	/** Whether to use the time code embedded in the input stream when time code locking is enabled in the Engine. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Synchronization)
	bool UseTimecode;

	/** Video or Video+Audio */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=VideoFormat)
	EBlackmagicMediaCaptureStyle CaptureStyle;

	/** Desired number of audio channel to capture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Audio)
	EBlackmagicMediaAudioChannel AudioChannels;

public:
	/**
	 * Encode Timecode in the output
	 * Current value will be white. The format will be encoded in hh:mm::ss::ff. Each value, will be on a different line.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug", meta = (EditCondition = "UseTimecode"))
	bool bEncodeTimecodeInTexel;

	/** Use the low level buffering, and polling read. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	bool UseStreamBuffer;

	/** number of frames to buffer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = Debug, meta = (ClampMin = "2", ClampMax = "16"))
	int32 NumberFrameBuffers;

public:
	/** Default constructor. */
	UBlackmagicMediaSource();

public:
	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

public:
	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;
};
