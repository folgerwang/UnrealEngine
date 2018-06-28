// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"
#include "MovieSceneSequence.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/RuntimeErrors.h"

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
}

UMovieSceneSequencePlayer::~UMovieSceneSequencePlayer()
{
	if (GEngine && OldMaxTickRate.IsSet())
	{
		GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
	}
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
	bool bAllowDefault = PlaybackSettings.BindingOverrides ? PlaybackSettings.BindingOverrides->LocateBoundObjects(InBindingId, SequenceID, OutObjects) : true;

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
	PlaybackSettings.LoopCount = NumLoops;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayInternal()
{
	if (!IsPlaying() && Sequence && CanPlay())
	{
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
		PlaybackSettings.TimeController->StartPlaying(GetCurrentTime());

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
		PlaybackSettings.TimeController->StopPlaying(GetCurrentTime());

		// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly. (ie. audio sounds should stop/pause)
		{
			bIsEvaluating = true;

			FMovieSceneEvaluationRange CurrentTimeRange = PlayPosition.GetCurrentPositionAsRange();
			const FMovieSceneContext Context(CurrentTimeRange, EMovieScenePlayerStatus::Stopped);
			RootTemplateInstance.Evaluate(Context, *this);

			bIsEvaluating = false;
		}

		ApplyLatentActions();

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
	PlaybackSettings.TimeController->StopPlaying(GetCurrentTime());
}

void UMovieSceneSequencePlayer::Stop()
{
	if (IsPlaying() || IsPaused())
	{
		if (bIsEvaluating)
		{
			LatentActions.Emplace(FLatentAction::Stop);
			return;
		}

		Status = EMovieScenePlayerStatus::Stopped;

		// Put the cursor back to the start
		PlayPosition.Reset(bReversePlayback ? GetLastValidTime() : FFrameTime(StartTime));

		PlaybackSettings.TimeController->StopPlaying(GetCurrentTime());

		CurrentNumLoops = 0;

		if (PlaybackSettings.bRestoreState)
		{
			RestorePreAnimatedState();
		}

		RootTemplateInstance.Finish(*this);

		if (OldMaxTickRate.IsSet())
		{
			GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
		}

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
	JumpToFrame(GetLastValidTime());
	Stop();
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

	if (PlaybackSettings.TimeController.IsValid())
	{
		PlaybackSettings.TimeController->Reset(GetCurrentTime());
	}
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

	PlaybackSettings.TimeController->Reset(GetCurrentTime());
}

void UMovieSceneSequencePlayer::ScrubToFrame(FFrameTime NewPosition)
{
	UpdateTimeCursorPosition(NewPosition, EUpdatePositionMethod::Scrub);

	PlaybackSettings.TimeController->Reset(GetCurrentTime());
}

void UMovieSceneSequencePlayer::JumpToFrame(FFrameTime NewPosition)
{
	UpdateTimeCursorPosition(NewPosition, EUpdatePositionMethod::Jump);

	PlaybackSettings.TimeController->Reset(GetCurrentTime());
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

	Sequence = InSequence;
	PlaybackSettings = InSettings;

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

		PlayPosition.Reset(StartTime + StartingTimeOffset);

		ClockToUse = MovieScene->GetClockSource();
	}

	if (!PlaybackSettings.TimeController.IsValid())
	{
		switch (ClockToUse)
		{
		case EUpdateClockSource::Audio:    PlaybackSettings.TimeController = MakeShared<FMovieSceneTimeController_AudioClock>();    break;
		case EUpdateClockSource::Platform: PlaybackSettings.TimeController = MakeShared<FMovieSceneTimeController_PlatformClock>(); break;
		default:                           PlaybackSettings.TimeController = MakeShared<FMovieSceneTimeController_Tick>();          break;
		}
	}

	PlaybackSettings.TimeController->Reset(GetCurrentTime());
	RootTemplateInstance.Initialize(*Sequence, *this);

	// Ensure everything is set up, ready for playback
	Stop();
}

void UMovieSceneSequencePlayer::Update(const float DeltaSeconds)
{
	if (IsPlaying())
	{
		// Delta seconds has already been multiplied by MatineeTimeDilation at this point, so don't pass that through to Tick
		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;
		PlaybackSettings.TimeController->Tick(DeltaSeconds, PlayRate);

		UWorld* World = GetPlaybackWorld();
		if (World)
		{
			PlayRate *= World->GetWorldSettings()->MatineeTimeDilation;
		}

		FFrameTime NewTime = PlaybackSettings.TimeController->RequestCurrentTime(GetCurrentTime(), PlayRate);
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
		const bool  bLoopIndefinitely = PlaybackSettings.LoopCount < 0;

		// loop playback
		if (bLoopIndefinitely || CurrentNumLoops + NumTimesLooped <= PlaybackSettings.LoopCount)
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
			PlaybackSettings.TimeController->Reset(ExactCurrentTime);

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
				Stop();

				// When playback stops naturally, the time cursor is put at the boundary that was crossed to make ping-pong playback easy
				PlayPosition.Reset(NewPosition);
			}

			PlaybackSettings.TimeController->StopPlaying(GetCurrentTime());

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
		case FLatentAction::Stop:          Stop();                         continue;
		case FLatentAction::Pause:         Pause();                        continue;
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
}