// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppMediaTimeSource.h"
#include "MediaUtilsPrivate.h"

#include "Misc/App.h"
#include "Misc/Timespan.h"


/* IMediaTimeSource interface 
 *****************************************************************************/

FTimespan FAppMediaTimeSource::GetTimecode()
{
	const double CurrentTime = FApp::GetCurrentTime();
	const FTimespan Timecode = FTimespan::FromSeconds(CurrentTime);

	UE_LOG(LogMediaUtils, VeryVerbose, TEXT("AppMediaTimeSource: Time %.10f, Delta %.10f, Timecode %s"), CurrentTime, FApp::GetDeltaTime(), *Timecode.ToString(TEXT("%h:%m:%s.%t")));

	return Timecode;
}
