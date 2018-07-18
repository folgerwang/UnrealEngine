// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapEmulatorSettings.h"

UMagicLeapEmulatorSettings::UMagicLeapEmulatorSettings()
{
	bEnableMagicLeapEmulation = false;
	bEnableCollisionWithBackground = true;
	ForegroundAspectRatio = 2.f;
	ForegroundHorizontalFOV = 30.f;
	bLimitForegroundFOV = false;
	EmulatorCompositingMaterial.SetPath(TEXT("/MagicLeapEmulator/Materials/M_EmulatorBackground.M_EmulatorBackground"));
}
