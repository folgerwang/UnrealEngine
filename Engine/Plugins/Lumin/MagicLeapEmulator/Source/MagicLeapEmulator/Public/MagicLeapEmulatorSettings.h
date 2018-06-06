// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MagicLeapEmulatorSettings.generated.h"

UCLASS(config=Engine, defaultconfig)
class MAGICLEAPEMULATOR_API UMagicLeapEmulatorSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	UMagicLeapEmulatorSettings();

 	/** True to make play-in-editor (including VR) emulate the Magic Leap AR hmd. */
 	UPROPERTY(config, EditAnywhere, Category = MagicLeapEmulator)
 	bool bEnableMagicLeapEmulation;

	UPROPERTY(config, EditAnywhere, Category = MagicLeapEmulator)
	bool bEnableCollisionWithBackground;

	/** True to emulate the limited FOV of the ML display. */
	UPROPERTY(config, EditAnywhere, Category = MagicLeapEmulator)
	bool bLimitForegroundFOV;

	/** Assumed aspect ratio of the ML display, to simulate the limited FOV. Ignored if bLimitForegroundFOV is false. */
	UPROPERTY(config, EditAnywhere, Category = MagicLeapEmulator, meta = (EditCondition = "bLimitForegroundFOV"))
	float ForegroundAspectRatio;

	/** In degrees. Ignored if bLimitForegroundFOV is false*/
	UPROPERTY(config, EditAnywhere, Category = MagicLeapEmulator, meta = (EditCondition = "bLimitForegroundFOV"))
	float ForegroundHorizontalFOV;

	/** */
	UPROPERTY(config, EditAnywhere, Category = MagicLeapEmulator, meta = (AllowedClasses = "MaterialInterface"))
	FStringAssetReference EmulatorCompositingMaterial;
};
