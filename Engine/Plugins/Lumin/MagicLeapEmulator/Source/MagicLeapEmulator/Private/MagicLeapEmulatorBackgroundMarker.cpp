// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapEmulatorBackgroundMarker.h"

AMagicLeapEmulatorBackgroundMarker::AMagicLeapEmulatorBackgroundMarker()
{
	bParentLevelIsBackgroundLevel = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AMagicLeapEmulatorBackgroundMarker::Tick(float DeltaTime)
{
	if (Emulator)
	{
		//Emulator->Update(DeltaTime);
	}
}
