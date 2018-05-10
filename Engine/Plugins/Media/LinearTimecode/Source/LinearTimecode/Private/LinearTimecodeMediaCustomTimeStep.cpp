// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LinearTimecodeMediaCustomTimeStep.h"
#include "LinearTimecodePlugin.h"
#include "LinearTimecodeDecoder.h"

#include "IMediaClock.h"
#include "IMediaModule.h"
#include "IMediaTicker.h"
#include "MediaAudioSampleReader.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "Modules/ModuleManager.h"
#include "Stats/StatsMisc.h"


/* ULinearTimecodeMediaCustomTimeStep
*****************************************************************************/
ULinearTimecodeMediaCustomTimeStep::ULinearTimecodeMediaCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ExtraBufferingTime(0.100)
	, bIsCurrentDecodedTimecodeValid(false)
	, bDecodingStarted(false)
{
}

/* UFixedFrameRateCustomTimeStep interface implementation
*****************************************************************************/
bool ULinearTimecodeMediaCustomTimeStep::Initialize(class UEngine* InEngine)
{
	if (MediaSource == nullptr)
	{
		UE_LOG(LogLinearTimecode, Warning, TEXT("The MediaSource of '%s' is not valid."), *GetName());
		return false;
	}

	MediaPlayer = NewObject<UMediaPlayer>(this, NAME_None, RF_Transient);
	
	if (!MediaPlayer->CanPlaySource(MediaSource))
	{
		UE_LOG(LogLinearTimecode, Warning, TEXT("The MediaSource of '%s' can't be played."), *GetName());
		MediaPlayer = nullptr;
		return false;
	}

	if (!MediaPlayer->OpenSource(MediaSource))
	{
		UE_LOG(LogLinearTimecode, Warning, TEXT("The MediaSource of '%s' can't be opened."), *GetName());
		MediaPlayer = nullptr;
		return false;
	}

	if (ExtraBufferingTime < SMALL_NUMBER)
	{
		ExtraBufferingTime = GetDefault<ULinearTimecodeMediaCustomTimeStep>()->ExtraBufferingTime;
		UE_LOG(LogLinearTimecode, Warning, TEXT("ExtraBufferingTime can't be null or negative. Revert to the default value."));
	}

	SampleQueue = MakeShared<FMediaAudioSampleQueue, ESPMode::ThreadSafe>();
	MediaPlayer->GetPlayerFacade()->AddAudioSampleSink(SampleQueue.ToSharedRef());

	CurrentDecodingTimecode = FDropTimecode();
	TimecodeDecoder.Reset(new FLinearTimecodeDecoder());
	ResetDecodedTimecodes();

	return true;
}

void ULinearTimecodeMediaCustomTimeStep::Shutdown(class UEngine* InEngine)
{
	if (MediaPlayer)
	{
		MediaPlayer->Close();
		MediaPlayer = nullptr;
	}

	TimecodeDecoder.Reset();
}

bool ULinearTimecodeMediaCustomTimeStep::UpdateTimeStep(class UEngine* InEngine)
{
	bool bRunEngineTimeStep = true;
	if (MediaPlayer && MediaPlayer->IsPlaying())
	{
		GatherTimecodeSignals();
		if (bDecodingStarted)
		{
			bRunEngineTimeStep = WaitForSignal();
		}
	}
	else
	{
		if (bDecodingStarted)
		{
			bDecodingStarted = false;
			CurrentDecodingTimecode = FDropTimecode();
			UE_LOG(LogLinearTimecode, Error, TEXT("The MediaPlayer stopped. The Engine is not in sync with the signal anymore."));
		}
	}
	return true;
}

/* ITimecodeProvider interface implementation
*****************************************************************************/
FTimecode ULinearTimecodeMediaCustomTimeStep::GetCurrentTimecode() const
{
	return IsSynchronized() ? CurrentDecodedTimecode.SMPTETimecode.Timecode : FTimecode();
}

FFrameRate ULinearTimecodeMediaCustomTimeStep::GetFrameRate() const
{
	return bDetectFrameRate && IsSynchronized() ? FFrameRate(CurrentDecodedTimecode.SMPTETimecode.FrameRate, 1) : FrameRate;
}

bool ULinearTimecodeMediaCustomTimeStep::IsSynchronizing() const
{
	return MediaPlayer && (MediaPlayer->IsPlaying() || MediaPlayer->IsConnecting() || MediaPlayer->IsBuffering() || MediaPlayer->IsPreparing());
}

bool ULinearTimecodeMediaCustomTimeStep::IsSynchronized() const
{
	return MediaPlayer && MediaPlayer->IsPlaying() && bDecodingStarted && bIsCurrentDecodedTimecodeValid;
}

/* ULinearTimecodeMediaCustomTimeStep implementation
*****************************************************************************/
void ULinearTimecodeMediaCustomTimeStep::GatherTimecodeSignals()
{
	check(MediaPlayer);

	TSharedPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacadeShared = MediaPlayer->GetPlayerFacade();
	if (PlayerFacadeShared.IsValid())
	{
		TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> AudioSample;
		while (SampleQueue->Dequeue(AudioSample))
		{
			check(AudioSample.IsValid());
			MediaAudioSampleReader SampleReader(AudioSample);
			float Sample;
			while (SampleReader.GetSample(Sample))
			{
				if (TimecodeDecoder->Sample(Sample, CurrentDecodingTimecode))
				{
					FDecodedSMPTETimecode DecodedTimecode;
					DecodedTimecode.SMPTETimecode = CurrentDecodingTimecode;
					CurrentDecodingTimecode = FDropTimecode();

					if (!bDecodingStarted)
					{
						bDecodingStarted = true;
						StartupTime = FPlatformTime::Seconds();
					}

					int32 Difference = SampleReader.GetCurrentSampleStream() - SampleReader.GetSampleStep() - reinterpret_cast<const uint8*>(AudioSample->GetBuffer());
					check(Difference >= 0);
					int32 NumberOfFrames = Difference / SampleReader.GetSampleStep();

					DecodedTimecode.ProcessSeconds = AudioSample->GetTime().GetTotalSeconds();
					DecodedTimecode.ProcessSeconds += (double)NumberOfFrames / (double)AudioSample->GetSampleRate();
					DecodedTimecode.ProcessSeconds += StartupTime;
					DecodedTimecode.ProcessSeconds += ExtraBufferingTime;

					// detect looping
					if (DecodedTimecodes.Num() > 0)
					{
						const FTimecode& Last = DecodedTimecodes.Last().SMPTETimecode.Timecode;
						const FTimecode& New = DecodedTimecode.SMPTETimecode.Timecode;
						if ((Last.Hours > New.Hours)
							|| (Last.Hours == New.Hours && Last.Minutes > New.Minutes)
							|| (Last.Hours == New.Hours && Last.Minutes == New.Minutes && Last.Seconds > New.Seconds)
							|| (Last.Hours == New.Hours && Last.Minutes == New.Minutes && Last.Seconds == New.Seconds && Last.Frames > New.Frames)
							)
						{
							UE_LOG(LogLinearTimecode, Warning, TEXT("The newly decoded timecode is smaller than the new timecode. (Is the generator got reset?)"));
							ResetDecodedTimecodes();
						}
					}
					DecodedTimecodes.Add(DecodedTimecode);
				}
			}
		}
	}

	if (bDecodingStarted && DecodedTimecodes.Num() == 0)
	{
		UE_LOG(LogLinearTimecode, Warning, TEXT("The signal doesn't come fast enough in the engine."));
	}
}

void ULinearTimecodeMediaCustomTimeStep::ResetDecodedTimecodes()
{
	DecodedTimecodes.Reset();
	bIsCurrentDecodedTimecodeValid = false;
}

bool ULinearTimecodeMediaCustomTimeStep::WaitForSignal()
{
	UpdateApplicationLastTime();

	const double CurrentTime = FPlatformTime::Seconds();

	// Find a valid time
	int32 FoundIndex = INDEX_NONE;
	if (DecodedTimecodes.Num() > 1)
	{
		for (int32 Index = 1; Index < DecodedTimecodes.Num(); ++Index)
		{
			const double TargetSeconds = DecodedTimecodes[Index].ProcessSeconds;
			const double PreviousTargetSeconds = DecodedTimecodes[Index - 1].ProcessSeconds;
			if (CurrentTime <= TargetSeconds && CurrentTime > PreviousTargetSeconds)
			{
				FoundIndex = Index;
			}
		}
		if (FoundIndex == INDEX_NONE)
		{
			for (int32 Index = 0; Index < DecodedTimecodes.Num(); ++Index)
			{
				const double TargetSeconds = DecodedTimecodes[Index].ProcessSeconds;
				if (CurrentTime <= TargetSeconds)
				{
					FoundIndex = Index;
					break;
				}
			}
		}
	}
	else if (DecodedTimecodes.Num() == 1)
	{
		if (CurrentTime <= DecodedTimecodes[0].ProcessSeconds)
		{
			FoundIndex = 0;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		// We didn't found anything. Remove. Warn the user and wait with WaitForFixedFrameRate
		if (bIsCurrentDecodedTimecodeValid)
		{
			ResetDecodedTimecodes();
		}

		// Warn the user that no timecode was found. Already warn if no new timecode got capture in the ULinearTimecodeMediaCustomTimeStep::GatherTimecodeSignals
		if (DecodedTimecodes.Num() > 0)
		{
			UE_LOG(LogLinearTimecode, Warning, TEXT("No signal was found with a timecode in sync with the Engine clock. (Do you have a low FPS?)"));
		}

		return true;
	}
	else
	{
		FDecodedSMPTETimecode NewDecodeTimecode = DecodedTimecodes[FoundIndex];

		// Remove old stuff and new NewDecodeTimecode
		if (FoundIndex > 0)
		{
			UE_LOG(LogLinearTimecode, Warning, TEXT("%d LTC signal(s) got skipped. (Do you have a low FPS?)"), FoundIndex);
		}
		DecodedTimecodes.RemoveAt(0, FoundIndex + 1, false);

		{
			double ActualWaitTime = 0.0;
			{
				FSimpleScopeSecondsCounter ActualWaitTimeCounter(ActualWaitTime);

				// Use fixed delta time and update time.
				if (bIsCurrentDecodedTimecodeValid && bDetectFrameRate)
				{
					FApp::SetDeltaTime((NewDecodeTimecode.ProcessSeconds - CurrentDecodedTimecode.ProcessSeconds));
				}
				else
				{
					FApp::SetDeltaTime(FrameRate.AsInterval());
				}

				const double StartWaitTime = FPlatformTime::Seconds();
				const double WaitTime = FMath::Max(NewDecodeTimecode.ProcessSeconds - StartWaitTime, 0.0);

				if (WaitTime > 5.f / 1000.f)
				{
					FPlatformProcess::SleepNoStats(WaitTime - 0.002f);
				}

				// Give up timeslice for remainder of wait time.
				while (FPlatformTime::Seconds() < NewDecodeTimecode.ProcessSeconds)
				{
					FPlatformProcess::SleepNoStats(0.f);
				}
			}

			FApp::SetIdleTime(ActualWaitTime);
			FApp::SetCurrentTime(FPlatformTime::Seconds());
		}

		CurrentDecodedTimecode = NewDecodeTimecode;
		bIsCurrentDecodedTimecodeValid = true;
	}

	return false;
}

#if WITH_EDITOR
void ULinearTimecodeMediaCustomTimeStep::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (ExtraBufferingTime < SMALL_NUMBER)
	{
		ExtraBufferingTime = GetDefault<ULinearTimecodeMediaCustomTimeStep>()->ExtraBufferingTime;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
