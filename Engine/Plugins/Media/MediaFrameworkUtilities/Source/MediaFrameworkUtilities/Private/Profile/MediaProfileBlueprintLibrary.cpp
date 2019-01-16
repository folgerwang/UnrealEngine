// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Profile/MediaProfileBlueprintLibrary.h"

#include "Features/IModularFeatures.h"
#include "Profile/IMediaProfileManager.h"
#include "Profile/MediaProfileSettings.h"


UMediaProfile* UMediaProfileBlueprintLibrary::GetMediaProfile()
{
	return IMediaProfileManager::Get().GetCurrentMediaProfile();
}


void UMediaProfileBlueprintLibrary::SetMediaProfile(UMediaProfile* MediaProfile)
{
	return IMediaProfileManager::Get().SetCurrentMediaProfile(MediaProfile);
}


TArray<UProxyMediaSource*> UMediaProfileBlueprintLibrary::GetAllMediaSourceProxy()
{
	return GetDefault<UMediaProfileSettings>()->GetAllMediaSourceProxy();
}


TArray<UProxyMediaOutput*> UMediaProfileBlueprintLibrary::GetAllMediaOutputProxy()
{
	return GetDefault<UMediaProfileSettings>()->GetAllMediaOutputProxy();
}
