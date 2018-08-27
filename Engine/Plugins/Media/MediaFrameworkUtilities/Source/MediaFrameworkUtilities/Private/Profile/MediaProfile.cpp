// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "Profile/MediaProfile.h"

#include "MediaFrameworkUtilitiesModule.h"

#include "Engine/Engine.h"
#include "MediaAssets/ProxyMediaOutput.h"
#include "MediaAssets/ProxyMediaSource.h"
#include "MediaOutput.h"
#include "MediaSource.h"
#include "Profile/MediaProfileSettings.h"


UMediaSource* UMediaProfile::GetMediaSource(int32 Index) const
{
	if (MediaSources.IsValidIndex(Index))
	{
		return MediaSources[Index];
	}
	return nullptr;
}


UMediaOutput* UMediaProfile::GetMediaOutput(int32 Index) const
{
	if (MediaOutputs.IsValidIndex(Index))
	{
		return MediaOutputs[Index];
	}
	return nullptr;
}


void UMediaProfile::Apply()
{
	if (GEngine == nullptr)
	{
		UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("The MediaProfile '%s' could not be applied. The Engine is not initialized."), *GetName());
		return;
	}

	{
		TArray<UProxyMediaSource*> SourceProxies = GetDefault<UMediaProfileSettings>()->GetAllMediaSourceProxy();
		if (MediaSources.Num() > SourceProxies.Num())
		{
			UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("The MediaProfile '%s' has too many sources."), *GetName());
		}

		int32 Index = 0;
		for (; Index < MediaSources.Num() && Index < SourceProxies.Num(); ++Index)
		{
			UProxyMediaSource* Proxy = SourceProxies[Index];
			if (Proxy)
			{
				Proxy->SetDynamicMediaSource(MediaSources[Index]);
			}
		}
		// Reset the other proxies
		for (; Index < SourceProxies.Num(); ++Index)
		{
			UProxyMediaSource* Proxy = SourceProxies[Index];
			if (Proxy)
			{
				Proxy->SetDynamicMediaSource(nullptr);
			}
		}
	}

	{
		TArray<UProxyMediaOutput*> OutputProxies = GetDefault<UMediaProfileSettings>()->GetAllMediaOutputProxy();
		if (MediaOutputs.Num() > OutputProxies.Num())
		{
			UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("The MediaProfile '%s' has too many outputs."), *GetName());
		}

		int32 Index = 0;
		for (; Index < MediaOutputs.Num() && Index < OutputProxies.Num(); ++Index)
		{
			UProxyMediaOutput* Proxy = OutputProxies[Index];
			if (Proxy)
			{
				Proxy->SetDynamicMediaOutput(MediaOutputs[Index]);
			}
		}
		// Reset the other proxies
		for (; Index < OutputProxies.Num(); ++Index)
		{
			UProxyMediaOutput* Proxy = OutputProxies[Index];
			if (Proxy)
			{
				Proxy->SetDynamicMediaOutput(nullptr);
			}
		}
	}
}
