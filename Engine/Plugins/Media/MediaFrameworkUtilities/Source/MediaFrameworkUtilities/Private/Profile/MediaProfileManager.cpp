// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Profile/MediaProfileManager.h"
#include "MediaFrameworkUtilitiesModule.h"

#include "Modules/ModuleManager.h"
#include "MediaAssets/ProxyMediaOutput.h"
#include "MediaAssets/ProxyMediaSource.h"
#include "Profile/MediaProfile.h"
#include "Profile/MediaProfileSettings.h"


IMediaProfileManager& IMediaProfileManager::Get()
{
	static FName NAME_MediaFrameworkUtilities = TEXT("MediaFrameworkUtilities");
	return FModuleManager::GetModuleChecked<IMediaFrameworkUtilitiesModule>(NAME_MediaFrameworkUtilities).GetProfileManager();
}


FMediaProfileManager::FMediaProfileManager()
	: Collector(this)
	, CurrentMediaProfile(nullptr)
{
}


UMediaProfile* FMediaProfileManager::GetCurrentMediaProfile() const
{
	return CurrentMediaProfile;
}


void FMediaProfileManager::SetCurrentMediaProfile(UMediaProfile* InMediaProfile)
{
	bool bChanged = false;
	UMediaProfile* Previous = CurrentMediaProfile;
	if (InMediaProfile != Previous)
	{
		if (Previous)
		{
			Previous->Reset();
		}

		if (InMediaProfile)
		{
			InMediaProfile->Apply();
		}

		// Set Current assets to prevent GC
		CurrentMediaProfile = InMediaProfile;
		CurrentProxyMediaSources.Reset();
		CurrentProxyMediaOutputs.Reset();

		if (CurrentMediaProfile)
		{
			TArray<UProxyMediaSource*> SourceProxies = GetDefault<UMediaProfileSettings>()->GetAllMediaSourceProxy();
			const int32 MinSourceProxies = FMath::Min(SourceProxies.Num(), CurrentMediaProfile->NumMediaSources());
			for (int32 Index = 0; Index < MinSourceProxies; ++Index)
			{
				UProxyMediaSource* Proxy = SourceProxies[Index];
				if (Proxy)
				{
					CurrentProxyMediaSources.Add(Proxy);
				}
			}
		}

		if (CurrentMediaProfile)
		{
			TArray<UProxyMediaOutput*> OutputProxies = GetDefault<UMediaProfileSettings>()->GetAllMediaOutputProxy();
			const int32 MinOutputProxies = FMath::Min(OutputProxies.Num(), CurrentMediaProfile->NumMediaOutputs());
			for (int32 Index = 0; Index < MinOutputProxies; ++Index)
			{
				UProxyMediaOutput* Proxy = OutputProxies[Index];
				if (Proxy)
				{
					CurrentProxyMediaOutputs.Add(Proxy);
				}
			}
		}

		MediaProfileChangedDelegate.Broadcast(Previous, InMediaProfile);
	}
}


FMediaProfileManager::FOnMediaProfileChanged& FMediaProfileManager::OnMediaProfileChanged()
{
	return MediaProfileChangedDelegate;
}


FMediaProfileManager::FInternalReferenceCollector::FInternalReferenceCollector(FMediaProfileManager* InOwner)
	: Owner(InOwner)
{
}


void FMediaProfileManager::FInternalReferenceCollector::AddReferencedObjects(FReferenceCollector& InCollector)
{
	check(Owner);
	InCollector.AddReferencedObject(Owner->CurrentMediaProfile);
	InCollector.AddReferencedObjects(Owner->CurrentProxyMediaSources);
	InCollector.AddReferencedObjects(Owner->CurrentProxyMediaOutputs);
}
