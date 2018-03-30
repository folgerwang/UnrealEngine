// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrameRate.h"
#include "Timecode.h"

#include "ITimeSynchronizationManager.generated.h"


/**
 * Interface for Time Synchronization Manager module.
 */
class ITimeSynchronizationManager
{
public:

	/**
	 * Get the manager's actual Timecode for this running frame.
	 * Only valid when IsSynchronized() is true
	 */
	UFUNCTION(BlueprintCallable, Category = "TimeSynchronization")
	virtual FTimecode GetCurrentTimecode() const = 0;
	
	/** Get the frame rate of the manager in frames per second */
	virtual FFrameRate GetFrameRate() const = 0;

	/**
	 * All sources are currently synchronizing/buffering, but not all sources are ready yet.
	 * GetCurrentTimecode is not valid in that state.
	 */
	UFUNCTION(BlueprintCallable, Category = "TimeSynchronization")
	virtual bool IsSynchronizing() const = 0;

	/** All sources are currently synchronizing/buffering, but not all sources are ready yet. */
	UFUNCTION(BlueprintCallable, Category = "TimeSynchronization")
	virtual bool IsSynchronized() const = 0;
};


UCLASS(Transient, MinimalAPI, meta = (ScriptName = "TimeSynchronizationLibrary"))
class UTimeSynchronizationManagerHelpers : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get the manager's actual Timecode for this running frame.
	 * Only valid when IsSynchronized() is true
	 */
	UFUNCTION(BlueprintPure, Category = "TimeSynchronization")
	static FTimecode GetCurrentTimecode();

	/**
	 * All sources are currently synchronizing/buffering, but not all sources are ready yet.
	 * GetCurrentTimecode is not valid in that state.
	 */
	UFUNCTION(BlueprintPure, Category = "TimeSynchronization")
	static bool IsSynchronizing();

	/** All sources are currently synchronizing/buffering, but not all sources are ready yet. */
	UFUNCTION(BlueprintPure, Category = "TimeSynchronization")
	static bool IsSynchronized();
};
