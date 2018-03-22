// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameRate.h"
#include "QualifiedFrameTime.h"
#include "TimeManagementBlueprintLibrary.generated.h"

/**
 * 
 */
UCLASS(meta = (BlueprintThreadSafe))
class TIMEMANAGEMENT_API UTimeManagementBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Converts an FrameRate to a float ie: 1/30 returns 0.0333333 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "FrameRate to Seconds", BlueprintAutocast), Category = "Utilities|Time Management")
	static float Conv_FrameRateToSeconds(const FFrameRate& InFrameRate);

	/** Converts an QualifiedFrameTime to seconds. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "QualifiedFrameTime to Seconds", BlueprintAutocast), Category = "Utilities|Time Management")
	static float Conv_QualifiedFrameTimeToSeconds(const FQualifiedFrameTime& InFrameTime);

	/** Multiplies a value in seconds against a FrameRate to get a new FrameTime. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Seconds * FrameRate", CompactNodeTitle = "*"), Category = "Utilities|Time Management")
	static FFrameTime Multiply_SecondsFrameRate(float TimeInSeconds, const FFrameRate& FrameRate);
};