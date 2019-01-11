// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Recorder/TakeRecorderParameters.h"

FTakeRecorderUserParameters::FTakeRecorderUserParameters()
	: bMaximizeViewport(false)
	, CountdownSeconds(0.f)
	, EngineTimeDilation(1.f)
	, SampleClock(nullptr)
	, bRemoveRedundantTracks(true)
	, bSaveRecordedAssets(false)
	, bAutoSerialize(false)
{
	// Defaults for all user parameter structures
	// User defaults should be set in UTakeRecorderUserSettings
	// So as not to affect structs created with script
}

FTakeRecorderProjectParameters::FTakeRecorderProjectParameters()
	: bRecordSourcesIntoSubSequences(false)
	, bRecordToPossessable(false)
{}

FTakeRecorderParameters::FTakeRecorderParameters()
{}
