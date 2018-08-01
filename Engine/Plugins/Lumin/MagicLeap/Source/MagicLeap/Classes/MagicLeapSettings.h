// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MagicLeapSettings.generated.h"

/**
* Implements the settings for the Magic Leap SDK setup.
*/
UCLASS(config=Engine, defaultconfig)
class MAGICLEAP_API UMagicLeapSettings : public UObject
{
public:
	GENERATED_BODY()

	// Enables 'Zero Iteration mode'. Note: OpenGL rendering will be used.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", Meta = (DisplayName = "Enable Zero Iteration", ConfigRestartRequired = true))
	bool bEnableZI;
};
