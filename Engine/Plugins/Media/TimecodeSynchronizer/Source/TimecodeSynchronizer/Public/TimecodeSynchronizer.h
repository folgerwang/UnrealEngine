// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TimecodeProvider.h"

#include "Delegates/Delegate.h"
#include "TimeSynchronizationSource.h"

#include "TimecodeSynchronizer.generated.h"


class UFixedFrameRateCustomTimeStep;


USTRUCT()
struct FTimecodeSynchronizerActiveTimecodedInputSource
{
	GENERATED_BODY()

	FTimecodeSynchronizerActiveTimecodedInputSource()
		: InputSource(nullptr)
		, bIsReady(false)
		, bCanBeSynchronized(false)
		, NextSampleTime(0)
		, AvailableSampleCount(0)
		, FrameRate(30, 1)
		, NextSampleLocalTime(0)
		, MaxSampleLocalTime(0)
	{}

	/* Associated source pointers */
	UPROPERTY(VisibleAnywhere, Transient, Category=Debug)
	UTimeSynchronizationSource* InputSource;
	
	/* Flag stating if the source is ready */
	UPROPERTY(VisibleAnywhere, Transient, Category=Debug)
	bool bIsReady;
	
	/* Flag stating if this source can be synchronized */
	UPROPERTY(VisibleAnywhere, Transient, Category=Debug)
	bool bCanBeSynchronized;

	/* Next sample FrameTime */
	FFrameTime NextSampleTime;
	
	/* Available sample count in the source */
	int32 AvailableSampleCount;
	
	/* The source FrameRate */
	FFrameRate FrameRate;

	/* Local Time of the next sample of the source */
	FFrameTime NextSampleLocalTime;
	
	/* Local Time of the maximum sample of the source */
	FFrameTime MaxSampleLocalTime;

	/* Convert source Timecode to FrameTime based on TimecodeSynchronizer FrameRate */
	void ConvertToLocalFrameRate(const FFrameRate& InLocalFrameRate);
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

/**
 * 
 */
UCLASS()
class TIMECODESYNCHRONIZER_API UTimecodeSynchronizer : public UTimecodeProvider
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

public:
	/** Start, or stop if already started. preroll mechanism*/
	bool StartPreRoll();
	
	/** Stops sources and resets internals */
	void StopInputSources();

	/** Gets the sources used for synchronization */
	const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& GetTimecodedSources() const { return ActiveTimecodedInputSources; }

	/** Gets the sources that want to be synchronized */
	const TArray<FTimecodeSynchronizerActiveTimecodedInputSource>& GetSynchronizationSources() const { return ActiveSynchronizedSources; }

	/** Gets the master synchronization source index */
	int32 GetActiveMasterSynchronizationTimecodedSourceIndex() const { return ActiveMasterSynchronizationTimecodedSourceIndex; }

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

public:
	//~ Begin TimecodeProvider Interface
	virtual FTimecode GetTimecode() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual ETimecodeProviderSynchronizationState GetSynchronizationState() const override;
	virtual bool Initialize(class UEngine* InEngine) override { return true; }
	virtual void Shutdown(class UEngine* InEngine) override {}
	//~ End TimecodeProvider Interface

private:
	
	/** Synchronization states */
	enum class ESynchronizationState : uint8
	{
		None,
		PreRolling_WaitReadiness,	// wait for all source to be bReady
		PreRolling_Synchronizing,	// wait and find a valid Timecode to start with
		PreRolling_Buffering,		// make sure each source have a big enough buffer
		Synchronized,				// all sources are running and synchronized
		Rolling,
		Error,
	};

	/** Is the TimecodeSynchronizer synchronizing */
	bool IsSynchronizing() const;

	/** Is the TimecodeSynchronizer synchronized */
	bool IsSynchronized() const;

	/** Registers asset to MediaModule tick */
	void SetTickEnabled(bool bEnabled);
	
	/** Changes internal state and execute it if required */
	void SwitchState(const ESynchronizationState NewState, const bool bDoTick = false);

	/** Tick method of the asset */
	void Tick();

	/** Switches on current state and ticks it */
	void Tick_Switch();
	
	/** Process PreRolling_WaitReadiness state */
	void TickPreRolling_WaitReadiness();
	
	/** Process PreRolling_Synchronizing state */
	void TickPreRolling_Synchronizing();
	
	/** Process PreRolling_Buffering state */
	void TickPreRolling_Buffering();
	
	/** Process Synchronized state */
	void TickSynchronized();
	
	/** Prepare for ErrorState to be engaged */
	void EnterStateError();
	
	/** Process Error state */
	void TickError();

	/** Register TimecodeSynchronizer as the TimecodeProvider */
	void Register();
	
	/** Unregister TimecodeSynchronizer as the TimecodeProvider */
	void Unregister();

	/** Unregister TimecodeSynchronizer as the TimecodeProvider */
	void SetCurrentFrameTime(const FFrameTime& InNewTime);

	/* Verify if all sources are ready */
	bool AreSourcesReady() const;
	
	/* Start all sources once we're ready to advance time */
	void StartSources();


public:
	/** The fixed framerate to use. */
	UPROPERTY(EditAnywhere, Category="Genlock", meta=(DisplayName="Enable"))
	bool bUseCustomTimeStep;

	/** Custom strategy to tick in a interval. */
	UPROPERTY(EditAnywhere, Instanced, Category="Genlock", meta=(EditCondition="bUseCustomTimeStep", DisplayName="Genlock Source"))
	class UFixedFrameRateCustomTimeStep* CustomTimeStep;

	/** The fixed framerate to use. */
	UPROPERTY(EditAnywhere, Category="Genlock", meta=(EditCondition="!bUseCustomTimeStep", ClampMin="15.0"))
	FFrameRate FixedFrameRate;

	/** Enable verification of margin between synchronized time and source time */
	UPROPERTY()
	bool bUsePreRollingTimecodeMarginOfErrors;

	/** Maximum gap size between synchronized time and source time */
	UPROPERTY(EditAnywhere, Category="Synchronization", meta=(EditCondition="bUsePreRollingTimecodeMarginOfErrors", ClampMin="0"))
	int32 PreRollingTimecodeMarginOfErrors;

	/** Enable PreRoll timeout */
	UPROPERTY()
	bool bUsePreRollingTimeout;

	/** How long to wait for all source to be ready */
	UPROPERTY(EditAnywhere, Category=Synchronization, meta=(EditCondition="bUsePreRollingTimeout", ClampMin="0.0"))
	float PreRollingTimeout; 

	UPROPERTY()
	bool bUseMasterSynchronizationSource;

	/**
	 * Index of the source that drives the synchronized Timecode.
	 * The source need to be timecoded and flag as bUseForSynchronization
	 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(EditCondition="bUseMasterSynchronizationSource"))
	int32 MasterSynchronizationSourceIndex;

	/** Array of all the sources that wants to be synchronized*/
	UPROPERTY(EditAnywhere, Instanced, Category="Input")
	TArray<UTimeSynchronizationSource*> TimeSynchronizationInputSources;

private:
	
	/** Sources used for synchronization */
	UPROPERTY(VisibleAnywhere, Category=Debug)
	TArray<FTimecodeSynchronizerActiveTimecodedInputSource> ActiveTimecodedInputSources;

	/* Sources that wants to be synchronized */
	UPROPERTY(VisibleAnywhere, Category=Debug)
	TArray<FTimecodeSynchronizerActiveTimecodedInputSource> ActiveSynchronizedSources;

private:

	/** The actual synchronization state */
	ESynchronizationState State;
	
	/** Current FrameTime of the system */
	FFrameTime CurrentFrameTime;
	
	/** Current Timecode of the system  */
	FTimecode CurrentSynchronizedTimecode;
	
	/** Timestamp when PreRolling has started */
	double StartPreRollingTime;
	
	/** Whether or not we are registered as the TimecodeProvider */
	bool bRegistered;
	
	/** Index of the active source that drives the synchronized Timecode*/
	int32 ActiveMasterSynchronizationTimecodedSourceIndex;

	/** An event delegate that is invoked when a synchronization event occurred. */
	FOnTimecodeSynchronizationEvent SynchronizationEvent;
};
