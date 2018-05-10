// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapEmulatorSceneCaptureComponent.h"
#include "MagicLeapEmulator.h"

UMagicLeapEmulatorSceneCaptureComponent::UMagicLeapEmulatorSceneCaptureComponent()
{
	bCaptureEveryFrame = true;
	bCaptureOnMovement = false;

	CaptureSource = SCS_FinalColorLDR;
}

void UMagicLeapEmulatorSceneCaptureComponent::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	// update component transform as late as possible
	if (Emulator)
	{
		Emulator->UpdateSceneCaptureTransform(this);
	}

	Super::UpdateSceneCaptureContents(Scene);
}

