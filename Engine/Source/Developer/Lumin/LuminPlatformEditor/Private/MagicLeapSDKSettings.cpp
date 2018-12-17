// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapSDKSettings.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(MagicLeapSDKSettings, Log, All);

UMagicLeapSDKSettings::UMagicLeapSDKSettings()
	: TargetManagerModule(nullptr)
	, LuminDeviceDetection(nullptr)
{
}

#if WITH_EDITOR
void UMagicLeapSDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateTargetModulePaths();
}

void UMagicLeapSDKSettings::SetTargetModule(ITargetPlatformManagerModule* InTargetManagerModule)
{
	TargetManagerModule = InTargetManagerModule;
}

void UMagicLeapSDKSettings::SetDeviceDetection(IAndroidDeviceDetection* InLuminDeviceDetection)
{
	LuminDeviceDetection = InLuminDeviceDetection;
}

void UMagicLeapSDKSettings::UpdateTargetModulePaths()
{
	TArray<FString> Keys;
	TArray<FString> Values;
	
	if (!MLSDKPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(MLSDKPath.Path);
		Keys.Add(TEXT("MLSDK"));
		Values.Add(MLSDKPath.Path);
	}

	SaveConfig();
	
	if (Keys.Num() != 0)
	{
		if (TargetManagerModule != nullptr)
		{
			TargetManagerModule->UpdatePlatformEnvironment(TEXT("Lumin"), Keys, Values);
		}

		if (LuminDeviceDetection != nullptr)
		{
			LuminDeviceDetection->UpdateADBPath();
		}
	}
}

#endif
