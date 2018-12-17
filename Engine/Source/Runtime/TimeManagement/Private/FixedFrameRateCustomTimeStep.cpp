// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FixedFrameRateCustomTimeStep.h"

#include "Misc/App.h"
#include "Stats/StatsMisc.h"

#include "HAL/PlatformProcess.h"


PRAGMA_DISABLE_DEPRECATION_WARNINGS
UFixedFrameRateCustomTimeStep::UFixedFrameRateCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FixedFrameRate(30, 1)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UFixedFrameRateCustomTimeStep::WaitForFixedFrameRate() const
{
	UpdateApplicationLastTime();

	const double CurrentTime = FPlatformTime::Seconds();

	const FFrameRate FrameRate = GetFixedFrameRate();

	// Calculate delta time
	const float DeltaRealTime = CurrentTime - FApp::GetLastTime();
	const float WaitTime = FMath::Max(FrameRate.AsInterval() - DeltaRealTime, 0.0);

	double ActualWaitTime = 0.0;
	{
		FSimpleScopeSecondsCounter ActualWaitTimeCounter(ActualWaitTime);

		if (WaitTime > 5.f / 1000.f)
		{
			FPlatformProcess::SleepNoStats(WaitTime - 0.002f);
		}

		// Give up timeslice for remainder of wait time.
		const double WaitEndTime = FApp::GetLastTime() + FApp::GetDeltaTime();
		while (FPlatformTime::Seconds() < WaitEndTime)
		{
			FPlatformProcess::SleepNoStats(0.f);
		}
	}

	// Use fixed delta time and update time.
	FApp::SetDeltaTime(FrameRate.AsInterval());
	FApp::SetIdleTime(ActualWaitTime);
	FApp::SetCurrentTime(FApp::GetLastTime() + FApp::GetDeltaTime());
}

FFrameRate UFixedFrameRateCustomTimeStep::GetFixedFrameRate_PureVirtual() const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return FixedFrameRate;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
