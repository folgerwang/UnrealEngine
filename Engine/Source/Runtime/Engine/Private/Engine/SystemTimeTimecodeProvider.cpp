// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/SystemTimeTimecodeProvider.h"

#include "Misc/CoreMisc.h"
#include "Misc/DateTime.h"


FTimecode USystemTimeTimecodeProvider::GetTimecode() const
{
	const FDateTime DateTime = FDateTime::Now();
	const FTimespan Timespan = DateTime.GetTimeOfDay();

	return FTimecode::FromTimespan(Timespan, FrameRate, FTimecode::IsDropFormatTimecodeSupported(FrameRate), false);
}
