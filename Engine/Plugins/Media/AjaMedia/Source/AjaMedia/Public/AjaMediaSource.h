// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSynchronizableMediaSource.h"

#include "AjaMediaFinder.h"
#include "Misc/FrameRate.h"

#include "AjaMediaSource.generated.h"

/**
 * Available input color formats for Aja sources.
 */
UENUM()
enum class EAjaMediaSourceColorFormat : uint8
{
	BGRA UMETA(DisplayName = "8bit RGBA"),
	BGR10 UMETA(DisplayName = "10bit RGB"),
};

/**
 * Available number of audio channel supported by UE4 & Aja
 */
UENUM()
enum class EAjaMediaAudioChannel : uint8
{
	Channel6,
	Channel8,
};

/**
 * Media source for Aja streams.
 */
UCLASS(BlueprintType, hideCategories=(Platforms,Object))
class AJAMEDIA_API UAjaMediaSource : public UTimeSynchronizableMediaSource
{
	GENERATED_BODY()

	/** Default constructor. */
	UAjaMediaSource();

public:
	/**
	 * The input name of the AJA source to be played".
	 * This combines the device ID, and the input.
	 */
	UPROPERTY()
	FAjaMediaPort MediaPort;

	/** Override project setting's media mode. */
	UPROPERTY()
	bool bIsDefaultModeOverriden;

	/** The expected input signal format from the MediaPort. Uses project settings by default. */
	UPROPERTY()
	FAjaMediaMode MediaMode;

	/** Use the time code embedded in the input stream. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA")
	EAjaMediaTimecodeFormat TimecodeFormat;

	/**
	 * Use a ring buffer to capture and transfer data.
	 * This may decrease transfer latency but increase stability.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="AJA")
	bool bCaptureWithAutoCirculating;

public:
	/**
	 * Capture Ancillary from the AJA source.
	 * It will decrease performance
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Ancillary")
	bool bCaptureAncillary;

	/** Maximum number of ancillary data frames to buffer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category="Ancillary", meta=(EditCondition="bCaptureAncillary", ClampMin="1", ClampMax="32"))
	int32 MaxNumAncillaryFrameBuffer;

public:
	/**
	 * Capture Audio from the AJA source.
	 * It will decrease performance
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Audio")
	bool bCaptureAudio;

	/** Desired number of audio channel to capture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Audio", meta=(EditCondition="bCaptureAudio"))
	EAjaMediaAudioChannel AudioChannel;

	/** Maximum number of audio frames to buffer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category="Audio", meta=(EditCondition="bCaptureAudio", ClampMin="1", ClampMax="32"))
	int32 MaxNumAudioFrameBuffer;

public:
	/**
	 * Capture Video from the AJA source.
	 * It will decrease performance
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Video")
	bool bCaptureVideo;

	/** Desired color format of input video frames (default = BGRA). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Video", meta=(EditCondition="bCaptureVideo"))
	EAjaMediaSourceColorFormat ColorFormat;

	/** Maximum number of video frames to buffer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category="Video", meta=(EditCondition="bCaptureVideo", ClampMin="1", ClampMax="32"))
	int32 MaxNumVideoFrameBuffer;

public:
	/** Log a warning when there's a drop frame. */
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bLogDropFrame;

	/**
	 * Encode Timecode in the output
	 * Current value will be white. The format will be encoded in hh:mm::ss::ff. Each value, will be on a different line.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Debug", meta=(EditCondition="IN_CPP"))
	bool bEncodeTimecodeInTexel;

public:
	FAjaMediaMode GetMediaMode() const;
	FAjaMediaConfiguration GetMediaConfiguration() const;

public:
	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

public:
	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

public:
	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif //WITH_EDITOR
	//~ End UObject interface
};
