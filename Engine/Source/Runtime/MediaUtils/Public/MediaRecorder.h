// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MediaSampleQueue.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

class FMediaPlayerFacade;
class FMediaRecorderClockSink;


/**
 * Records samples from a media player.
 *
 * Currently only records texture samples.
 */
class FMediaRecorder
{
public:

	/** Default constructor. */
	FMediaRecorder();

public:

	/**
	 * Start recording samples from a given media player.
	 *
	 * @param PlayerFacade The media player facade to record from.
	 * @see StopRecording
	 */
	void StartRecording(const TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe>& PlayerFacade);

	/**
	 * Stop recording media samples.
	 *
	 * @see StartRecording
	 */
	void StopRecording();

protected:

	/**
	 * Tick the recorder.
	 *
	 * @param Timecode The current timecode.
	 */
	void TickRecording(FTimespan Timecode);

private:

	friend class FMediaRecorderClockSink;

	/** The recorder's media clock sink. */
	TSharedPtr<FMediaRecorderClockSink, ESPMode::ThreadSafe> ClockSink;

	/** Number of frames recorded. */
	int32 FrameCount;

	/** Whether recording is in progress. */
	bool Recording;

	/** Texture sample queue. */
	TSharedPtr<FMediaTextureSampleQueue, ESPMode::ThreadSafe> SampleQueue;
};
