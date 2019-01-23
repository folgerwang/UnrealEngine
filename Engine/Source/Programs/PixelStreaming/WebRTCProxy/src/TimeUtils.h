// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

struct FDateTime_BypassRedefinition
{
	FDateTime_BypassRedefinition(int Year, int Month, int Day, int Hour, int Minute, int Second, int Milliseconds)
	    : Year(Year)
	    , Month(Month)
	    , Day(Day)
	    , Hour(Hour)
	    , Minute(Minute)
	    , Second(Second)
	    , MSec(Milliseconds)
	{
	}

	// Full year (e.g: 2018)
	int Year;
	// 1..12
	int Month;
	// Day of the month (e.g: 1..31)
	int Day;
	// (0..23)
	int Hour;
	// (0..59)
	int Minute;
	// (0..59)
	int Second;
	// Milliseconds (0..999)
	int MSec;

	/**
	Formats in a way ready for logging, matching UE4 format
	YYYY.MM.DD-HH.MM.SS:MSEC
	*/
	const char* ToString(bool bIncludeMSec=true);
};

// FDateTime will be removed.
#define FDateTime FDateTime_BypassRedefinition

/**
 * Returns the local date/time
 */
FDateTime Now(); 

/**
 * Returns curent UTC date/time
 */
FDateTime UtcNow();
