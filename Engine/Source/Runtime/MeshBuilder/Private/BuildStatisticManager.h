// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuildStatistic, Log, All);

namespace BuildStatisticManager
{
	class FBuildStatisticScope
	{
	public:
		FBuildStatisticScope(const FString& InTimerDescription)
		{
			TimerDescription = InTimerDescription;
			StartTime = FPlatformTime::Cycles();
		}
		~FBuildStatisticScope()
		{
			float ScopeTime = FPlatformTime::ToSeconds(FPlatformTime::Cycles() - StartTime);
			UE_LOG(LogBuildStatistic, Log, TEXT("%s: %f seconds"), *TimerDescription, ScopeTime);
		}
	private:
		int32 StartTime;
		FString TimerDescription;
	};
}