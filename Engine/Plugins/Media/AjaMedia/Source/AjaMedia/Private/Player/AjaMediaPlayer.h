// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCorePlayerBase.h"

#include "AjaMediaFinder.h"
#include "AjaMediaPrivate.h"
#include "AjaMediaSource.h"

class FAjaMediaAudioSamplePool;
class FAjaMediaBinarySamplePool;
class FAjaMediaTextureSamplePool;
class IMediaEventSink;

enum class EMediaTextureSampleFormat;

namespace AJA
{
	class AJAInputChannel;
}

/**
 * Implements a media player using Aja.
 *
 * The processing of metadata and video frames is delayed until the fetch stage
 * (TickFetch) in order to increase the window of opportunity for receiving Aja
 * frames for the current render frame time code.
 *
 * Depending on whether the media source enables time code synchronization,
 * the player's current play time (CurrentTime) is derived either from the
 * time codes embedded in Aja frames or from the Engine's global time code.
 */
class FAjaMediaPlayer
	: public FMediaIOCorePlayerBase
	, protected AJA::IAJAInputOutputChannelCallbackInterface
{
	using Super = FMediaIOCorePlayerBase;
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 */
	FAjaMediaPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FAjaMediaPlayer();

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual FName GetPlayerName() const override;
	virtual FString GetUrl() const override;

	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;

	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

	virtual FString GetStats() const override;

protected:

	//~ IAJAInputOutputCallbackInterface interface
	
	virtual void OnInitializationCompleted(bool bSucceed) override;
	virtual bool OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame) override;
	virtual bool OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData) override;
	virtual void OnCompletion(bool bSucceed) override;

protected:

	/**
	 * Process pending audio and video frames, and forward them to the sinks.
	 */
	void ProcessFrame();
	
protected:

	/** Verify if we lost some frames since last Tick*/
	void VerifyFrameDropCount();


	virtual bool IsHardwareReady() const override;

private:

	/** Audio, MetaData, Texture  sample object pool. */
	FAjaMediaAudioSamplePool* AudioSamplePool;
	FAjaMediaBinarySamplePool* MetadataSamplePool;
	FAjaMediaTextureSamplePool* TextureSamplePool;

	/** The media sample cache. */
	int32 MaxNumAudioFrameBuffer;
	int32 MaxNumMetadataFrameBuffer;
	int32 MaxNumVideoFrameBuffer;

	/** Current state of the media player. */
	EMediaState AjaThreadNewState;

	/** Current playback time. */
	FTimespan AjaThreadCurrentTime;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Number of audio channels in the last received sample. */
	int32 AjaThreadAudioChannels;

	/** Audio sample rate in the last received sample. */
	int32 AjaThreadAudioSampleRate;

	/** Video dimensions in the last received sample. */
	FIntPoint AjaLastVideoDim;

	/** Video frame rate in the last received sample. */
	FFrameRate VideoFrameRate;

	/** Number of frames drop from the last tick. */
	int32 AjaThreadFrameDropCount;
	int32 AjaThreadAutoCirculateAudioFrameDropCount;
	int32 AjaThreadAutoCirculateMetadataFrameDropCount;
	int32 AjaThreadAutoCirculateVideoFrameDropCount;

	/** Whether to use the time code embedded in Aja frames. */
	bool bEncodeTimecodeInTexel;

	/** Which field need to be capture. */
	bool bUseAncillary;
	bool bUseAncillaryField2;
	bool bUseAudio;
	bool bUseVideo;
	bool bVerifyFrameDropCount;

	/** The current video sample format. */
	EMediaTextureSampleFormat VideoSampleFormat;
	
	/** The currently opened URL. */
	FAjaMediaPort DeviceSource;

	/** Maps to the current input Device */
	AJA::AJAInputChannel* InputChannel;

	/** Frame Description from capture device */
	AJA::FAJAVideoFormat LastVideoFormatIndex;

	/** Previous frame timecode to calculate a timespan */
	AJA::FTimecode AjaThreadPreviousFrameTimecode;
	FTimespan AjaThreadPreviousFrameTimespan;
};
