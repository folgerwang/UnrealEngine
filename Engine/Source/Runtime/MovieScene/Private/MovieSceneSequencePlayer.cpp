// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"
#include "MovieSceneSequence.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/RuntimeErrors.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetDriver.h"

DEFINE_LOG_CATEGORY_STATIC(LogMovieSceneRepl, Warning, All);

bool FMovieSceneSequenceLoopCount::SerializeFromMismatchedTag( const FPropertyTag& Tag, FStructuredArchive::FSlot Slot )
{
	if (Tag.Type == NAME_IntProperty)
	{
		Slot << Value;
		return true;
	}

	return false;
}

bool FMovieSceneSequencePlaybackSettings::SerializeFromMismatchedTag( const FPropertyTag& Tag, FStructuredArchive::FSlot Slot )
{
	if (Tag.Type == NAME_StructProperty && Tag.StructName == "LevelSequencePlaybackSettings")
	{
		StaticStruct()->SerializeItem(Slot, this, nullptr);
		return true;
	}

	return false;
}

UMovieSceneSequencePlayer::UMovieSceneSequencePlayer(const FObjectInitializer& Init)
	: Super(Init)
	, Status(EMovieScenePlayerStatus::Stopped)
	, bReversePlayback(false)
	, bIsEvaluating(false)
	, Sequence(nullptr)
	, StartTime(0)
	, DurationFrames(0)
	, CurrentNumLoops(0)
{
	PlayPosition.Reset(FFrameTime(0));

	NetSyncProps.LastKnownPosition = FFrameTime(0);
	NetSyncProps.LastKnownStatus   = Status;
}

UMovieSceneSequencePlayer::~UMovieSceneSequencePlayer()
{
	if (GEngine && OldMaxTickRate.IsSet())
	{
		GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
	}
}

void UMovieSceneSequencePlayer::UpdateNetworkSyncProperties()
{
	if (HasAuthority())
	{
		NetSyncProps.LastKnownPosition = PlayPosition.GetCurrentPosition();
		NetSyncProps.LastKnownStatus   = Status;
		NetSyncProps.LastKnownNumLoops = CurrentNumLoops;
	}
}

void UMovieSceneSequencePlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMovieSceneSequencePlayer, NetSyncProps);
	DOREPLIFETIME(UMovieSceneSequencePlayer, bReversePlayback);
	DOREPLIFETIME(UMovieSceneSequencePlayer, StartTime);
	DOREPLIFETIME(UMovieSceneSequencePlayer, DurationFrames);
	DOREPLIFETIME(UMovieSceneSequencePlayer, PlaybackSettings);
}

EMovieScenePlayerStatus::Type UMovieSceneSequencePlayer::GetPlaybackStatus() const
{
	return Status;
}

FMovieSceneSpawnRegister& UMovieSceneSequencePlayer::GetSpawnRegister()
{
	return SpawnRegister.IsValid() ? *SpawnRegister : IMovieScenePlayer::GetSpawnRegister();
}

void UMovieSceneSequencePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	bool bAllowDefault = PlaybackClient ? PlaybackClient->RetrieveBindingOverrides(InBindingId, SequenceID, OutObjects) : true;

	if (bAllowDefault)
	{
		InSequence.LocateBoundObjects(InBindingId, ResolutionContext, OutObjects);
	}
}

void UMovieSceneSequencePlayer::Play()
{
	bReversePlayback = false;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayReverse()
{
	bReversePlayback = true;
	PlayInternal();
}

void UMovieSceneSequencePlayer::ChangePlaybackDirection()
{
	bReversePlayback = !bReversePlayback;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayLooping(int32 NumLoops)
{
	PlaybackSettings.LoopCount.Value = NumLoops;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayInternal()
{
	if (!IsPlaying() && Sequence && CanPlay())
	{
		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;

		// If at the end and playing forwards, rewind to beginning
		if (GetCurrentTime().Time == GetLastValidTime())
		{
			if (PlayRate > 0.f)
			{
				JumpToFrame(FFrameTime(StartTime));
			}
		}
		else if (GetCurrentTime().Time == FFrameTime(StartTime))
		{
			if (PlayRate < 0.f)
			{
				JumpToFrame(GetLastValidTime());
			}
		}

		// Start playing
		// @todo Sequencer playback: Should we recreate the instance every time?
		// We must not recreate the instance since it holds stateful information (such as which objects it has spawned). Recreating the instance would break any 
		// @todo: Is this still the case now that eval state is stored (correctly) in the player?
		if (!RootTemplateInstance.IsValid())
		{
			RootTemplateInstance.Initialize(*Sequence, *this);
		}

		// Update now
		if (PlaybackSettings.bRestoreState)
		{
			PreAnimatedState.EnableGlobalCapture();
		}

		Status = EMovieScenePlayerStatus::Playing;
		TimeController->StartPlaying(GetCurrentTime());

		OnStartedPlaying();

		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		UMovieScene*         MovieScene         = MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;

		if (PlayPosition.GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked)
		{
			OldMaxTickRate = GEngine->GetMaxFPS();
			GEngine->SetMaxFPS(1.f / PlayPosition.GetInputRate().AsInterval());
		}

		if (!PlayPosition.GetLastPlayEvalPostition().IsSet() || PlayPosition.GetLastPlayEvalPostition() != PlayPosition.GetCurrentPosition())
		{
			UpdateMovieSceneInstance(PlayPosition.PlayTo(PlayPosition.GetCurrentPosition()), EMovieScenePlayerStatus::Playing);
		}

		UpdateNetworkSyncProperties();

		if (MovieSceneSequence)
		{
			UE_LOG(LogMovieScene, Verbose, TEXT("PlayInternal - MovieSceneSequence: %s"), *MovieSceneSequence->GetName());
		}

		if (bReversePlayback)
		{
			if (OnPlayReverse.IsBound())
			{
				OnPlayReverse.Broadcast();
			}
		}
		else
		{
			if (OnPlay.IsBound())
			{
				OnPlay.Broadcast();
			}
		}
	}
}

void UMovieSceneSequencePlayer::Pause()
{
	if (IsPlaying())
	{
		if (bIsEvaluating)
		{
			LatentActions.Emplace(FLatentAction::Pause);
			return;
		}

		Status = EMovieScenePlayerStatus::Paused;
		TimeController->StopPlaying(GetCurrentTime());

		// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly. (ie. audio sounds should stop/pause)
		{
			bIsEvaluating = true;

			FMovieSceneEvaluationRange CurrentTimeRange = PlayPosition.GetCurrentPositionAsRange();
			const FMovieSceneContext Context(CurrentTimeRange, EMovieScenePlayerStatus::Stopped);
			RootTemplateInstance.Evaluate(Context, *this);

			bIsEvaluating = false;
		}

		ApplyLatentActions();
		UpdateNetworkSyncProperties();

		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		if (MovieSceneSequence)
		{
			UE_LOG(LogMovieScene, Verbose, TEXT("Pause - MovieSceneSequence: %s"), *MovieSceneSequence->GetName());
		}

		if (OnPause.IsBound())
		{
			OnPause.Broadcast();
		}
	}
}

void UMovieSceneSequencePlayer::Scrub()
{
	// @todo Sequencer playback: Should we recreate the instance every time?
	// We must not recreate the instance since it holds stateful information (such as which objects it has spawned). Recreating the instance would break any 
	// @todo: Is this still the case now that eval state is stored (correctly) in the player?
	if (ensureAsRuntimeWarning(Sequence != nullptr))
	{
		if (!RootTemplateInstance.IsValid())
		{
			RootTemplateInstance.Initialize(*Sequence, *this);
		}
	}

	Status = EMovieScenePlayerStatus::Scrubbing;
	TimeController->StopPlaying(GetCurrentTime());

	UpdateNetworkSyncProperties();
}

void UMovieSceneSequencePlayer::Stop()
{
	StopInternal(bReversePlayback ? GetLastValidTime() : FFrameTime(StartTime));
}

void UMovieSceneSequencePlayer::StopAtCurrentTime()
{
	StopInternal(PlayPosition.GetCurrentPosition());
}

void UMovieSceneSequencePlayer::StopInternal(FFrameTime TimeToResetTo)
{
	if (IsPlaying() || IsPaused() || RootTemplateInstance.IsValid())
	{
		if (bIsEvaluating)
		{
			LatentActions.Emplace(FLatentAction::Stop, TimeToResetTo);
			return;
		}

		Status = EMovieScenePlayerStatus::Stopped;

		// Put the cursor at the specified position
		PlayPosition.Reset(TimeToResetTo);
		if (TimeController.IsValid())
		{
			TimeController->StopPlaying(GetCurrentTime());
		}

		CurrentNumLoops = 0;

		// Reset loop count on stop so that it doesn't persist to the next call to play
		PlaybackSettings.LoopCount.Value = 0;

		if (PlaybackSettings.bRestoreState)
		{
			RestorePreAnimatedState();
		}

		RootTemplateInstance.Finish(*this);

		if (OldMaxTickRate.IsSet())
		{
			GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
		}

		if (HasAuthority())
		{
			// Explicitly handle Stop() events through an RPC call
			RPC_OnStopEvent(TimeToResetTo);
		}
		UpdateNetworkSyncProperties();

		OnStopped();

		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		if (MovieSceneSequence)
		{
			UE_LOG(LogMovieScene, Verbose, TEXT("Stop - MovieSceneSequence: %s"), *MovieSceneSequence->GetName());
		}

		if (OnStop.IsBound())
		{
			OnStop.Broadcast();
		}
	}
}

void UMovieSceneSequencePlayer::GoToEndAndStop()
{
	FFrameTime Time = GetLastValidTime();
	JumpToFrame(Time);
	StopInternal(Time);
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetCurrentTime() const
{
	FFrameTime Time = PlayPosition.GetCurrentPosition();
	return FQualifiedFrameTime(Time, PlayPosition.GetInputRate());
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetDuration() const
{
	return FQualifiedFrameTime(DurationFrames, PlayPosition.GetInputRate());
}

int32 UMovieSceneSequencePlayer::GetFrameDuration() const
{
	return DurationFrames;
}

void UMovieSceneSequencePlayer::SetFrameRate(FFrameRate FrameRate)
{
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (MovieScene)
	{
		if (MovieScene->GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked && !FrameRate.IsMultipleOf(MovieScene->GetTickResolution()))
		{
			UE_LOG(LogMovieScene, Warning, TEXT("Attempting to play back a sequence with tick resolution of %f ticks per second frame locked to %f fps, which is not a multiple of the resolution."), MovieScene->GetTickResolution().AsDecimal(), FrameRate.AsDecimal());
		}
	}

	FFrameRate CurrentInputRate = PlayPosition.GetInputRate();

	StartTime      = ConvertFrameTime(StartTime,                    CurrentInputRate, FrameRate).FloorToFrame();
	DurationFrames = ConvertFrameTime(FFrameNumber(DurationFrames), CurrentInputRate, FrameRate).RoundToFrame().Value;

	PlayPosition.SetTimeBase(FrameRate, PlayPosition.GetOutputRate(), PlayPosition.GetEvaluationType());
}

void UMovieSceneSequencePlayer::SetFrameRange( int32 NewStartTime, int32 Duration )
{
	Duration = FMath::Max(Duration, 0);

	StartTime      = NewStartTime;
	DurationFrames = Duration;

	TOptional<FFrameTime> CurrentTime = PlayPosition.GetCurrentPosition();
	if (CurrentTime.IsSet())
	{
		FFrameTime LastValidTime = GetLastValidTime();

		if (CurrentTime.GetValue() < StartTime)
		{
			PlayPosition.Reset(StartTime);
		}
		else if (CurrentTime.GetValue() > LastValidTime)
		{
			PlayPosition.Reset(LastValidTime);
		}
	}

	if (TimeController.IsValid())
	{
		TimeController->Reset(GetCurrentTime());
	}

	UpdateNetworkSyncProperties();
}

void UMovieSceneSequencePlayer::SetTimeRange( float StartTimeSeconds, float DurationSeconds )
{
	const FFrameRate Rate = PlayPosition.GetInputRate();

	const FFrameNumber StartFrame = ( StartTimeSeconds * Rate ).FloorToFrame();
	const FFrameNumber Duration   = ( DurationSeconds  * Rate ).RoundToFrame();

	SetFrameRange(StartFrame.Value, Duration.Value);
}

void UMovieSceneSequencePlayer::PlayToFrame(FFrameTime NewPosition)
{
	UpdateTimeCursorPosition(NewPosition, EUpdatePositionMethod::Play);

	TimeController->Reset(GetCurrentTime());

	if (HasAuthority())
	{
		RPC_ExplicitServerUpdateEvent(EUpdatePositionMethod::Play, NewPosition);
	}
}

void UMovieSceneSequencePlayer::ScrubToFrame(FFrameTime NewPosition)
{
	UpdateTimeCursorPosition(NewPosition, EUpdatePositionMethod::Scrub);

	TimeController->Reset(GetCurrentTime());

	if (HasAuthority())
	{
		RPC_ExplicitServerUpdateEvent(EUpdatePositionMethod::Scrub, NewPosition);
	}
}

void UMovieSceneSequencePlayer::JumpToFrame(FFrameTime NewPosition)
{
	UpdateTimeCursorPosition(NewPosition, EUpdatePositionMethod::Jump);

	TimeController->Reset(GetCurrentTime());

	if (HasAuthority())
	{
		RPC_ExplicitServerUpdateEvent(EUpdatePositionMethod::Jump, NewPosition);
	}
}

void UMovieSceneSequencePlayer::PlayToSeconds(float TimeInSeconds)
{
	PlayToFrame(TimeInSeconds * PlayPosition.GetInputRate());
}

void UMovieSceneSequencePlayer::ScrubToSeconds(float TimeInSeconds)
{
	ScrubToFrame(TimeInSeconds * PlayPosition.GetInputRate());
}

void UMovieSceneSequencePlayer::JumpToSeconds(float TimeInSeconds)
{
	JumpToFrame(TimeInSeconds * PlayPosition.GetInputRate());
}


int32 UMovieSceneSequencePlayer::FindMarkedFrameByLabel(const FString& InLabel) const
{
	if (!Sequence)
	{
		return INDEX_NONE;
	}

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

	int32 MarkedIndex = MovieScene->FindMarkedFrameByLabel(InLabel);
	return MarkedIndex;
}

bool UMovieSceneSequencePlayer::PlayToMarkedFrame(const FString& InLabel)
{
	int32 MarkedIndex = FindMarkedFrameByLabel(InLabel);

	if (MarkedIndex != INDEX_NONE)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		PlayToFrame(ConvertFrameTime(MovieScene->GetMarkedFrames()[MarkedIndex].FrameNumber, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()));

		return true;
	}

	return false;
}

bool UMovieSceneSequencePlayer::ScrubToMarkedFrame(const FString& InLabel)
{
	int32 MarkedIndex = FindMarkedFrameByLabel(InLabel);

	if (MarkedIndex != INDEX_NONE)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		ScrubToFrame(ConvertFrameTime(MovieScene->GetMarkedFrames()[MarkedIndex].FrameNumber, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()));

		return true;
	}

	return false;
}

bool UMovieSceneSequencePlayer::JumpToMarkedFrame(const FString& InLabel)
{
	int32 MarkedIndex = FindMarkedFrameByLabel(InLabel);

	if (MarkedIndex != INDEX_NONE)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		JumpToFrame(ConvertFrameTime(MovieScene->GetMarkedFrames()[MarkedIndex].FrameNumber, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate()));

		return true;
	}

	return false;
}

bool UMovieSceneSequencePlayer::IsPlaying() const
{
	return Status == EMovieScenePlayerStatus::Playing;
}

bool UMovieSceneSequencePlayer::IsPaused() const
{
	return Status == EMovieScenePlayerStatus::Paused;
}

bool UMovieSceneSequencePlayer::IsReversed() const
{
	return bReversePlayback;
}

float UMovieSceneSequencePlayer::GetLength() const
{
	return GetDuration().AsSeconds();
}

float UMovieSceneSequencePlayer::GetPlayRate() const
{
	return PlaybackSettings.PlayRate;
}

void UMovieSceneSequencePlayer::SetPlayRate(float PlayRate)
{
	PlaybackSettings.PlayRate = PlayRate;
}

void UMovieSceneSequencePlayer::SetPlaybackRange( float NewStartTime, float NewEndTime )
{
	SetTimeRange(NewStartTime, NewEndTime - NewStartTime);
}

FFrameTime UMovieSceneSequencePlayer::GetLastValidTime() const
{
	return DurationFrames > 0 ? FFrameTime(StartTime + DurationFrames-1, 0.99999994f) : FFrameTime(StartTime);
}

bool UMovieSceneSequencePlayer::ShouldStopOrLoop(FFrameTime NewPosition) const
{
	bool bShouldStopOrLoop = false;
	if (IsPlaying())
	{
		if (!bReversePlayback)
		{
			bShouldStopOrLoop = NewPosition.FrameNumber >= StartTime + GetFrameDuration();
		}
		else
		{
			bShouldStopOrLoop = NewPosition.FrameNumber < StartTime ;
		}
	}

	return bShouldStopOrLoop;
}

void UMovieSceneSequencePlayer::Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings)
{
	check(InSequence);
	check(!bIsEvaluating);

	// If we have a valid sequence that may have been played back,
	// Explicitly stop and tear down the template instance before 
	// reinitializing it with the new sequence. Care should be taken
	// here that Stop is not called on the first Initialization as this
	// may be called during PostLoad.
	if (Sequence)
	{
		StopAtCurrentTime();
	}

	Sequence = InSequence;
	PlaybackSettings = InSettings;

	FFrameTime StartTimeWithOffset = StartTime;

	EUpdateClockSource ClockToUse = EUpdateClockSource::Tick;

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		EMovieSceneEvaluationType EvaluationType    = MovieScene->GetEvaluationType();
		FFrameRate                TickResolution    = MovieScene->GetTickResolution();
		FFrameRate                DisplayRate       = MovieScene->GetDisplayRate();

		UE_LOG(LogMovieScene, Verbose, TEXT("Initialize - MovieSceneSequence: %s, TickResolution: %f, DisplayRate: %d, CurrentTime: %d"), *InSequence->GetName(), TickResolution.Numerator, DisplayRate.Numerator);

		// We set the play position in terms of the display rate,
		// but want evaluation ranges in the moviescene's tick resolution
		PlayPosition.SetTimeBase(DisplayRate, TickResolution, EvaluationType);

		{
			// Set up the default frame range from the sequence's play range
			TRange<FFrameNumber> PlaybackRange   = MovieScene->GetPlaybackRange();

			const FFrameNumber SrcStartFrame = MovieScene::DiscreteInclusiveLower(PlaybackRange);
			const FFrameNumber SrcEndFrame   = MovieScene::DiscreteExclusiveUpper(PlaybackRange);

			const FFrameNumber StartingFrame = ConvertFrameTime(SrcStartFrame, TickResolution, DisplayRate).FloorToFrame();
			const FFrameNumber EndingFrame   = ConvertFrameTime(SrcEndFrame,   TickResolution, DisplayRate).FloorToFrame();

			SetFrameRange(StartingFrame.Value, (EndingFrame - StartingFrame).Value);
		}

		// Reset the play position based on the user-specified start offset, or a random time
		FFrameTime SpecifiedStartOffset = PlaybackSettings.StartTime * DisplayRate;

		// Setup the starting time
		FFrameTime StartingTimeOffset = PlaybackSettings.bRandomStartTime
			? FFrameTime(FMath::Rand() % GetFrameDuration())
			: FMath::Clamp<FFrameTime>(SpecifiedStartOffset, 0, GetFrameDuration()-1);

		StartTimeWithOffset = StartTime + StartingTimeOffset;

		ClockToUse = MovieScene->GetClockSource();
	}

	if (!TimeController.IsValid())
	{
		switch (ClockToUse)
		{
		case EUpdateClockSource::Audio:    TimeController = MakeShared<FMovieSceneTimeController_AudioClock>();    break;
		case EUpdateClockSource::Platform: TimeController = MakeShared<FMovieSceneTimeController_PlatformClock>(); break;
		case EUpdateClockSource::Timecode: TimeController = MakeShared<FMovieSceneTimeController_TimecodeClock>(); break;
		default:                           TimeController = MakeShared<FMovieSceneTimeController_Tick>();          break;
		}
	}

	RootTemplateInstance.Initialize(*Sequence, *this);

	// Set up playback position (with offset) after Stop(), which will reset the starting time to StartTime
	PlayPosition.Reset(StartTimeWithOffset);
	TimeController->Reset(GetCurrentTime());
}

void UMovieSceneSequencePlayer::Update(const float DeltaSeconds)
{
	if (IsPlaying())
	{
		// Delta seconds has already been multiplied by MatineeTimeDilation at this point, so don't pass that through to Tick
		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;

		TimeController->Tick(DeltaSeconds, PlayRate);

		UWorld* World = GetPlaybackWorld();
		if (World)
		{
			PlayRate *= World->GetWorldSettings()->MatineeTimeDilation;
		}

		FFrameTime NewTime = TimeController->RequestCurrentTime(GetCurrentTime(), PlayRate);
		UpdateTimeCursorPosition(NewTime, EUpdatePositionMethod::Play);
	}
}

void UMovieSceneSequencePlayer::UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method)
{
	if (bIsEvaluating)
	{
		LatentActions.Emplace(Method, NewPosition);
	}
	else
	{
		UpdateTimeCursorPosition_Internal(NewPosition, Method);
	}
}

EMovieScenePlayerStatus::Type UpdateMethodToStatus(EUpdatePositionMethod Method)
{
	switch(Method)
	{
	case EUpdatePositionMethod::Scrub: return EMovieScenePlayerStatus::Scrubbing;
	case EUpdatePositionMethod::Jump:  return EMovieScenePlayerStatus::Stopped;
	case EUpdatePositionMethod::Play:  return EMovieScenePlayerStatus::Playing;
	default:                           return EMovieScenePlayerStatus::Stopped;
	}
}

FMovieSceneEvaluationRange UpdatePlayPosition(FMovieScenePlaybackPosition& InOutPlayPosition, FFrameTime NewTime, EUpdatePositionMethod Method)
{
	if (Method == EUpdatePositionMethod::Play)
	{
		return InOutPlayPosition.PlayTo(NewTime);
	}

	return InOutPlayPosition.JumpTo(NewTime);
}

void UMovieSceneSequencePlayer::UpdateTimeCursorPosition_Internal(FFrameTime NewPosition, EUpdatePositionMethod Method)
{
	EMovieScenePlayerStatus::Type StatusOverride = UpdateMethodToStatus(Method);

	const int32 Duration = DurationFrames;
	if (Duration == 0)
	{
		UE_LOG(LogMovieScene, Warning, TEXT("Attempting to play back a sequence with zero duration"));
		return;
	}
	
	if (Method == EUpdatePositionMethod::Play && ShouldStopOrLoop(NewPosition))
	{
		// The actual start time taking into account reverse playback
		FFrameNumber StartTimeWithReversed = bReversePlayback ? StartTime + Duration : StartTime;

		FFrameTime PositionRelativeToStart = NewPosition.FrameNumber - StartTimeWithReversed;

		const int32 NumTimesLooped    = FMath::Abs(PositionRelativeToStart.FrameNumber.Value / Duration);
		const bool  bLoopIndefinitely = PlaybackSettings.LoopCount.Value < 0;

		// loop playback
		if (bLoopIndefinitely || CurrentNumLoops + NumTimesLooped <= PlaybackSettings.LoopCount.Value)
		{
			CurrentNumLoops += NumTimesLooped;

			const FFrameTime Overplay       = FFrameTime(PositionRelativeToStart.FrameNumber.Value % Duration, PositionRelativeToStart.GetSubFrame());
			FFrameTime NewFrameOffset;
			
			if (bReversePlayback)
			{
				NewFrameOffset = (Overplay > 0) ?  FFrameTime(Duration) + Overplay : Overplay;
			}
			else
			{
				NewFrameOffset = (Overplay < 0) ? FFrameTime(Duration) + Overplay : Overplay;
			}

			if (SpawnRegister.IsValid())
			{
				SpawnRegister->ForgetExternallyOwnedSpawnedObjects(State, *this);
			}

			// Reset the play position, and generate a new range that gets us to the new frame time
			if (bReversePlayback)
			{
				PlayPosition.Reset(Overplay > 0 ? GetLastValidTime() : StartTimeWithReversed);
			}
			else
			{
				PlayPosition.Reset(Overplay < 0 ? GetLastValidTime() : StartTimeWithReversed);
			}

			FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(StartTimeWithReversed + NewFrameOffset);

			const bool bHasJumped = true;
			UpdateMovieSceneInstance(Range, StatusOverride, bHasJumped);

			// Use the exact time here rather than a frame locked time to ensure we don't skip the amount that was overplayed in the time controller
			FQualifiedFrameTime ExactCurrentTime(StartTimeWithReversed + NewFrameOffset, PlayPosition.GetInputRate());
			TimeController->Reset(ExactCurrentTime);

			OnLooped();
		}

		// stop playback
		else
		{
			// Clamp the position to the duration
			NewPosition = FMath::Clamp(NewPosition, FFrameTime(StartTime), GetLastValidTime());

			FMovieSceneEvaluationRange Range = UpdatePlayPosition(PlayPosition, NewPosition, Method);
			UpdateMovieSceneInstance(Range, StatusOverride);

			if (PlaybackSettings.bPauseAtEnd)
			{
				Pause();
			}
			else
			{
				StopInternal(NewPosition);
			}

			TimeController->StopPlaying(GetCurrentTime());

			if (OnFinished.IsBound())
			{
				OnFinished.Broadcast();
			}
		}
	}
	else
	{
		// Just update the time and sequence
		FMovieSceneEvaluationRange Range = UpdatePlayPosition(PlayPosition, NewPosition, Method);
		UpdateMovieSceneInstance(Range, StatusOverride);
	}

	UpdateNetworkSyncProperties();
}

void UMovieSceneSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped)
{
#if !NO_LOGGING
	UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
	if (MovieSceneSequence)
	{
		FQualifiedFrameTime CurrentTime = GetCurrentTime();
		UE_LOG(LogMovieScene, VeryVerbose, TEXT("Evaluating sequence %s at frame %d, subframe %f (%f fps)."), *MovieSceneSequence->GetName(), CurrentTime.Time.FrameNumber.Value, CurrentTime.Time.GetSubFrame(), CurrentTime.Rate.AsDecimal());
	}
#endif

	bIsEvaluating = true;

	FMovieSceneContext Context(InRange, PlayerStatus);
	Context.SetHasJumped(bHasJumped);

	RootTemplateInstance.Evaluate(Context, *this);

#if WITH_EDITOR
	FFrameTime CurrentTime  = ConvertFrameTime(Context.GetTime(),         Context.GetFrameRate(), PlayPosition.GetInputRate());
	FFrameTime PreviousTime = ConvertFrameTime(Context.GetPreviousTime(), Context.GetFrameRate(), PlayPosition.GetInputRate());
	OnMovieSceneSequencePlayerUpdate.Broadcast(*this, CurrentTime, PreviousTime);
#endif
	bIsEvaluating = false;

	ApplyLatentActions();
}

void UMovieSceneSequencePlayer::ApplyLatentActions()
{
	// Swap to a stack array to ensure no reentrancy if we evaluate during a pause, for instance
	TArray<FLatentAction> TheseActions;
	Swap(TheseActions, LatentActions);

	for (const FLatentAction& LatentAction : TheseActions)
	{
		switch (LatentAction.Type)
		{
		case FLatentAction::Stop:          StopInternal(LatentAction.Position); continue;
		case FLatentAction::Pause:         Pause();                             continue;
		}

		check(LatentAction.Type == FLatentAction::Update);
		switch (LatentAction.UpdateMethod)
		{
		case EUpdatePositionMethod::Play:  PlayToFrame( LatentAction.Position); continue;
		case EUpdatePositionMethod::Jump:  JumpToFrame( LatentAction.Position); continue;
		case EUpdatePositionMethod::Scrub: ScrubToFrame(LatentAction.Position); continue;
		}
	}
}

void UMovieSceneSequencePlayer::SetPlaybackClient(TScriptInterface<IMovieScenePlaybackClient> InPlaybackClient)
{
	PlaybackClient = InPlaybackClient;
}

void UMovieSceneSequencePlayer::SetTimeController(TSharedPtr<FMovieSceneTimeController> InTimeController)
{
	TimeController = InTimeController;
	if (TimeController.IsValid())
	{
		TimeController->Reset(GetCurrentTime());
	}
}

TArray<UObject*> UMovieSceneSequencePlayer::GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding)
{
	TArray<UObject*> Objects;
	for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ObjectBinding.GetGuid(), ObjectBinding.GetSequenceID()))
	{
		if (UObject* Object = WeakObject.Get())
		{
			Objects.Add(Object);
		}
	}
	return Objects;
}

TArray<FMovieSceneObjectBindingID> UMovieSceneSequencePlayer::GetObjectBindings(UObject* InObject)
{
	TArray<FMovieSceneObjectBindingID> ObjectBindings;

	for (FMovieSceneSequenceIDRef SequenceID : GetEvaluationTemplate().GetThisFrameMetaData().ActiveSequences)
	{
		FGuid ObjectGuid = FindObjectId(*InObject, SequenceID);
		if (ObjectGuid.IsValid())
		{
			FMovieSceneObjectBindingID ObjectBinding(ObjectGuid, SequenceID);

			ObjectBindings.Add(ObjectBinding);
		}
	}

	return ObjectBindings;
}

void UMovieSceneSequencePlayer::BeginDestroy()
{
	Stop();

	if (GEngine && OldMaxTickRate.IsSet())
	{
		GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
	}

	Super::BeginDestroy();
}

UWorld* UMovieSceneSequencePlayer::GetPlaybackWorld() const
{
	UObject* PlaybackContext = GetPlaybackContext();
	return PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
}bool UMovieSceneSequencePlayer::HasAuthority() const
{
	AActor* Actor = GetTypedOuter<AActor>();
	return Actor && Actor->HasAuthority() && !IsPendingKillOrUnreachable();
}

void UMovieSceneSequencePlayer::RPC_ExplicitServerUpdateEvent_Implementation(EUpdatePositionMethod EventMethod, FFrameTime MarkerTime)
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Handle an explicit jump/play/scrub command from the server.

	if (HasAuthority() || !Sequence)
	{
		// Never run network sync operations on authoritative players
		return;
	}

#if !NO_LOGGING
	// Log the sync event if necessary
	if (UE_LOG_ACTIVE(LogMovieSceneRepl, Verbose))
	{
		FFrameTime   CurrentTime     = PlayPosition.GetCurrentPosition();
		FString      SequenceName    = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root)->GetName();

		AActor* Actor = GetTypedOuter<AActor>();
		if (Actor && Actor->GetWorld()->GetNetMode() == NM_Client)
		{
			SequenceName += FString::Printf(TEXT(" (client %d)"), GPlayInEditorID - 1);
		}

		UE_LOG(LogMovieSceneRepl, Verbose, TEXT("Explicit update event for sequence %s %s @ frame %d, subframe %f. Server has moved to frame %d, subframe %f with EUpdatePositionMethod::%s."),
			*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
			NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame(), *UEnum::GetValueAsString(TEXT("MovieScene.EUpdatePositionMethod"), NetSyncProps.LastKnownStatus.GetValue()));
	}
#endif

	// Explicitly repeat the authoritative update event on this client.

	// Note: in the case of PlayToFrame this will not necessarily sweep the exact same range as the server did
	// because this client player is unlikely to be at exactly the same time that the server was at when it performed the operation.
	// This is irrelevant for jumps and scrubs as only the new time is meaningful.
	switch (EventMethod)
	{
	case EUpdatePositionMethod::Play:  PlayToFrame( MarkerTime);  break;
	case EUpdatePositionMethod::Jump:  JumpToFrame( MarkerTime);  break;
	case EUpdatePositionMethod::Scrub: ScrubToFrame(MarkerTime);  break;
	}
}

void UMovieSceneSequencePlayer::RPC_OnStopEvent_Implementation(FFrameTime StoppedTime)
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Handle an explicit Stop command from the server.

	if (HasAuthority() || !Sequence)
	{
		// Never run network sync operations on authoritative players or players that have not been initialized yet
		return;
	}

#if !NO_LOGGING
	if (UE_LOG_ACTIVE(LogMovieSceneRepl, Verbose))
	{
		FFrameTime CurrentTime  = PlayPosition.GetCurrentPosition();
		FString    SequenceName = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root)->GetName();

		AActor* Actor = GetTypedOuter<AActor>();
		if (Actor && Actor->GetWorld()->GetNetMode() == NM_Client)
		{
			SequenceName += FString::Printf(TEXT(" (client %d)"), GPlayInEditorID - 1);
		}

		UE_LOG(LogMovieSceneRepl, Verbose, TEXT("Explicit Stop() event for sequence %s %s @ frame %d, subframe %f. Server has stopped at frame %d, subframe %f."),
			*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
			NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame());
	}
#endif

	switch (Status.GetValue())
	{
	case EMovieScenePlayerStatus::Playing:   PlayToFrame( StoppedTime);  break;
	case EMovieScenePlayerStatus::Stopped:   JumpToFrame( StoppedTime);  break;
	case EMovieScenePlayerStatus::Scrubbing: ScrubToFrame(StoppedTime);  break;
	}

	StopInternal(StoppedTime);
}

void UMovieSceneSequencePlayer::PostNetReceive()
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Handle a passive update of the replicated status and time properties of the player.

	Super::PostNetReceive();

	if (!ensure(!HasAuthority()) || !Sequence)
	{
		// Never run network sync operations on authoritative players or players that have not been initialized yet
		return;
	}

	const bool bHasStartedPlaying = NetSyncProps.LastKnownStatus == EMovieScenePlayerStatus::Playing && Status != EMovieScenePlayerStatus::Playing;
	const bool bHasChangedStatus  = NetSyncProps.LastKnownStatus   != Status;
	const bool bHasChangedTime    = NetSyncProps.LastKnownPosition != PlayPosition.GetCurrentPosition();

	const FFrameTime LagThreshold = 0.2f * PlayPosition.GetInputRate();
	const FFrameTime LagDisparity = FMath::Abs(PlayPosition.GetCurrentPosition() - NetSyncProps.LastKnownPosition);

	if (!bHasChangedStatus && !bHasChangedTime)
	{
		// Nothing to do
		return;
	}

#if !NO_LOGGING
	if (UE_LOG_ACTIVE(LogMovieSceneRepl, VeryVerbose))
	{
		FFrameTime CurrentTime  = PlayPosition.GetCurrentPosition();
		FString    SequenceName = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root)->GetName();

		AActor* Actor = GetTypedOuter<AActor>();
		if (Actor->GetWorld()->GetNetMode() == NM_Client)
		{
			SequenceName += FString::Printf(TEXT(" (client %d)"), GPlayInEditorID - 1);
		}

		UE_LOG(LogMovieSceneRepl, VeryVerbose, TEXT("Network sync for sequence %s %s @ frame %d, subframe %f. Server is %s @ frame %d, subframe %f."),
			*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
			*UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), NetSyncProps.LastKnownStatus.GetValue()), NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame());
	}
#endif

	// Deal with changes of state from stopped <-> playing separately, as they require slightly different considerations
	if (bHasStartedPlaying)
	{
		// Note: when starting playback, we assume that the client and server were at the same time prior to the server initiating playback

		// Initiate playback from our current position
		PlayInternal();

		if (LagDisparity > LagThreshold)
		{
			// Synchronize to the server time as best we can if there is a large disparity
			PlayToFrame(NetSyncProps.LastKnownPosition);
		}
	}
	else
	{
		if (bHasChangedTime)
		{
			// Make sure the client time matches the server according to the client's current status
			if (Status == EMovieScenePlayerStatus::Playing)
			{
				// When the server has looped back to the start but a client is near the end (and is thus about to loop), we don't want to forcibly synchronize the time unless
				// the *real* difference in time is above the threshold. We compute the real-time difference by adding SequenceDuration*LoopCountDifference to the server position:
				//		start	srv_time																																clt_time		end
				//		0		1		2		3		4		5		6		7		8		9		10		11		12		13		14		15		16		17		18		19		20
				//		|		|																																		|				|
				//
				//		Let NetSyncProps.LastKnownNumLoops = 1, CurrentNumLoops = 0, bReversePlayback = false
				//			=> LoopOffset = 1
				//			   OffsetServerTime = srv_time + FrameDuration*LoopOffset = 1 + 20*1 = 21
				//			   Difference = 21 - 18 = 3 frames
				static float       ThresholdS       = 0.2f;
				const int32        LoopOffset       = (NetSyncProps.LastKnownNumLoops - CurrentNumLoops) * (bReversePlayback ? -1 : 1);
				const FFrameTime   FrameThreshold   = ThresholdS * PlayPosition.GetInputRate();
				const FFrameTime   OffsetServerTime = NetSyncProps.LastKnownPosition + GetFrameDuration()*LoopOffset;
				const FFrameTime   Difference       = FMath::Abs(PlayPosition.GetCurrentPosition() - OffsetServerTime);

				if (bHasChangedStatus)
				{
					// If the status has changed forcibly play to the server position before setting the new status
					PlayToFrame(NetSyncProps.LastKnownPosition);
				}
				else if (Difference > FrameThreshold)
				{
					// We're drastically out of sync with the server so we need to forcibly set the time.
					// Play to the time only if it is further on in the sequence (in our play direction)
					const bool bPlayToFrame = bReversePlayback ? NetSyncProps.LastKnownPosition < PlayPosition.GetCurrentPosition() : NetSyncProps.LastKnownPosition > PlayPosition.GetCurrentPosition();
					if (bPlayToFrame)
					{
						PlayToFrame(NetSyncProps.LastKnownPosition);
					}
					else
					{
						JumpToFrame(NetSyncProps.LastKnownPosition);
					}
				}
			}
			else if (Status == EMovieScenePlayerStatus::Stopped)
			{
				JumpToFrame(NetSyncProps.LastKnownPosition);
			}
			else if (Status == EMovieScenePlayerStatus::Scrubbing)
			{
				ScrubToFrame(NetSyncProps.LastKnownPosition);
			}
		}

		if (bHasChangedStatus)
		{
			switch (NetSyncProps.LastKnownStatus)
			{
			case EMovieScenePlayerStatus::Paused:    Pause(); break;
			case EMovieScenePlayerStatus::Playing:   Play();  break;
			case EMovieScenePlayerStatus::Scrubbing: Scrub(); break;
			}
		}
	}
}

int32 UMovieSceneSequencePlayer::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}

	check(GetOuter());
	return GetOuter()->GetFunctionCallspace(Function, Parameters, Stack);
}

bool UMovieSceneSequencePlayer::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	AActor*     Actor     = GetTypedOuter<AActor>();
	UNetDriver* NetDriver = Actor ? Actor->GetNetDriver() : nullptr;
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, this);
		return true;
	}

	return false;
}
