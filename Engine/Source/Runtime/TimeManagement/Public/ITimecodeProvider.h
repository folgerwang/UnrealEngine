// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrameRate.h"
#include "Timecode.h"

/**
 * Interface for timecode provider.
 * The interface is responsible to have a consistent Timecode for the duration of the frame.
 */
class ITimecodeProvider
{
public:

	/**
	 * Return the Timecode for this running frame.
	 * Only valid when IsSynchronized() is true
	 */
	UFUNCTION(BlueprintCallable, Category = "Provider")
	virtual FTimecode GetCurrentTimecode() const = 0;
	
	/**
	 * Return the frame rate.
	 * Only valid when IsSynchronized() is true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Provider")
	virtual FFrameRate GetFrameRate() const = 0;

	/**
	 * The provider is trying to connect to the source and may not have a valid decoded timecode or frame rate yet.
	 * GetCurrentTimecode is not valid in that state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Provider")
	virtual bool IsSynchronizing() const = 0;

	/** The source is currently synchronized and the Timecode and FrameRate are valid */
	UFUNCTION(BlueprintCallable, Category = "Provider")
	virtual bool IsSynchronized() const = 0;
};
