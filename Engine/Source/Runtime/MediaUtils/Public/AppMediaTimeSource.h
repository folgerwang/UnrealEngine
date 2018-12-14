// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTimeSource.h"


/**
 * Implements the a media time source that derives its time from the application's global time.
 */
class MEDIAUTILS_API FAppMediaTimeSource
	: public IMediaTimeSource
{
public:

	/** Virtual destructor. */
	virtual ~FAppMediaTimeSource() { }

public:

	//~ IMediaTimeSource interface

	virtual FTimespan GetTimecode() override;
};
