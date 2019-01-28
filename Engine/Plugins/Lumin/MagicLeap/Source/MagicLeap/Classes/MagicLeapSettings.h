// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

	// Enables 'Zero Iteration mode'. Note: Vulkan rendering will be used by default. Set bUseVulkan to false to use OpenGL instead.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", Meta = (DisplayName = "Enable Zero Iteration", ConfigRestartRequired = true))
	bool bEnableZI;

	// Use the editor in Vulkan. If False, OpenGL is used with ZI.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "General", Meta = (DisplayName = "Use Vulkan with Zero Iteration (otherwise, OpenGL)", ConfigRestartRequired = true))
	bool bUseVulkanForZI;
};
