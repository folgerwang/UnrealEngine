// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IMediaSamples.h"
#include "Templates/SharedPointer.h"

#include "MediaSampleQueue.h"

class IMediaAudioSample;
class IMediaBinarySample;
class IMediaOverlaySample;
class IMediaTextureSample;

/**
 * General purpose media sample queue.
 */
class MEDIAIOCORE_API FMediaIOCoreSamples
	: public IMediaSamples
{
public:
	FMediaIOCoreSamples() = default;
	FMediaIOCoreSamples(const FMediaIOCoreSamples&) = delete;
	FMediaIOCoreSamples& operator=(const FMediaIOCoreSamples&) = delete;

public:

	/**
	 * Add the given audio sample to the cache.
	 *
	 * @param Sample The sample to add.
	 * @return True if the operation succeeds.
	 * @see AddCaption, AddMetadata, AddSubtitle, AddVideo, PopAudio, NumAudioSamples
	 */
	bool AddAudio(const TSharedRef<IMediaAudioSample, ESPMode::ThreadSafe>& Sample)
	{
		return AudioSampleQueue.Enqueue(Sample);
	}

	/**
	 * Add the given caption sample to the cache.
	 *
	 * @param Sample The sample to add.
	 * @return True if the operation succeeds.
	 * @see AddAudio, AddMetadata, AddSubtitle, AddVideo, PopCaption, NumCaptionSamples
	 */
	bool AddCaption(const TSharedRef<IMediaOverlaySample, ESPMode::ThreadSafe>& Sample)
	{
		return CaptionSampleQueue.Enqueue(Sample);
	}

	/**
	 * Add the given metadata sample to the cache.
	 *
	 * @param Sample The sample to add.
	 * @return True if the operation succeeds.
	 * @see AddAudio, AddCaption, AddSubtitle, AddVideo, PopMetadata, NumMetadataSamples
	 */
	bool AddMetadata(const TSharedRef<IMediaBinarySample, ESPMode::ThreadSafe>& Sample)
	{
		return MetadataSampleQueue.Enqueue(Sample);
	}

	/**
	 * Add the given subtitle sample to the cache.
	 *
	 * @param Sample The sample to add.
	 * @return True if the operation succeeds.
	 * @see AddAudio, AddCaption, AddMetadata, AddVideo, PopSubtitle, NumSubtitleSamples
	 */
	bool AddSubtitle(const TSharedRef<IMediaOverlaySample, ESPMode::ThreadSafe>& Sample)
	{
		return SubtitleSampleQueue.Enqueue(Sample);
	}

	/**
	 * Add the given video sample to the cache.
	 *
	 * @param Sample The sample to add.
	 * @return True if the operation succeeds.
	 * @see AddAudio, AddCaption, AddMetadata, AddSubtitle, PopVideo, NumVideoSamples
	 */
	bool AddVideo(const TSharedRef<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		return VideoSampleQueue.Enqueue(Sample);
	}

	/**
	 * Pop a Audio sample from the cache.
	 *
	 * @return True if the operation succeeds.
	 * @see AddAudio, NumAudioSamples
	 */
	bool PopAudio()
	{
		return AudioSampleQueue.Pop();
	}

	/**
	 * Pop a Caption sample from the cache.
	 *
	 * @return True if the operation succeeds.
	 * @see AddCaption, NumCaption
	 */
	bool PopCaption()
	{
		return CaptionSampleQueue.Pop();
	}

	/**
	 * Pop a Metadata sample from the cache.
	 *
	 * @return True if the operation succeeds.
	 * @see AddMetadata, NumMetadata
	 */
	bool PopMetadata()
	{
		return MetadataSampleQueue.Pop();
	}

	/**
	 * Pop a Subtitle sample from the cache.
	 *
	 * @return True if the operation succeeds.
	 * @see AddSubtitle, NumSubtitle
	 */
	bool PopSubtitle()
	{
		return SubtitleSampleQueue.Pop();
	}

	/**
	 * Pop a video sample from the cache.
	 *
	 * @return True if the operation succeeds.
	 * @see AddVideo, NumVideo
	 */
	bool PopVideo()
	{
		return VideoSampleQueue.Pop();
	}

	/**
	 * Get the number of queued audio samples.
	 *
	 * @return Number of samples.
	 * @see AddAudio, PopAudio
	 */
	int32 NumAudioSamples() const
	{
		return AudioSampleQueue.Num();
	}

	/**
	 * Get the number of queued caption samples.
	 *
	 * @return Number of samples.
	 * @see AddCaption, PopCaption
	 */
	int32 NumCaptionSamples() const
	{
		return CaptionSampleQueue.Num();
	}

	/**
	 * Get the number of queued metadata samples.
	 *
	 * @return Number of samples.
	 * @see AddMetadata, PopMetada
	 */
	int32 NumMetadataSamples() const
	{
		return MetadataSampleQueue.Num();
	}

	/**
	 * Get the number of queued subtitle samples.
	 *
	 * @return Number of samples.
	 * @see AddSubtitle, PopSubtitle
	 */
	int32 NumSubtitleSamples() const
	{
		return SubtitleSampleQueue.Num();
	}

	/**
	 * Get the number of queued video samples.
	 *
	 * @return Number of samples.
	 * @see AddVideo, PopVideo
	 */
	int32 NumVideoSamples() const
	{
		return VideoSampleQueue.Num();
	}

	/**
	 * Get next sample time from the VideoSampleQueue.
	 *
	 * @return Time of the next sample from the VideoSampleQueue
	 * @see AddVideo, NumVideoSamples
	 */
	FTimespan GetNextVideoSampleTime()
	{
		FTimespan NextTime;
		TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;
		const bool bHasSucceed = VideoSampleQueue.Peek(Sample);
		if (bHasSucceed)
		{
			NextTime = Sample->GetTime();
		}
		return NextTime;
	}

public:

	//~ IMediaSamples interface

	virtual bool FetchAudio(TRange<FTimespan> TimeRange, TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool FetchCaption(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool FetchMetadata(TRange<FTimespan> TimeRange, TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool FetchSubtitle(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual void FlushSamples() override;

protected:

	/** Audio sample queue. */
	FMediaAudioSampleQueue AudioSampleQueue;

	/** Caption sample queue. */
	FMediaOverlaySampleQueue CaptionSampleQueue;

	/** Metadata sample queue. */
	FMediaBinarySampleQueue MetadataSampleQueue;

	/** Subtitle sample queue. */
	FMediaOverlaySampleQueue SubtitleSampleQueue;

	/** Video sample queue. */
	FMediaTextureSampleQueue VideoSampleQueue;
};
