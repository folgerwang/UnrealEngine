// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TimeManagementBlueprintLibrary.h"

#include "Misc/App.h"

float UTimeManagementBlueprintLibrary::Conv_FrameRateToSeconds(const FFrameRate& InFrameRate)
{
	// Accept the loss of precision from conversion when in use with Blueprints.
	return (float)InFrameRate.AsDecimal();
}

float UTimeManagementBlueprintLibrary::Conv_QualifiedFrameTimeToSeconds(const FQualifiedFrameTime& InFrameTime)
{
	// Accept the loss of precision from conversion when in use with Blueprints.
	return (float)InFrameTime.AsSeconds();
}

FFrameTime UTimeManagementBlueprintLibrary::Multiply_SecondsFrameRate(float TimeInSeconds, const FFrameRate& FrameRate)
{
	return FrameRate.AsFrameTime(TimeInSeconds);
}

FString UTimeManagementBlueprintLibrary::Conv_TimecodeToString(const FTimecode& InTimecode, bool bForceSignDisplay)
{
	return InTimecode.ToString(bForceSignDisplay);
}

FTimecode UTimeManagementBlueprintLibrary::GetTimecode()
{
	return FApp::GetTimecode();
}
