// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTimeController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"

void FMovieSceneTimeController::Tick(float DeltaSeconds, float InPlayRate)
{
	OnTick(DeltaSeconds, InPlayRate);
}

void FMovieSceneTimeController::Reset(const FQualifiedFrameTime& InStartTime)
{
	if (PlaybackStartTime.IsSet())
	{
		StopPlaying(InStartTime);
		StartPlaying(InStartTime);
	}
}

void FMovieSceneTimeController::PlayerStatusChanged(EMovieScenePlayerStatus::Type InStatus, const FQualifiedFrameTime& InCurrentTime)
{
	if (PlaybackStartTime.IsSet() && InStatus != EMovieScenePlayerStatus::Playing)
	{
		StopPlaying(InCurrentTime);
	}

	if (!PlaybackStartTime.IsSet() && InStatus == EMovieScenePlayerStatus::Playing)
	{
		StartPlaying(InCurrentTime);
	}
}

void FMovieSceneTimeController::StartPlaying(const FQualifiedFrameTime& InStartTime)
{
	UE_LOG(LogMovieScene, VeryVerbose, TEXT("TimeController Start: Sequence started: frame %d, subframe %f. Frame rate: %f fps."), InStartTime.Time.FrameNumber.Value, InStartTime.Time.GetSubFrame(), InStartTime.Rate.AsDecimal());

	PlaybackStartTime = InStartTime;
	OnStartPlaying(InStartTime);
}

void FMovieSceneTimeController::StopPlaying(const FQualifiedFrameTime& InStopTime)
{
	UE_LOG(LogMovieScene, VeryVerbose, TEXT("TimeController Start: Sequence stopped."));

	OnStopPlaying(InStopTime);
	PlaybackStartTime.Reset();
}

FFrameTime FMovieSceneTimeController::RequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate)
{
	return OnRequestCurrentTime(InCurrentTime, InPlayRate);
}

void FMovieSceneTimeController_ExternalClock::OnStartPlaying(const FQualifiedFrameTime& InStartTime)
{
	ClockStartTime = ClockLastUpdateTime = GetCurrentTime();
}

void FMovieSceneTimeController_ExternalClock::OnStopPlaying(const FQualifiedFrameTime& InStopTime)
{
	ClockLastUpdateTime.Reset();
	ClockStartTime.Reset();
	AccumulatedDilation = 0.0;
}

FFrameTime FMovieSceneTimeController_ExternalClock::OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate)
{
	TOptional<FQualifiedFrameTime> StartTimeIfPlaying = GetPlaybackStartTime();
	if (!StartTimeIfPlaying.IsSet())
	{
		return InCurrentTime.Time;
	}

	double StartedTime = ClockStartTime.GetValue();
	double CurrentTime = GetCurrentTime();

	double UndilatedDeltaTime = (CurrentTime - StartedTime);

	AccumulatedDilation += (InPlayRate - 1.0) * (CurrentTime - ClockLastUpdateTime.GetValue());

	ClockLastUpdateTime = CurrentTime;

	double CurrentSequenceTimeSeconds = UndilatedDeltaTime + AccumulatedDilation;

	FFrameTime StartTime = StartTimeIfPlaying->ConvertTo(InCurrentTime.Rate);
	FFrameTime NewTime   = StartTime + CurrentSequenceTimeSeconds * InCurrentTime.Rate;


	UE_LOG(LogMovieScene, VeryVerbose, TEXT("TimeController Clock tick: Clock Start Time: %f, Clock Now: %f, Dilation Offset: %f, Sequence Start Time: frame %d, subframe %f, Sequence Offset Seconds: %f, Sequence Now: frame %d, subframe %f"),
		StartedTime, CurrentTime, AccumulatedDilation, StartTime.FrameNumber.Value, StartTime.GetSubFrame(), CurrentSequenceTimeSeconds, NewTime.FrameNumber.Value, NewTime.GetSubFrame());


	return NewTime;
}

double FMovieSceneTimeController_PlatformClock::GetCurrentTime() const
{
	return FPlatformTime::Seconds();
}

double FMovieSceneTimeController_AudioClock::GetCurrentTime() const
{
	FAudioDevice* AudioDevice = GEngine ? GEngine->GetMainAudioDevice() : nullptr;
	return AudioDevice ? AudioDevice->GetAudioClock() : 0.0;
}

void FMovieSceneTimeController_Tick::OnStartPlaying(const FQualifiedFrameTime& InStartTime)
{
	CurrentOffsetSeconds = 0.0;
}

void FMovieSceneTimeController_Tick::OnTick(float DeltaSeconds, float InPlayRate)
{
	CurrentOffsetSeconds += DeltaSeconds * InPlayRate;
}

FFrameTime FMovieSceneTimeController_Tick::OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate)
{
	TOptional<FQualifiedFrameTime> StartTimeIfPlaying = GetPlaybackStartTime();
	if (!StartTimeIfPlaying.IsSet())
	{
		return InCurrentTime.Time;
	}
	else
	{
		FFrameTime StartTime = StartTimeIfPlaying->ConvertTo(InCurrentTime.Rate);
		return StartTime + CurrentOffsetSeconds * InCurrentTime.Rate;
	}
}