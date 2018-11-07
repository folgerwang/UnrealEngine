// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimecodeProvider.h"

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "TimeSynchronizationSource.h"

#include "Misc/QualifiedFrameTime.h"

#include "TimecodeSynchronizer.generated.h"

class UFixedFrameRateCustomTimeStep;

/**
 * Defines the various modes that the synchronizer can use to try and achieve synchronization.
 */
UENUM()
enum class ETimecodeSynchronizationSyncMode
{
	/**
	 * User will specify an offset (number of frames) from the Timecode Source (see ETimecodeSycnrhonizationTimecodeType).
	 * This offset may be positive or negative depending on the latency of the source.
	 * Synchronization will be achieved once the synchronizer detects all input sources have frames that correspond
	 * with the offset timecode.
	 *
	 * This is suitable for applications trying to keep multiple UE4 instances in sync while using nDisplay / genlock.
	 */
	UserDefinedOffset,

	/**
	 * Engine will try and automatically determine an appropriate offset based on what frames are available
	 * on the given sources.
	 *
	 * This is suitable for running a single UE4 instance that just wants to synchronize its inputs.
	 */
	Auto,

	/**
	 * The same as Auto except that instead of trying to find a suitable timecode nearest to the
	 * newest common frame, we try to find a suitable timecode nearest to the oldest common frame.
	 */
	AutoOldest,
};

/**
 * Enumerates Timecode source type.
 */
UENUM()
enum class ETimecodeSynchronizationTimecodeType
{
	/** Use an external Timecode Provider to provide the timecode to follow. */
	TimecodeProvider,

	/** Use one of the InputSource as the Timecode Provider. */
	InputSource,
};

/**
 * Enumerates Synchronization related events.
 */
enum class ETimecodeSynchronizationEvent
{
	/** The synchronization procedure has started. */
	SynchronizationStarted,

	/** The synchronization procedure failed. */
	SynchronizationFailed,

	/** The synchronization procedure succeeded. */
	SynchronizationSucceeded,
};

/** Cached values to use during synchronization / while synchronized */
struct FTimecodeSynchronizerCachedSyncState
{
	/** If we're using rollover, the frame time that represents the rollover point (e.g., the modulus). */
	TOptional<FFrameTime> RolloverFrame;

	/** The FrameRate of the synchronizer. */
	FFrameRate FrameRate;

	/** Synchronization mode that's being used. */
	ETimecodeSynchronizationSyncMode SyncMode;

	/** Frame offset that will be used if SyncMode != Auto; */
	int32 FrameOffset;
};

/** Cached frame values for a given source. */
struct FTimecodeSourceState
{
	/** Frame time of the newest available sample. */
	FFrameTime NewestAvailableSample;

	/** Frame time of the oldest available sample. */
	FFrameTime OldestAvailableSample;
};

/**
 * Provides a wrapper around a UTimeSynchronizerSource, and caches data necessary
 * to provide synchronization.
 *
 * The values are typically updated once per frame.
 */
USTRUCT()
struct FTimecodeSynchronizerActiveTimecodedInputSource
{
	GENERATED_BODY()

public:

	FTimecodeSynchronizerActiveTimecodedInputSource()
		: bIsReady(false)
		, bCanBeSynchronized(false)
		, TotalNumberOfSamples(0)
		, FrameRate(60, 1)
		, InputSource(nullptr)
	{
	}

	FTimecodeSynchronizerActiveTimecodedInputSource(UTimeSynchronizationSource* Source)
		: bIsReady(false)
		, bCanBeSynchronized(Source->bUseForSynchronization)
		, TotalNumberOfSamples(0)
		, FrameRate(60, 1)
		, InputSource(Source)
	{
	}

	/** Updates the internal state of this source, returning whether or not the source is ready (e.g. IsReady() == true). */
	const bool UpdateSourceState(const FFrameRate& SynchronizerFrameRate);

	FORCEINLINE const UTimeSynchronizationSource* GetInputSource() const
	{
		return InputSource;
	}

	FORCEINLINE bool IsInputSourceValid() const
	{
		return nullptr != InputSource;
	}

	FORCEINLINE FString GetDisplayName() const
	{
		return InputSource->GetDisplayName();
	}

	/** Whether or not this source is ready. */
	FORCEINLINE bool IsReady() const
	{
		return bIsReady;
	}

	/** Whether or not this source can be synchronized. */
	FORCEINLINE bool CanBeSynchronized() const
	{
		return bCanBeSynchronized;
	}

	/** Gets the FrameRate of the source. */
	FORCEINLINE const FFrameRate& GetFrameRate() const
	{
		return FrameRate;
	}

	/** Gets the state of the Source relative to its own frame rate. */
	FORCEINLINE const FTimecodeSourceState& GetInputSourceState() const
	{
		return InputSourceState;
	}

	/** Gets the state of the Source relative to the Synchronizer's frame rate. */
	FORCEINLINE const FTimecodeSourceState& GetSynchronizerRelativeState() const
	{
		return SynchronizerRelativeState;
	}

private:

	/* Flag stating if the source is ready */
	UPROPERTY(VisibleAnywhere, Transient, Category=Debug, Meta=(DisplayName = "Is Ready"))
	bool bIsReady;

	/* Flag stating if this source can be synchronized */
	UPROPERTY(VisibleAnywhere, Transient, Category=Debug, Meta=(DisplayName = "Can Be Synchronized"))
	bool bCanBeSynchronized;

	UPROPERTY(VisibleAnywhere, Transient, Category=Debug)
	int32 TotalNumberOfSamples;

	FFrameRate FrameRate;

	FTimecodeSourceState InputSourceState;
	FTimecodeSourceState SynchronizerRelativeState;

	/* Associated source pointers */
	UPROPERTY(VisibleAnywhere, Transient, Category = Debug, Meta=(DisplayName="Input Source"))
	UTimeSynchronizationSource* InputSource;

	FTimecodeSynchronizerActiveTimecodedInputSource(const FTimecodeSynchronizerActiveTimecodedInputSource&) = delete;
	FTimecodeSynchronizerActiveTimecodedInputSource& operator=(const FTimecodeSynchronizerActiveTimecodedInputSource&) = delete;
	FTimecodeSynchronizerActiveTimecodedInputSource(FTimecodeSynchronizerActiveTimecodedInputSource&&) = delete;
	FTimecodeSynchronizerActiveTimecodedInputSource& operator=(FTimecodeSynchronizerActiveTimecodedInputSource&&) = delete;
};

template<>
struct TStructOpsTypeTraits<FTimecodeSynchronizerActiveTimecodedInputSource> : public TStructOpsTypeTraitsBase2<FTimecodeSynchronizerActiveTimecodedInputSource>
{
	enum
	{
		WithCopy = false
	};
};

/**
 * Timecode Synchronizer is intended to correlate multiple timecode sources to help ensure
 * that all sources can produce data that is frame aligned.
 *
 * This typically works by having sources buffer data until we have enough frames that
 * such that we can find an overlap. Once that process is finished, the Synchronizer will
 * provide the appropriate timecode to the engine (which can be retrieved via FApp::GetTimecode
 * and FApp::GetTimecodeFrameRate).
 *
 * Note, the Synchronizer doesn't perform any buffering of data itself (that is left up to
 * TimeSynchronizationSources). Instead, the synchronizer simply acts as a coordinator
 * making sure all sources are ready, determining if sync is possible, etc.
 */
UCLASS()
class TIMECODESYNCHRONIZER_API UTimecodeSynchronizer : public UTimecodeProvider
{
	GENERATED_BODY()

public:

	UTimecodeSynchronizer();

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin TimecodeProvider Interface
	virtual FTimecode GetTimecode() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual ETimecodeProviderSynchronizationState GetSynchronizationState() const override;
	virtual bool Initialize(class UEngine* InEngine) override { return true; }
	virtual void Shutdown(class UEngine* InEngine) override {}
	//~ End TimecodeProvider Interface

public:

	/**
	 * Starts the synchronization process. Does nothing if we're already synchronized, or attempting to synchronize.
	 *
	 * @return True if the synchronization process was successfully started (or was previously started).
	 */
	bool StartSynchronization();

	/** Stops the synchronization process. Does nothing if we're not synchronized, or attempting to synchronize. */
	void StopSynchronization();

	DEPRECATED(4.21, "Please use GetSynchronizedSources.")
	const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& GetTimecodedSources() const { return GetSynchronizedSources(); }

	DEPRECATED(4.21, "Please use GetNonSynchronizedSources.")
	const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& GetSynchronizationSources() const { return GetNonSynchronizedSources(); }

	/** Returns the list of sources that are used to perform synchronization. */
	const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& GetSynchronizedSources() const { return SynchronizedSources; }

	/** Returns the list of sources that are not actively being used in synchronization. */
	const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& GetNonSynchronizedSources() const { return NonSynchronizedSources; }
	
	/** Returns the index of the Master Synchronization Source in the Synchronized Sources list. */
	int32 GetActiveMasterSynchronizationTimecodedSourceIndex() const { return MasterSynchronizationSourceIndex; }

	/**
	 * Get an event delegate that is invoked when a Asset synchronization event occurred.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UTimecodeSynchronizer, FOnTimecodeSynchronizationEvent, ETimecodeSynchronizationEvent /*Event*/)
	FOnTimecodeSynchronizationEvent& OnSynchronizationEvent()
	{
		return SynchronizationEvent;
	}

private:

	/** Synchronization states */
	enum class ESynchronizationState : uint8
	{
		None,
		Error,
		Initializing,								// Kicking off the initialization process.
		PreRolling_WaitGenlockTimecodeProvider,		// wait for the TimecodeProvider & CustomTimeStep to be Ready
		PreRolling_WaitReadiness,					// wait for all source to be Ready
		PreRolling_Synchronizing,					// wait and find a valid Timecode to start with
		Synchronized,								// all sources are running and synchronized
	};

	FORCEINLINE static FString SynchronizationStateToString(ESynchronizationState InState)
	{
		switch (InState)
		{
		case ESynchronizationState::None:
			return FString(TEXT("None"));

		case ESynchronizationState::Initializing:
			return FString(TEXT("Initializing"));

		case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
			return FString(TEXT("WaitGenlockTimecodeProvider"));

		case ESynchronizationState::PreRolling_WaitReadiness:
			return FString(TEXT("WaitReadiness"));

		case ESynchronizationState::PreRolling_Synchronizing:
			return FString(TEXT("Synchronizing"));

		case ESynchronizationState::Synchronized:
			return FString(TEXT("Synchronized"));

		case ESynchronizationState::Error:
			return FString(TEXT("Error"));

		default:
			return FString::Printf(TEXT("Invalid State %d"), static_cast<int32>(InState));
		}
	}

	/** Registers asset to MediaModule tick */
	void SetTickEnabled(bool bEnabled);

	/** Tick method of the asset */
	void Tick();

	/** Switches on current state and ticks it */
	void Tick_Switch();

	bool ShouldTick();

	/** Test if the genlock & timecode provider are properly setup */
	bool Tick_TestGenlock();
	bool Tick_TestTimecode();

	/** Process PreRolling_WaitGenlockTimecodeProvider state. */
	void TickPreRolling_WaitGenlockTimecodeProvider();

	/** Process PreRolling_WaitReadiness state. */
	void TickPreRolling_WaitReadiness();
	
	/** Process PreRolling_Synchronizing state. */
	void TickPreRolling_Synchronizing();

	/** Process Synchronized state. */
	void Tick_Synchronized();

	/** Register TimecodeSynchronizer as the TimecodeProvider */
	void Register();
	
	/** Unregister TimecodeSynchronizer as the TimecodeProvider */
	void Unregister();

	void OpenSources();
	void StartSources();
	void CloseSources();

	/** Updates and caches the state of the sources. */
	void UpdateSourceStates();
	FFrameTime CalculateSyncTime();

	bool IsSynchronizing() const;
	bool IsSynchronized() const;
	bool IsError() const;

	/** Changes internal state and execute it if required */
	void SwitchState(const ESynchronizationState NewState);

	FFrameTime GetProviderFrameTime() const;

public:
	/** The fixed framerate to use. */
	UPROPERTY(EditAnywhere, Category="Genlock", Meta=(DisplayName="Enable"))
	bool bUseCustomTimeStep;

	/** Custom strategy to tick in a interval. */
	UPROPERTY(EditAnywhere, Instanced, Category="Genlock", Meta=(EditCondition="bUseCustomTimeStep", DisplayName="Genlock Source"))
	UFixedFrameRateCustomTimeStep* CustomTimeStep;

	/** The fixed framerate to use. */
	UPROPERTY(EditAnywhere, Category="Genlock", Meta=(EditCondition="!bUseCustomTimeStep", ClampMin="15.0"))
	FFrameRate FixedFrameRate;

public:
	/** Use a Timecode Provider. */
	UPROPERTY(EditAnywhere, Category="Timecode Provider", Meta=(DisplayName="Select"))
	ETimecodeSynchronizationTimecodeType TimecodeProviderType;

	/** Custom strategy to tick in a interval. */
	UPROPERTY(EditAnywhere, Instanced, Category="Timecode Provider", Meta=(EditCondition="IN_CPP", DisplayName="Timecode Source"))
	UTimecodeProvider* TimecodeProvider;

	/**
	 * Index of the source that drives the synchronized Timecode.
	 * The source need to be timecoded and flag as bUseForSynchronization
	 */
	UPROPERTY(EditAnywhere, Category="Timecode Provider", Meta=(EditCondition="IN_CPP"))
	int32 MasterSynchronizationSourceIndex;

public:
	/** Enable verification of margin between synchronized time and source time */
	UPROPERTY()
	bool bUsePreRollingTimecodeMarginOfErrors;

	/** Maximum gap size between synchronized time and source time */
	UPROPERTY(EditAnywhere, Category="Synchronization", Meta=(EditCondition="bUsePreRollingTimecodeMarginOfErrors", ClampMin="0"))
	int32 PreRollingTimecodeMarginOfErrors;

	/** Enable PreRoll timeout */
	UPROPERTY()
	bool bUsePreRollingTimeout;

	/** How long to wait for all source to be ready */
	UPROPERTY(EditAnywhere, Category="Synchronization", Meta=(EditCondition="bUsePreRollingTimeout", ClampMin="0.0"))
	float PreRollingTimeout; 

public:

	UPROPERTY(EditAnywhere, Instanced, Category="Input")
	TArray<UTimeSynchronizationSource*> TimeSynchronizationInputSources;

private:

	/** What mode will be used for synchronization. */
	UPROPERTY(EditAnywhere, Category = "Synchronization", Meta=(EditCondition="IN_CPP"))
	ETimecodeSynchronizationSyncMode SyncMode;

	/**
	 * When UserDefined mode is used, the number of frames delayed from the Provider's timecode.
	 * Negative values indicate the used timecode will be ahead of the Provider's.
	 */
	UPROPERTY(EditAnywhere, Category = "Synchronization", Meta=(EditCondition = "IN_CPP", ClampMin="-640", ClampMax="640"))
	int32 FrameOffset;

	/**
	 * Similar to FrameOffset.
	 * For Auto mode, this represents the number of frames behind the newest synced frame.
	 * For AutoModeOldest, the is the of frames ahead of the last synced frame.
	 */
	UPROPERTY(EditAnywhere, Category = "Synchronization", Meta = (EditCondition = "IN_CPP", ClampMin = "0", ClampMax = "640"))
	int32 AutoFrameOffset = 3;

	/** Whether or not the specified Provider's timecode rolls over. (Rollover is expected to occur at Timecode 24:00:00:00). */
	UPROPERTY(EditAnywhere, Category = "Synchronization", Meta=(EditCondition="IN_CPP"))
	bool bWithRollover = false;

	/** Sources used for synchronization */
	UPROPERTY(Transient, DuplicateTransient, VisibleAnywhere, Category=Debug)
	TArray<FTimecodeSynchronizerActiveTimecodedInputSource> SynchronizedSources;

	/* Sources that wants to be synchronized */
	UPROPERTY(Transient, DuplicateTransient, VisibleAnywhere, Category=Debug)
	TArray<FTimecodeSynchronizerActiveTimecodedInputSource> NonSynchronizedSources;

	UPROPERTY(Transient)
	UFixedFrameRateCustomTimeStep* RegisteredCustomTimeStep;

	UPROPERTY(Transient)
	const UTimecodeProvider* RegisteredTimecodeProvider;

	UPROPERTY(Transient, DuplicateTransient, VisibleAnywhere, Category = "Synchronization")
	int32 ActualFrameOffset;

private:

	int64 LastUpdatedSources = 0;

	/** The actual synchronization state */
	ESynchronizationState State;
	
	/** Frame time that we'll use for the system */
	TOptional<FFrameTime> CurrentSystemFrameTime;

	/** The current frame from our specified provider. */
	FFrameTime CurrentProviderFrameTime;
	
	/** Timestamp when PreRolling has started */
	double StartPreRollingTime;
	
	/** Whether or not we are registered as the TimecodeProvider */
	bool bRegistered;
	float PreviousFixedFrameRate;
	bool bPreviousUseFixedFrameRate;
	
	/** Index of the active source that drives the synchronized Timecode*/
	int32 ActiveMasterSynchronizationTimecodedSourceIndex;

	/** An event delegate that is invoked when a synchronization event occurred. */
	FOnTimecodeSynchronizationEvent SynchronizationEvent;

	FTimecodeSynchronizerCachedSyncState CachedSyncState;
};
