// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FixedFrameRateCustomTimeStep.h"

#include "Misc/App.h"
#include "Stats/StatsMisc.h"

#include "HAL/PlatformProcess.h"

UFixedFrameRateCustomTimeStep::UFixedFrameRateCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FixedFrameRate(30, 1)
{
}

void UFixedFrameRateCustomTimeStep::WaitForFixedFrameRate() const
{
	UpdateApplicationLastTime();

	const double CurrentTime = FPlatformTime::Seconds();

	// Calculate delta time
	const float DeltaRealTime = CurrentTime - FApp::GetLastTime();
	const float WaitTime = FMath::Max(FixedFrameRate.AsInterval() - DeltaRealTime, 0.0);

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
	FApp::SetDeltaTime(FixedFrameRate.AsInterval());
	FApp::SetIdleTime(ActualWaitTime);
	FApp::SetCurrentTime(FApp::GetLastTime() + FApp::GetDeltaTime());
}
