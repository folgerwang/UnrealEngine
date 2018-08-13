// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Profile/MediaProfileManager.h"
#include "MediaFrameworkUtilitiesModule.h"

#include "Modules/ModuleManager.h"

IMediaProfileManager& IMediaProfileManager::Get()
{
	static FName NAME_MediaFrameworkUtilities = TEXT("MediaFrameworkUtilities");
	return FModuleManager::GetModuleChecked<IMediaFrameworkUtilitiesModule>(NAME_MediaFrameworkUtilities).GetProfileManager();
}

UMediaProfile* FMediaProfileManager::GetCurrentMediaProfile() const
{
	return CurrentMediaProfile.Get();
}

void FMediaProfileManager::SetCurrentMediaProfile(UMediaProfile* InMediaProfile)
{
	UMediaProfile* Previous = CurrentMediaProfile.Get();
	if (InMediaProfile != Previous)
	{
		if (InMediaProfile)
		{
			InMediaProfile->Apply();
		}
		else
		{
			GetMutableDefault<UMediaProfile>()->Apply();
		}

		CurrentMediaProfile.Reset(InMediaProfile);
		MediaProfileChangedDelegate.Broadcast(Previous, InMediaProfile);
	}
}

FMediaProfileManager::FOnMediaProfileChanged& FMediaProfileManager::OnMediaProfileChanged()
{
	return MediaProfileChangedDelegate;
}
