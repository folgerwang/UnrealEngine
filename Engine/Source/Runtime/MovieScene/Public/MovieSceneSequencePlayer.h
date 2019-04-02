// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlaybackClient.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeController.h"
#include "MovieSceneSequencePlayer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovieSceneSequencePlayerEvent);

/**
 * Enum used to define how to update to a particular time
 */
UENUM()
enum class EUpdatePositionMethod : uint8
{
	/** Update from the current position to a specified position (including triggering events), using the current player status */
	Play,
	/** Jump to a specified position (without triggering events in between), using the current player status */
	Jump,
	/** Jump to a specified position, temporarily using EMovieScenePlayerStatus::Scrubbing */
	Scrub,
};



/** POD struct that represents a number of loops where -1 signifies infinite looping, 0 means no loops, etc
 * Defined as a struct rather than an int so a property type customization can be bound to it
 */
USTRUCT(BlueprintType)
struct FMovieSceneSequenceLoopCount
{
	FMovieSceneSequenceLoopCount()
		: Value(0)
	{}

	GENERATED_BODY()

	/** Serialize this count from an int */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot );

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(UIMin=1, DisplayName="Loop"))
	int32 Value;
};
template<> struct TStructOpsTypeTraits<FMovieSceneSequenceLoopCount> : public TStructOpsTypeTraitsBase2<FMovieSceneSequenceLoopCount>
{
	enum { WithStructuredSerializeFromMismatchedTag = true };
};



/**
 * Properties that are broadcast from server->clients for time/state synchronization
 */
USTRUCT()
struct FMovieSceneSequenceReplProperties
{
	GENERATED_BODY()

	FMovieSceneSequenceReplProperties()
		: LastKnownStatus(EMovieScenePlayerStatus::Stopped)
		, LastKnownNumLoops(0)
	{}

	/** The last known position of the sequence on the server */
	UPROPERTY()
	FFrameTime LastKnownPosition;

	/** The last known playback status of the sequence on the server */
	UPROPERTY()
	TEnumAsByte<EMovieScenePlayerStatus::Type> LastKnownStatus;

	/** The last known number of loops of the sequence on the server */
	UPROPERTY()
	int32 LastKnownNumLoops;
};



/**
 * Settings for the level sequence player actor.
 */
USTRUCT(BlueprintType)
struct FMovieSceneSequencePlaybackSettings
{
	FMovieSceneSequencePlaybackSettings()
		: bAutoPlay(false)
		, PlayRate(1.f)
		, StartTime(0.f)
		, bRandomStartTime(false)
		, bRestoreState(false)
		, bDisableMovementInput(false)
		, bDisableLookAtInput(false)
		, bHidePlayer(false)
		, bHideHud(false)
		, bDisableCameraCuts(false)
		, bPauseAtEnd(false)
	{ }

	GENERATED_BODY()

	/** Auto-play the sequence when created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playback")
	uint32 bAutoPlay : 1;

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(UIMin=1, DisplayName="Loop"))
	FMovieSceneSequenceLoopCount LoopCount;

	/** The rate at which to playback the animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(Units=Multiplier))
	float PlayRate;

	/** Start playback at the specified offset from the start of the sequence's playback range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", DisplayName="Start Offset", meta=(Units=s, EditCondition="!bRandomStartTime"))
	float StartTime;

	/** Start playback at a random time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	uint32 bRandomStartTime : 1;

	/** Flag used to specify whether actor states should be restored on stop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	uint32 bRestoreState : 1;

	/** Disable Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bDisableMovementInput : 1;

	/** Disable LookAt Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bDisableLookAtInput : 1;

	/** Hide Player Pawn during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bHidePlayer : 1;

	/** Hide HUD during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bHideHud : 1;

	/** Disable camera cuts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bDisableCameraCuts : 1;

	/** Pause the sequence when playback reaches the end rather than stopping it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	uint32 bPauseAtEnd : 1;

	MOVIESCENE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
};

template<> struct TStructOpsTypeTraits<FMovieSceneSequencePlaybackSettings> : public TStructOpsTypeTraitsBase2<FMovieSceneSequencePlaybackSettings>
{
	enum { WithCopy = true, WithStructuredSerializeFromMismatchedTag = true };
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

	/** Stop playback and move the cursor to the end (or start, for reversed playback) of the sequence. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Stop();

	/** Stop playback without moving the cursor. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void StopAtCurrentTime();

	/** Go to end and stop. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (ToolTip = "Go to end of the sequence and stop. Adheres to 'When Finished' section rules."))
	void GoToEndAndStop();

public:

	/**
	 * Get the current playback position
	 * @return The current playback position
	 */
	UE_DEPRECATED(4.20, "Please use GetCurrentTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use GetCurrentTime instead"))
	float GetPlaybackPosition() const { return GetCurrentTime().AsSeconds() - StartTime / PlayPosition.GetInputRate(); }

	/**
	 * Get the playback length of the sequence
	 */
	UE_DEPRECATED(4.20, "Please use GetDuration instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use GetDuration instead"))
	float GetLength() const;

	/**
	 * Get the offset within the level sequence to start playing
	 */
	UE_DEPRECATED(4.20, "Please use GetStartTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use GetStartTime instead"))
	float GetPlaybackStart() const { return StartTime / PlayPosition.GetInputRate(); }

	/**
	 * Get the offset within the level sequence to finish playing
	 */
	UE_DEPRECATED(4.20, "Please use GetEndTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use GetEndTime instead"))
	float GetPlaybackEnd() const { return (StartTime + DurationFrames) / PlayPosition.GetInputRate(); }

	/**
	 * Set the current playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * If the animation is currently playing, it will continue to do so from the new position
	 */
	UE_DEPRECATED(4.20, "Please use PlayToFrame instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use PlayToFrame instead"))
	void SetPlaybackPosition(float NewPlaybackPosition) { Status == EMovieScenePlayerStatus::Playing ? PlayToSeconds(NewPlaybackPosition + StartTime / PlayPosition.GetInputRate()) : JumpToSeconds(NewPlaybackPosition + StartTime / PlayPosition.GetInputRate()); }

	/**
	 * Sets the range in time to be played back by this player, overriding the default range stored in the asset
	 *
	 * @param	NewStartTime	The new starting time for playback
	 * @param	NewEndTime		The new ending time for playback.  Must be larger than the start time.
	 */
	UE_DEPRECATED(4.20, "Please use SetFrameRange or SetTimeRange instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use SetFrameRange or SetTimeRange instead"))
	void SetPlaybackRange( const float NewStartTime, const float NewEndTime );

	/**
	 * Jump to new playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * This can be used to update sequencer repeatedly, as if in a scrubbing state
	 */
	UE_DEPRECATED(4.20, "Please use ScrubToTime instead")
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (DeprecatedFunction, DeprecationMessage = "Please use ScrubToTime instead"))
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
	 * Low-level call to set the current time of the player by evaluating from the current time to the specified time, as if the sequence is playing. 
	 * Triggers events that lie within the evaluated range. Does not alter the persistent playback status of the player (IsPlaying).
	 *
	 * @param NewPosition     The new frame time to play to
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Play To (Frames)")
	void PlayToFrame(FFrameTime NewPosition);

	/**
	 * Low-level call to set the current time of the player by evaluating only the specified time. Will not trigger any events. 
	 * Does not alter the persistent playback status of the player (IsPlaying).
	 *
	 * @param NewPosition     The new frame time to scrub to
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", DisplayName="Scrub To (Frames)")
	void ScrubToFrame(FFrameTime NewPosition);

	/**
	 * Low-level call to set the current time of the player by evaluating only the specified time, as if scrubbing the timeline. Will trigger only events that exist at the specified time. 
	 * Does not alter the persistent playback status of the player (IsPlaying).
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


	/**
	 * Play the sequence from the current time, to the specified marked frame by label
	 *
	 * @param InLabel   The desired marked frame label to play to
	 * @return Whether the marked frame was found
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool PlayToMarkedFrame(const FString& InLabel);

	/**
	 * Scrub the sequence from the current time, to the specified marked frame by label
	 *
	 * @param InLabel   The desired marked frame label to scrub to
	 * @return Whether the marked frame was found
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool ScrubToMarkedFrame(const FString& InLabel);

	/**
	 * Jump to the specified marked frame by label, without evaluating the sequence in between the current and desired time (as if in a paused state)
	 *
	 * @param InLabel   The desired marked frame label to jump to
	 * @return Whether the marked frame was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Game|Cinematic")
	bool JumpToMarkedFrame(const FString& InLabel);

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

	/** Get the object bindings for the requested object */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	TArray<FMovieSceneObjectBindingID> GetObjectBindings(UObject* InObject);

public:

	/** Update the sequence for the current time, if playing */
	void Update(const float DeltaSeconds);

	/** Initialize this player with a sequence and some settings */
	void Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings);

public:

	/**
	 * Access the sequence this player is playing
	 * @return the sequence currently assigned to this player
	 */
	UMovieSceneSequence* GetSequence() const { return Sequence; }

	/**
	 * Assign a playback client interface for this sequence player, defining instance data and binding overrides
	 */
	void SetPlaybackClient(TScriptInterface<IMovieScenePlaybackClient> InPlaybackClient);

	/**
	 * Assign a time controller for this sequence player allowing custom time management implementations.
	 */
	void SetTimeController(TSharedPtr<FMovieSceneTimeController> InTimeController);

protected:

	void PlayInternal();
	void StopInternal(FFrameTime TimeToResetTo);

	virtual void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped = false);

	void UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method);
	bool ShouldStopOrLoop(FFrameTime NewPosition) const;

	UWorld* GetPlaybackWorld() const;

	FFrameTime GetLastValidTime() const;

	int32 FindMarkedFrameByLabel(const FString& InLabel) const;

protected:

	//~ IMovieScenePlayer interface
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootTemplateInstance; }
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() override;
	virtual UObject* AsUObject() override { return this; }

	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
	virtual bool CanUpdateCameraCut() const override { return !PlaybackSettings.bDisableCameraCuts; }
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override {}
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual IMovieScenePlaybackClient* GetPlaybackClient() override { return PlaybackClient ? &*PlaybackClient : nullptr; }

	/*~ Begin UObject interface */
	virtual void BeginDestroy() override;
	virtual bool IsSupportedForNetworking() const { return true; }
	virtual int32 GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	virtual void PostNetReceive() override;
	/*~ End UObject interface */

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

private:

	/**
	 * Called on the server whenever an explicit change in time has occurred through one of the (Play|Jump|Scrub)To methods
	 */
	UFUNCTION(netmulticast, reliable)
	void RPC_ExplicitServerUpdateEvent(EUpdatePositionMethod Method, FFrameTime RelevantTime);

	/**
	 * Called on the server when Stop() is called in order to differentiate Stops from Pauses.
	 */
	UFUNCTION(netmulticast, reliable)
	void RPC_OnStopEvent(FFrameTime StoppedTime);

	/**
	 * Check whether this sequence player is an authority, as determined by its outer Actor
	 */
	bool HasAuthority() const;

	/**
	 * Update the replicated properties required for synchronizing to clients of this sequence player
	 */
	void UpdateNetworkSyncProperties();

protected:

	/** Movie player status. */
	UPROPERTY()
	TEnumAsByte<EMovieScenePlayerStatus::Type> Status;

	/** Whether we're currently playing in reverse. */
	UPROPERTY(replicated)
	uint32 bReversePlayback : 1;

	/** Set to true while evaluating to prevent reentrancy */
	uint32 bIsEvaluating : 1;

	/** The sequence to play back */
	UPROPERTY(transient)
	UMovieSceneSequence* Sequence;

	/** Time (in playback frames) at which to start playing the sequence (defaults to the lower bound of the sequence's play range) */
	UPROPERTY(replicated)
	FFrameNumber StartTime;

	/** Time (in playback frames) at which to stop playing the sequence (defaults to the upper bound of the sequence's play range) */
	UPROPERTY(replicated)
	int32 DurationFrames;

	/** The number of times we have looped in the current playback */
	UPROPERTY(transient)
	int32 CurrentNumLoops;

	struct FLatentAction
	{
		enum EType { Stop, Pause, Update };

		FLatentAction(EType InType, FFrameTime DesiredTime = 0)
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
	UPROPERTY(replicated)
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** The root template instance we're evaluating */
	UPROPERTY(transient)
	FMovieSceneRootEvaluationTemplateInstance RootTemplateInstance;

	/** Play position helper */
	FMovieScenePlaybackPosition PlayPosition;

	TSharedPtr<FMovieSceneSpawnRegister> SpawnRegister;

	/** Replicated playback status and current time that are replicated to clients */
	UPROPERTY(replicated)
	FMovieSceneSequenceReplProperties NetSyncProps;

	/** External client pointer in charge of playing back this sequence */
	UPROPERTY(Transient)
	TScriptInterface<IMovieScenePlaybackClient> PlaybackClient;

	/** (Optional) Externally supplied time controller */
	TSharedPtr<FMovieSceneTimeController> TimeController;

private:

	/** The event that will be broadcast every time the sequence is updated */
	mutable FOnMovieSceneSequencePlayerUpdated OnMovieSceneSequencePlayerUpdate;

	/** The maximum tick rate prior to playing (used for overriding delta time during playback). */
	TOptional<double> OldMaxTickRate;
};
