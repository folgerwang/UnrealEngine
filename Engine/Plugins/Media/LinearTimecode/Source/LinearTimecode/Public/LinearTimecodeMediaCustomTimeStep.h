// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineCustomTimeStep.h"
#include "ITimecodeProvider.h"

#include "LinearTimecodeDecoder.h"

#include "DropTimecode.h"
#include "MediaSampleQueue.h"

#include "LinearTimecodeMediaCustomTimeStep.generated.h"

class UMediaSource;
class UMediaPlayer;

/**
 * Class to control the Engine TimeStep from the audio of a MediaSource that have LTC encoded.
 * Not all MediaPlayer will behave properly. The MediaPlayer need to be tick properly and prefetch the values ahead of time.
 */
UCLASS(editinlinenew)
class LINEARTIMECODE_API ULinearTimecodeMediaCustomTimeStep : public UEngineCustomTimeStep
															, public ITimecodeProvider
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Detect the frame rate from the Audio Source.
	 * It may takes a full second before the frame rate is properly detected.
	 * Until the frame rate is properly detected, FrameRate will be used.
	 */
	UPROPERTY(EditAnywhere, Category = Time)
	bool bDetectFrameRate;

	/** Source's Frame Rate */
	UPROPERTY(EditAnywhere, Category=Time, meta=(EditCondition="!bDetectFrameRate"))
	FFrameRate FrameRate;

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;
	virtual bool UpdateTimeStep(class UEngine* InEngine) override;

	//~ ITimecodeProvider interface
	virtual FTimecode GetCurrentTimecode() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual bool IsSynchronizing() const override;
	virtual bool IsSynchronized() const override;

private:
	void GatherTimecodeSignals();
	bool WaitForSignal();

	void ResetDecodedTimecodes();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/** MediaSource from where the LTC signal can be decoded. */
	UPROPERTY(EditAnywhere, Category = "Media")
	UMediaSource* MediaSource;

	/*
	 * Extra time to add to the pulse signal when detected.
	 * It is used for synchronization with the Computer clock. It can't be negative.
	 * In seconds
	 */
	UPROPERTY(EditAnywhere, Category = "Media")
	double ExtraBufferingTime;

private:
	/** Media to read the LTC signal. */
	UPROPERTY(Transient)
	UMediaPlayer* MediaPlayer;

	/* LTC decoder */
	TUniquePtr<FLinearTimecodeDecoder> TimecodeDecoder;

	/** Current time code decoded by the TimecodeDecoder */
	FDropTimecode CurrentDecodingTimecode;

	/* Sample queue used by the MediaPlayer */
	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> SampleQueue;

	/* Decoded timecode for the LTC signal. */
	struct FDecodedSMPTETimecode
	{
		FDropTimecode SMPTETimecode;
		double ProcessSeconds;
	};
	TArray<FDecodedSMPTETimecode> DecodedTimecodes;

	/* Valid timecode value decoded from the LTC signal. */
	FDecodedSMPTETimecode CurrentDecodedTimecode;
	bool bIsCurrentDecodedTimecodeValid;

	/* Time of the platform clock when the media start being decoded. */
	double StartupTime;
	bool bDecodingStarted;
};
