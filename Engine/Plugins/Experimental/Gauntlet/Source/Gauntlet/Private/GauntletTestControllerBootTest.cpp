// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GauntletTestControllerBootTest.h"



void UGauntletTestControllerBootTest::OnTick(float TimeDelta)
{
	if (IsBootProcessComplete())
	{
		EndTest(0);
	}
	else
	{
		if (GetTimeInCurrentState() > 300)
		{
			UE_LOG(LogGauntlet, Error, TEXT("Failing boot test after 300 secs!"));
			EndTest(-1);
		}
	}
}