// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AudioCompressionSettings.h"

FPlatformRuntimeAudioCompressionOverrides::FPlatformRuntimeAudioCompressionOverrides()
	: bOverrideCompressionTimes(false)
	, DurationThreshold(5.0f)
	, MaxNumRandomBranches(0)
	, SoundCueQualityIndex(0)
{
	
}

FPlatformRuntimeAudioCompressionOverrides* FPlatformRuntimeAudioCompressionOverrides::DefaultCompressionOverrides = nullptr;
