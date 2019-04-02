// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timespan.h"


/**
 * Interface for media clock time sources.
 */
class IMediaTimeSource
{
public:

	/**
	 * Get the current time code.
	 *
	 * @return Time code.
	 */
	virtual FTimespan GetTimecode() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaTimeSource() { }
};
