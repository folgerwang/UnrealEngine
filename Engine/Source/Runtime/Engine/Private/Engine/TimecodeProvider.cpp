// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Engine/TimecodeProvider.h"

#include "Misc/CoreMisc.h"
#include "Misc/DateTime.h"


FTimecode UTimecodeProvider::GetSystemTimeTimecode(const FFrameRate& InForFrameRate)
{
	const FDateTime DateTime = FDateTime::Now();
	const FTimespan Timespan = DateTime.GetTimeOfDay();
	const double TotalSeconds = Timespan.GetTotalSeconds();
	FFrameNumber FrameNumber = InForFrameRate.AsFrameNumber(TotalSeconds);

	return FTimecode::FromFrameNumber(FrameNumber, InForFrameRate, FTimecode::IsDropFormatTimecodeSupported(InForFrameRate));
}
