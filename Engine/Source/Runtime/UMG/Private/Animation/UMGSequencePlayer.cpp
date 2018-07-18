// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/UMGSequencePlayer.h"
#include "MovieScene.h"
#include "UMGPrivate.h"
#include "Animation/WidgetAnimation.h"
#include "MovieSceneTimeHelpers.h"


UUMGSequencePlayer::UUMGSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
	TimeCursorPosition = FFrameTime(0);
	PlaybackSpeed = 1;
	Animation = nullptr;
	bIsEvaluating = false;
}

void UUMGSequencePlayer::InitSequencePlayer( UWidgetAnimation& InAnimation, UUserWidget& InUserWidget )
{
	Animation = &InAnimation;
	UserWidget = &InUserWidget;


	UMovieScene* MovieScene = Animation->GetMovieScene();

	// Cache the time range of the sequence to determine when we stop
	Duration = MovieScene::DiscreteSize(MovieScene->GetPlaybackRange());
	AnimationResolution = MovieScene->GetTickResolution();
	AbsolutePlaybackStart = MovieScene::DiscreteInclusiveLower(MovieScene->GetPlaybackRange());
}

void UUMGSequencePlayer::Tick(float DeltaTime)
{
	if ( PlayerStatus == EMovieScenePlayerStatus::Playing )
	{
		FFrameTime DeltaFrameTime = (bIsPlayingForward ? DeltaTime * PlaybackSpeed : -DeltaTime * PlaybackSpeed) * AnimationResolution;

		FFrameTime LastTimePosition = TimeCursorPosition;
		TimeCursorPosition += DeltaFrameTime;

		// Check if we crossed over bounds
		const bool bCrossedLowerBound = TimeCursorPosition < 0;
		const bool bCrossedUpperBound = TimeCursorPosition >= FFrameTime(Duration);
		const bool bCrossedEndTime = bIsPlayingForward
			? LastTimePosition < EndTime && EndTime <= TimeCursorPosition
			: LastTimePosition > EndTime && EndTime >= TimeCursorPosition;

		// Increment the num loops if we crossed any bounds.
		if (bCrossedLowerBound || bCrossedUpperBound || (bCrossedEndTime && NumLoopsCompleted >= NumLoopsToPlay - 1))
		{
			NumLoopsCompleted++;
		}

		// Did the animation complete
		const bool bCompleted = NumLoopsToPlay != 0 && NumLoopsCompleted >= NumLoopsToPlay;

		// Handle Times
		if (bCrossedLowerBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = FFrameTime(0);
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = FMath::Abs(TimeCursorPosition);
				}
				else
				{
					TimeCursorPosition += FFrameTime(Duration);
					LastTimePosition = TimeCursorPosition;
				}
			}
		}
		else if (bCrossedUpperBound)
		{
			FFrameTime LastValidFrame(Duration-1, 0.99999994f);

			if (bCompleted)
			{
				TimeCursorPosition = LastValidFrame;
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = LastValidFrame - (TimeCursorPosition - FFrameTime(Duration));
				}
				else
				{
					TimeCursorPosition = TimeCursorPosition - FFrameTime(Duration);
					LastTimePosition = TimeCursorPosition;
				}
			}
		}
		else if (bCrossedEndTime)
		{
			if (bCompleted)
			{
				TimeCursorPosition = EndTime;
			}
		}
		if (RootTemplateInstance.IsValid())
		{
			UMovieScene* MovieScene = Animation->GetMovieScene();

			bIsEvaluating = true;

			const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart + TimeCursorPosition, AbsolutePlaybackStart + LastTimePosition, AnimationResolution), PlayerStatus);
			RootTemplateInstance.Evaluate(Context, *this);

			bIsEvaluating = false;

			ApplyLatentActions();
		}

		if ( bCompleted )
		{
			PlayerStatus = EMovieScenePlayerStatus::Stopped;
			OnSequenceFinishedPlayingEvent.Broadcast(*this);
			Animation->OnAnimationFinished.Broadcast();
		}
	}
}

void UUMGSequencePlayer::PlayInternal(double StartAtTime, double EndAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	RootTemplateInstance.Initialize(*Animation, *this);

	PlaybackSpeed = FMath::Abs(InPlaybackSpeed);
	PlayMode = InPlayMode;

	FFrameTime LastValidFrame(Duration-1, 0.99999994f);

	if (PlayMode == EUMGSequencePlayMode::Reverse)
	{
		// When playing in reverse count subtract the start time from the end.
		TimeCursorPosition = LastValidFrame - StartAtTime * AnimationResolution;
	}
	else
	{
		TimeCursorPosition = StartAtTime * AnimationResolution;
	}

	// Clamp the start time and end time to be within the bounds
	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, FFrameTime(0), LastValidFrame);
	EndTime = FMath::Clamp(EndAtTime * AnimationResolution, FFrameTime(0), LastValidFrame);

	if ( PlayMode == EUMGSequencePlayMode::PingPong )
	{
		// When animating in ping-pong mode double the number of loops to play so that a loop is a complete forward/reverse cycle.
		NumLoopsToPlay = 2 * InNumLoopsToPlay;
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}

	NumLoopsCompleted = 0;
	bIsPlayingForward = InPlayMode != EUMGSequencePlayMode::Reverse;

	// Immediately evaluate the first frame of the animation so that if tick has already occurred, the widget is setup correctly and ready to be
	// rendered using the first frames data, otherwise you may see a *pop* due to a widget being constructed with a default different than the
	// first frame of the animation.
	if (RootTemplateInstance.IsValid())
	{
		const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart + TimeCursorPosition, AbsolutePlaybackStart + TimeCursorPosition, AnimationResolution), PlayerStatus);
		RootTemplateInstance.Evaluate(Context, *this);
	}

	PlayerStatus = EMovieScenePlayerStatus::Playing;
	Animation->OnAnimationStarted.Broadcast();
}

void UUMGSequencePlayer::Play(float StartAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	PlayInternal(StartAtTime, 0.0, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed);
}

void UUMGSequencePlayer::PlayTo(float StartAtTime, float EndAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	PlayInternal(StartAtTime, EndAtTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed);
}

void UUMGSequencePlayer::Pause()
{
	if (bIsEvaluating)
	{
		LatentActions.Add(ELatentAction::Pause);
		return;
	}

	// Purposely don't trigger any OnFinished events
	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly. (ie. audio sounds should stop/pause)
	const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart + TimeCursorPosition, AbsolutePlaybackStart + TimeCursorPosition, AnimationResolution), PlayerStatus);
	RootTemplateInstance.Evaluate(Context, *this);

	ApplyLatentActions();
}

void UUMGSequencePlayer::Reverse()
{
	if (PlayerStatus == EMovieScenePlayerStatus::Playing)
	{
		bIsPlayingForward = !bIsPlayingForward;
	}
}

void UUMGSequencePlayer::Stop()
{
	if (bIsEvaluating)
	{
		LatentActions.Add(ELatentAction::Stop);
		return;
	}

	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	if (RootTemplateInstance.IsValid())
	{
		const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart, AnimationResolution), PlayerStatus);
		RootTemplateInstance.Evaluate(Context, *this);
		RootTemplateInstance.Finish(*this);
	}

	OnSequenceFinishedPlayingEvent.Broadcast(*this);
	Animation->OnAnimationFinished.Broadcast();

	TimeCursorPosition = FFrameTime(0);
}

void UUMGSequencePlayer::SetNumLoopsToPlay(int32 InNumLoopsToPlay)
{
	if (PlayMode == EUMGSequencePlayMode::PingPong)
	{
		NumLoopsToPlay = (2 * InNumLoopsToPlay);
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}
}

void UUMGSequencePlayer::SetPlaybackSpeed(float InPlaybackSpeed)
{
	PlaybackSpeed = InPlaybackSpeed;
}

EMovieScenePlayerStatus::Type UUMGSequencePlayer::GetPlaybackStatus() const
{
	return PlayerStatus;
}

UObject* UUMGSequencePlayer::GetPlaybackContext() const
{
	return UserWidget.Get();
}

TArray<UObject*> UUMGSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (UserWidget.IsValid())
	{
		EventContexts.Add(UserWidget.Get());
	}
	return EventContexts;
}

void UUMGSequencePlayer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlayerStatus = InPlaybackStatus;
}

void UUMGSequencePlayer::ApplyLatentActions()
{
	// Swap to a stack array to ensure no reentrancy if we evaluate during a pause, for instance
	TArray<ELatentAction> TheseActions;
	Swap(TheseActions, LatentActions);

	for (ELatentAction LatentAction : TheseActions)
	{
		switch(LatentAction)
		{
		case ELatentAction::Stop:	Stop(); break;
		case ELatentAction::Pause:	Pause(); break;
		}
	}
}
