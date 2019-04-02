// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "WindowsMixedRealityRuntimeSettings.generated.h"

/**
* Add a default value for every new UPROPERTY in this class in <UnrealEngine>/Engine/Config/BaseEngine.ini
*/

/**
 * Implements the settings for the WindowsMixedReality runtime platform.
 */
UCLASS(config=Engine, defaultconfig)
class WINDOWSMIXEDREALITYRUNTIMESETTINGS_API UWindowsMixedRealityRuntimeSettings : public UObject
{
public:
	GENERATED_BODY()

private:
	static class UWindowsMixedRealityRuntimeSettings* WMRSettingsSingleton;

public:
	static UWindowsMixedRealityRuntimeSettings* Get();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** The IP of the HoloLens to remote to. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Holographic Remoting", Meta = (DisplayName = "IP of HoloLens to remote to."))
	FString RemoteHoloLensIP;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Holographic Remoting", Meta = (DisplayName = "Max network transfer rate (kb/s)"))
	unsigned int MaxBitrate = 4000;
};
