// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneBindingOverridesInterface.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeController.h"
#include "MovieSceneSequencePlayer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovieSceneSequencePlayerEvent);

/**
 * Enum used to define how to update to a particular time
 */
enum class EUpdatePositionMethod
{
	/** Update from the current position to a specified position (including triggering events), using the current player status */
	Play,
	/** Jump to a specified position (without triggering events in between), using the current player status */
	Jump,
	/** Jump to a specified position, temporarily using EMovieScenePlayerStatus::Scrubbing */
	Scrub,
};

/**
 * Settings for the level sequence player actor.
 */
USTRUCT(BlueprintType)
struct FMovieSceneSequencePlaybackSettings
{
	FMovieSceneSequencePlaybackSettings()
		: LoopCount(0)
		, PlayRate(1.f)
		, bRandomStartTime(false)
		, StartTime(0.f)
		, bRestoreState(false)
		, bDisableMovementInput(false)
		, bDisableLookAtInput(false)
		, bHidePlayer(false)
		, bHideHud(false)
		, bDisableCameraCuts(false)
		, bPauseAtEnd(false)
		, InstanceData(nullptr)
	{ }

	GENERATED_BODY()

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(UIMin=1, DisplayName="Loop"))
	int32 LoopCount;

	/** The rate at which to playback the animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(Units=Multiplier))
	float PlayRate;

	/** Start playback at a random time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	bool bRandomStartTime;

	/** Start playback at the specified offset from the start of the sequence's playback range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", DisplayName="Start Offset", meta=(Units=s))
	float StartTime;

	/** Flag used to specify whether actor states should be restored on stop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	bool bRestoreState;

	/** Disable Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bDisableMovementInput;

	/** Disable LookAt Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bDisableLookAtInput;

	/** Hide Player Pawn during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bHidePlayer;

	/** Hide HUD during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bHideHud;

	/** Disable camera cuts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bDisableCameraCuts;

	/** Pause the sequence when playback reaches the end rather than stopping it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bPauseAtEnd;

	/** An object that can implement specific instance overrides for the sequence */
	UPROPERTY(BlueprintReadWrite, Category="Cinematic")
	UObject* InstanceData;

	/** Interface that defines overridden bindings for this sequence */
	UPROPERTY()
	TScriptInterface<IMovieSceneBindingOverridesInterface> BindingOverrides;

	/** (Optional) Externally supplied time controller */
	TSharedPtr<FMovieSceneTimeController> TimeController;

	MOVIESCENE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);
};

template<> struct TStructOpsTypeTraits<FMovieSceneSequencePlaybackSettings> : public TStructOpsTypeTraitsBase2<FMovieSceneSequencePlaybackSettings>
{
	enum { WithCopy = true, WithSerializeFromMismatchedTag = true };
};

/**
 * Abstract class that provides consistent player behaviour for various animation players
 */
UCLASS(Abstract, BlueprintType)
class MOVIESCENE_API UMovieSceneSequencePlayer
	: public UObject
	, public IMovieScenePlayer
{
public:
	GENERATED_BODY()

	UMovieSceneSequencePlayer(const FObjectInitializer&);
	virtual ~UMovieSceneSequencePlayer();

	/** Start playback forwards from the current time cursor position, using the current play rate. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Play();

	/** Reverse playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void PlayReverse();

	/** Changes the direction of playback (go in reverse if it was going forward, or vice versa) */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void ChangePlaybackDirection();

	/**
	 * Start playback from the current time cursor position, looping the specified number of times.
	 * @param NumLoops - The number of loops to play. -1 indicates infinite looping.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void PlayLooping(int32 NumLoops = -1);
	
	/** Pause playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Pause();
	
	/** Scrub playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Scrub();

	/** Stop playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Stop();

	/** Go to end and stop. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (ToolTip = "Go to end of the sequence and stop. Adheres to 'When Finished' section rules."))
	void GoToEndAndStop();

public:

	/**
	 * Get the current playback position
	 * @return The current playback position
	 */
	DEPRECATED(4.20, "Please use GetCurrentTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackPosition() const { return GetCurrentTime().AsSeconds() - StartTime / PlayPosition.GetInputRate(); }

	/**
	 * Get the playback length of the sequence
	 */
	DEPRECATED(4.20, "Please use GetDuration instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetLength() const;

	/**
	 * Get the offset within the level sequence to start playing
	 */
	DEPRECATED(4.20, "Please use GetStartTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackStart() const { return StartTime / PlayPosition.GetInputRate(); }

	/**
	 * Get the offset within the level sequence to finish playing
	 */
	DEPRECATED(4.20, "Please use GetEndTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackEnd() const { return (StartTime + DurationFrames) / PlayPosition.GetInputRate(); }

	/**
	 * Set the current playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * If the animation is currently playing, it will continue to do so from the new position
	 */
	DEPRECATED(4.20, "Please use PlayToFrame instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlaybackPosition(float NewPlaybackPosition) { Status == EMovieScenePlayerStatus::Playing ? PlayToSeconds(NewPlaybackPosition + StartTime / PlayPosition.GetInputRate()) : JumpToSeconds(NewPlaybackPosition + StartTime / PlayPosition.GetInputRate()); }

	/**
	 * Sets the range in time to be played back by this player, overriding the default range stored in the asset
	 *
	 * @param	NewStartTime	The new starting time for playback
	 * @param	NewEndTime		The new ending time for playback.  Must be larger than the start time.
	 */
	DEPRECATED(4.20, "Please use SetFrameRange or SetTimeRange instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlaybackRange( const float NewStartTime, const float NewEndTime );

	/**
	 * Jump to new playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * This can be used to update sequencer repeatedly, as if in a scrubbing state
	 */
	DEPRECATED(4.20, "Please use ScrubToTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void JumpToPosition(float NewPlaybackPosition) { ScrubToSeconds(NewPlaybackPosition); }

public:

	/**
	 * Get the current playback position
	 * @return The current playback position
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	FQualifiedFrameTime GetCurrentTime() const;

	/**
	 * Get the total duration of the sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	FQualifiedFrameTime GetDuration() const;

	/**
	 * Get this sequence's duration in frames
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	int32 GetFrameDuration() const;

	/**
	 * Get this sequence's display rate.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	FFrameRate GetFrameRate() const { return PlayPosition.GetInputRate(); }

	/**
	 * Set the frame-rate that this player should play with, making all frame numbers in the specified time-space
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetFrameRate(FFrameRate FrameRate);

	/**
	 * Get the offset within the level sequence to start playing
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	FQualifiedFrameTime GetStartTime() const { return FQualifiedFrameTime(StartTime, PlayPosition.GetInputRate()); }

	/**
	 * Get the offset within the level sequence to finish playing
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	FQualifiedFrameTime GetEndTime() const { return FQualifiedFrameTime(StartTime + DurationFrames, PlayPosition.GetInputRate()); }

public:

	/**
	 * Set the valid play range for this sequence, determined by a starting frame number (in this sequence player's plaback frame), and a number of frames duration
	 *
	 * @param StartFrame      The frame number to start playing back the sequence
	 * @param Duration        The number of frames to play
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Set Play Range (Frames)")
	void SetFrameRange( int32 StartFrame, int32 Duration );

	/**
	 * Set the valid play range for this sequence, determined by a starting time  and a duration (in seconds)
	 *
	 * @param StartTime       The time to start playing back the sequence in seconds
	 * @param Duration        The length to play for
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Set Play Range (Seconds)")
	void SetTimeRange( float StartTime, float Duration );

public:

	/**
	 * Play the sequence from the current time, to the specified frame position
	 *
	 * @param NewPosition     The new frame time to play to
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Play To (Frames)")
	void PlayToFrame(FFrameTime NewPosition);

	/**
	 * Scrub the sequence from the current time, to the specified frame position
	 *
	 * @param NewPosition     The new frame time to scrub to
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Scrub To (Frames)")
	void ScrubToFrame(FFrameTime NewPosition);

	/**
	 * Jump to the specified frame position, without evaluating the sequence in between the current and desired time (as if in a paused state)
	 *
	 * @param NewPosition     The new frame time to jump to
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Jump To (Frames)")
	void JumpToFrame(FFrameTime NewPosition);


	/**
	 * Play the sequence from the current time, to the specified time in seconds
	 *
	 * @param TimeInSeconds   The desired time in seconds
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Play To (Seconds)")
	void PlayToSeconds(float TimeInSeconds);

	/**
	 * Scrub the sequence from the current time, to the specified time in seconds
	 *
	 * @param TimeInSeconds   The desired time in seconds
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Scrub To (Seconds)")
	void ScrubToSeconds(float TimeInSeconds);

	/**
	 * Jump to the specified time in seconds, without evaluating the sequence in between the current and desired time (as if in a paused state)
	 *
	 * @param TimeInSeconds   The desired time in seconds
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Jump To (Seconds)")
	void JumpToSeconds(float TimeInSeconds);

public:

	/** Check whether the sequence is actively playing. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool IsPlaying() const;

	/** Check whether the sequence is paused. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool IsPaused() const;

	/** Check whether playback is reversed. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool IsReversed() const;

	/** Get the playback rate of this player. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlayRate() const;

	/**
	 * Set the playback rate of this player. Negative values will play the animation in reverse.
	 * @param PlayRate - The new rate of playback for the animation.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlayRate(float PlayRate);

	/** Set whether to disable camera cuts */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetDisableCameraCuts(bool bInDisableCameraCuts) { PlaybackSettings.bDisableCameraCuts = bInDisableCameraCuts; }

	/** Set whether to disable camera cuts */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool GetDisableCameraCuts() { return PlaybackSettings.bDisableCameraCuts; }

	/** An event that is broadcast each time this level sequence player is updated */
	DECLARE_EVENT_ThreeParams( UMovieSceneSequencePlayer, FOnMovieSceneSequencePlayerUpdated, const UMovieSceneSequencePlayer&, FFrameTime /*current time*/, FFrameTime /*previous time*/ );
	FOnMovieSceneSequencePlayerUpdated& OnSequenceUpdated() const { return OnMovieSceneSequencePlayerUpdate; }

	/** Event triggered when the level sequence player is played */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnPlay;

	/** Event triggered when the level sequence player is played in reverse */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnPlayReverse;

	/** Event triggered when the level sequence player is stopped */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnStop;

	/** Event triggered when the level sequence player is paused */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnPause;

	/** Event triggered when the level sequence player finishes naturally (without explicitly calling stop) */
	UPROPERTY(BlueprintAssignable, Category = "Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnFinished;


public:

	/** Retrieve all objects currently bound to the specified binding identifier */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	TArray<UObject*> GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding);

public:

	/** Update the sequence for the current time, if playing */
	void Update(const float DeltaSeconds);

	/** Initialize this player with a sequence and some settings */
	void Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings);

	/** Begin play called */
	virtual void BeginPlay() {}

public:

	/**
	 * Access the sequence this player is playing
	 * @return the sequence currently assigned to this player
	 */
	UMovieSceneSequence* GetSequence() const { return Sequence; }

protected:

	void PlayInternal();

	void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped = false);

	void UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method);
	bool ShouldStopOrLoop(FFrameTime NewPosition) const;

	UWorld* GetPlaybackWorld() const;

	FFrameTime GetLastValidTime() const;

protected:

	//~ IMovieScenePlayer interface
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootTemplateInstance; }
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() override;

	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
	virtual bool CanUpdateCameraCut() const override { return !PlaybackSettings.bDisableCameraCuts; }
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override {}
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual const IMovieSceneBindingOverridesInterface* GetBindingOverrides() const override { return PlaybackSettings.BindingOverrides ? &*PlaybackSettings.BindingOverrides : nullptr; }
	virtual const UObject* GetInstanceData() const override { return PlaybackSettings.InstanceData; }

	//~ UObject interface
	virtual void BeginDestroy() override;

protected:

	virtual bool CanPlay() const { return true; }
	virtual void OnStartedPlaying() {}
	virtual void OnLooped() {}
	virtual void OnPaused() {}
	virtual void OnStopped() {}
	
private:

	/** Apply any latent actions which may have accumulated while the sequence was being evaluated */
	void ApplyLatentActions();

	void UpdateTimeCursorPosition_Internal(FFrameTime NewPosition, EUpdatePositionMethod Method);

protected:

	/** Movie player status. */
	UPROPERTY()
	TEnumAsByte<EMovieScenePlayerStatus::Type> Status;

	/** Whether we're currently playing in reverse. */
	UPROPERTY()
	uint32 bReversePlayback : 1;

	/** Set to true while evaluating to prevent reentrancy */
	bool bIsEvaluating : 1;

	/** The sequence to play back */
	UPROPERTY(transient)
	UMovieSceneSequence* Sequence;

	/** Time (in playback frames) at which to start playing the sequence (defaults to the lower bound of the sequence's play range) */
	UPROPERTY()
	FFrameNumber StartTime;

	/** Time (in playback frames) at which to stop playing the sequence (defaults to the upper bound of the sequence's play range) */
	UPROPERTY()
	int32 DurationFrames;

	/** The number of times we have looped in the current playback */
	UPROPERTY(transient)
	int32 CurrentNumLoops;

	struct FLatentAction
	{
		enum EType { Stop, Pause, Update };

		FLatentAction(EType InType)
			: Type(InType)
		{}

		FLatentAction(EUpdatePositionMethod InUpdateMethod, FFrameTime DesiredTime)
			: Type(Update), UpdateMethod(InUpdateMethod), Position(DesiredTime)
		{}

		EType                 Type;
		EUpdatePositionMethod UpdateMethod;
		FFrameTime            Position;
	};

	/** Set of latent actions that are to be performed when the sequence has finished evaluating this frame */
	TArray<FLatentAction> LatentActions;

	/** Specific playback settings for the animation. */
	UPROPERTY()
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** The root template instance we're evaluating */
	FMovieSceneRootEvaluationTemplateInstance RootTemplateInstance;

	/** Play position helper */
	FMovieScenePlaybackPosition PlayPosition;

	TSharedPtr<FMovieSceneSpawnRegister> SpawnRegister;

private:

	/** The event that will be broadcast every time the sequence is updated */
	mutable FOnMovieSceneSequencePlayerUpdated OnMovieSceneSequencePlayerUpdate;

	/** The maximum tick rate prior to playing (used for overriding delta time during playback). */
	TOptional<double> OldMaxTickRate;
};
