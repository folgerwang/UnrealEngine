// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaFrameworkWorldSettingsAssetUserData.h"
#include "UObject/EnterpriseObjectVersion.h"


FMediaFrameworkCaptureCameraViewportCameraOutputInfo::FMediaFrameworkCaptureCameraViewportCameraOutputInfo()
	: MediaOutput(nullptr)
	, ViewMode(VMI_Unknown)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UMediaFrameworkWorldSettingsAssetUserData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEnterpriseObjectVersion::GUID);
	if (Ar.IsLoading() && FEnterpriseObjectVersion::MediaFrameworkUserDataLazyObject > Ar.CustomVer(FEnterpriseObjectVersion::GUID))
	{
		for (FMediaFrameworkCaptureCameraViewportCameraOutputInfo& OutputInfo : ViewportCaptures)
		{
			if (OutputInfo.LockedCameraActors_DEPRECATED.Num() > 0)
			{
				for (AActor* Actor : OutputInfo.LockedCameraActors_DEPRECATED)
				{
					if (Actor)
					{
						OutputInfo.LockedActors.Add(Actor);
					}
				}
				OutputInfo.LockedCameraActors_DEPRECATED.Empty();
			}
		}
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
