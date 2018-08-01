// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TimecodeProvider.h"

#include "Delegates/Delegate.h"
#include "TimeSynchronizationSource.h"

#include "TimecodeSynchronizer.generated.h"


class UFixedFrameRateCustomTimeStep;
class UTimeSynchronizationSource;


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
 * Enumerates Timecode source type.
 */
UENUM()
enum class ETimecodeSynchronizationTimecodeType
{
	/** Use an external Timecode Provider to provide the timecode to follow. */
	TimecodeProvider,

	/** Use one of the InputSource as the Timecode Provider. */
	InputSource,

	/** Use one of the SystemTime as the Timecode Provider. */
	SystemTime,
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
	virtual bool CanEditChange(const UProperty* InProperty) const override;
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
		PreRolling_WaitGenlockTimecodeProvider,	// wait for the TimecodeProvider & CustomTimeStep to be Ready
		PreRolling_WaitReadiness,	// wait for all source to be Ready
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

	/** Test if the genlock & timecode provider are properly setup */
	bool Tick_TestGenlock();
	bool Tick_TestTimecode();

	/** Process PreRolling_WaitGenlockTimecodeProvider state */
	void TickPreRolling_WaitGenlockTimecodeProvider();

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

	/** Convert Timecode to a FrameTime */
	FFrameTime ConvertTimecodeToFrameTime(const FTimecode& InTimecode) const;
	FTimecode ConvertFrameTimeToTimecode(const FFrameTime& InFFrameTime) const;

	/** Verify if all sources are ready */
	bool AreSourcesReady() const;
	
	/** Start all sources once we're ready to advance time */
	void StartSources();


public:
	/** The fixed framerate to use. */
	UPROPERTY(EditAnywhere, Category="Genlock", meta=(DisplayName="Enable"))
	bool bUseCustomTimeStep;

	/** Custom strategy to tick in a interval. */
	UPROPERTY(EditAnywhere, Instanced, Category="Genlock", meta=(EditCondition="bUseCustomTimeStep", DisplayName="Genlock Source"))
	UFixedFrameRateCustomTimeStep* CustomTimeStep;

	/** The fixed framerate to use. */
	UPROPERTY(EditAnywhere, Category="Genlock", meta=(EditCondition="!bUseCustomTimeStep", ClampMin="15.0"))
	FFrameRate FixedFrameRate;

public:
	/** Use a Timecode Provider. */
	UPROPERTY(EditAnywhere, Category="Timecode Provider", meta=(DisplayName="Select"))
	ETimecodeSynchronizationTimecodeType TimecodeProviderType;

	/** Custom strategy to tick in a interval. */
	UPROPERTY(EditAnywhere, Instanced, Category="Timecode Provider", meta=(EditCondition="IN_CPP", DisplayName="Timecode Source"))
	UTimecodeProvider* TimecodeProvider;

	/**
	 * Index of the source that drives the synchronized Timecode.
	 * The source need to be timecoded and flag as bUseForSynchronization
	 */
	UPROPERTY(EditAnywhere, Category="Timecode Provider", meta=(EditCondition="IN_CPP"))
	int32 MasterSynchronizationSourceIndex;

public:
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
	UPROPERTY(EditAnywhere, Category="Synchronization", meta=(EditCondition="bUsePreRollingTimeout", ClampMin="0.0"))
	float PreRollingTimeout; 

public:
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

	UPROPERTY(Transient)
	UFixedFrameRateCustomTimeStep* RegisteredCustomTimeStep;

	UPROPERTY(Transient)
	UTimecodeProvider* RegisteredTimecodeProvider;

private:

	/** The actual synchronization state */
	ESynchronizationState State;
	bool bSourceStarted;
	
	/** Current FrameTime of the system */
	FFrameTime CurrentFrameTime;
	
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
};
