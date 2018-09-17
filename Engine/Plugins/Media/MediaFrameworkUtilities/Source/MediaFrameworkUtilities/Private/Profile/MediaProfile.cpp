// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "Profile/MediaProfile.h"

#include "MediaFrameworkUtilitiesModule.h"

#include "Engine/Engine.h"
#include "Engine/EngineCustomTimeStep.h"
#include "Engine/TimecodeProvider.h"
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


UTimecodeProvider* UMediaProfile::GetTimecodeProvider() const
{
	return bOverrideTimecodeProvider ? TimecodeProvider : nullptr;
}


UEngineCustomTimeStep* UMediaProfile::GetCustomTimeStep() const
{
	return bOverrideCustomTimeStep ? CustomTimeStep : nullptr;
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

	if (bOverrideTimecodeProvider)
	{
		if (TimecodeProvider)
		{
			bool bResult = GEngine->SetTimecodeProvider(TimecodeProvider);
			if (!bResult)
			{
				UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("The TimecodeProvider '%s' could not be initialized."), *TimecodeProvider->GetName());
			}
		}
		else
		{
			GEngine->SetTimecodeProvider(nullptr);
		}
	}

	if (bOverrideCustomTimeStep)
	{
		if (CustomTimeStep)
		{
			bool bResult = GEngine->SetCustomTimeStep(CustomTimeStep);
			if (!bResult)
			{
				UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("The Custom Time Step '%s' could not be initialized."), *CustomTimeStep->GetName());
			}
		}
		else
		{
			GEngine->SetCustomTimeStep(nullptr);
		}
	}
}


void UMediaProfile::Reset()
{
	if (GEngine == nullptr)
	{
		UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("The MediaProfile '%s' could not be reset. The Engine is not initialized."), *GetName());
		return;
	}

	{
		// Reset the proxies
		TArray<UProxyMediaSource*> SourceProxies = GetDefault<UMediaProfileSettings>()->GetAllMediaSourceProxy();
		for (UProxyMediaSource* Proxy : SourceProxies)
		{
			if (Proxy)
			{
				Proxy->SetDynamicMediaSource(nullptr);
			}
		}
	}

	{
		TArray<UProxyMediaOutput*> OutputProxies = GetDefault<UMediaProfileSettings>()->GetAllMediaOutputProxy();
		for (UProxyMediaOutput* Proxy : OutputProxies)
		{
			if (Proxy)
			{
				Proxy->SetDynamicMediaOutput(nullptr);
			}
		}
	}

	if (bOverrideTimecodeProvider)
	{
		GEngine->SetTimecodeProvider(nullptr);
	}

	if (bOverrideCustomTimeStep)
	{
		GEngine->SetCustomTimeStep(GEngine->GetDefaultCustomTimeStep());
	}
}
