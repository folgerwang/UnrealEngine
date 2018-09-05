// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCorePlayerBase.h"

#include "BlackmagicMediaFinder.h"
#include "BlackmagicMediaPrivate.h"
#include "BlackmagicMediaSource.h"

class FBlackmagicMediaAudioSamplePool;
class FBlackmagicMediaTextureSamplePool;
class IMediaEventSink;

enum class EMediaTextureSampleFormat;

/**
* Implements a media player for Blackmagic.
*
* The processing of metadata and video frames is delayed until the fetch stage
* (TickFetch) in order to increase the window of opportunity for receiving
* frames for the current render frame time code.
*
* Depending on whether the media source enables time code synchronization,
* the player's current play time (CurrentTime) is derived either from the
* time codes embedded in frames or from the Engine's global time code.
*/
class FBlackmagicMediaPlayer : public FMediaIOCorePlayerBase
{
	using Super = FMediaIOCorePlayerBase;
public:

	/**
	* Create and initialize a new instance.
	*
	* @param InEventSink The object that receives media events from this player.
	*/
	FBlackmagicMediaPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FBlackmagicMediaPlayer();

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual FName GetPlayerName() const override;
	virtual FString GetUrl() const override;

	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;

	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;

	/** Process pending audio and video frames, and forward them to the sinks. */
	void ProcessFrame();

	/** Verify if we lost some frames since last Tick. */
	void VerifyFrameDropCount();

	/** Is Hardware initialized */
	virtual bool IsHardwareReady() const override;

protected:
	bool OnFrameArrived(BlackmagicDevice::FFrame InFrame);
	bool DeliverFrame(BlackmagicDevice::FFrame InFrame);

private:

	/** Encode the time into video frame */
	bool bEncodeTimecodeInTexel;

	/** Whether to use the timecode embedded in a frame. */
	bool bUseFrameTimecode;

	/** Open has finished */
	bool bIsOpen;

	/** Audio sample object pool. */
	FBlackmagicMediaAudioSamplePool* AudioSamplePool;

	/** The currently opened URL. */
	FBlackmagicMediaPort DeviceSource;

	/** Which feature do we captured. Audio/Video */
	EBlackmagicMediaCaptureStyle CaptureStyle;

	/** Current Frame Description */
	BlackmagicDevice::FFrameDesc LastFrameDesc;
	BlackmagicDevice::FFrameDesc FrameDesc;
	int32 BmThread_AudioSampleRate;
	int32 BmThread_AudioChannels;

	/** Current Frame Description Info */
	BlackmagicDevice::FFrameInfo FrameInfo;

	/** Currently active capture Device */
	BlackmagicDevice::FDevice Device;

	/** Maps to the current input Device */
	BlackmagicDevice::FPort Port;

	/** Previous frame timecode */
	BlackmagicDevice::FTimecode PreviousFrameTimecode;

	//* get notifications for frames arriving */
	struct FCallbackHandler;
	friend FCallbackHandler;
	FCallbackHandler* CallbackHandler;

};
