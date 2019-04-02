// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealityRuntimeSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "CoreGlobals.h"
#include "UObject/Package.h"

UWindowsMixedRealityRuntimeSettings* UWindowsMixedRealityRuntimeSettings::WMRSettingsSingleton = nullptr;

#if WITH_EDITOR

void UWindowsMixedRealityRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	GConfig->Flush(1);
}

#endif

UWindowsMixedRealityRuntimeSettings* UWindowsMixedRealityRuntimeSettings::Get()
{
	if (WMRSettingsSingleton == nullptr)
	{
		static const TCHAR* SettingsContainerName = TEXT("WindowsMixedRealityRuntimeSettingsContainer");

		WMRSettingsSingleton = FindObject<UWindowsMixedRealityRuntimeSettings>(GetTransientPackage(), SettingsContainerName);

		if (WMRSettingsSingleton == nullptr)
		{
			WMRSettingsSingleton = NewObject<UWindowsMixedRealityRuntimeSettings>(GetTransientPackage(), UWindowsMixedRealityRuntimeSettings::StaticClass(), SettingsContainerName);
			WMRSettingsSingleton->AddToRoot();
		}
	}
	return WMRSettingsSingleton;
}