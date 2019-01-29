// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IMediaModule.h"
#include "IMagicLeapMediaModule.h"
#include "MagicLeapMediaFactoryPrivate.h"
#include "IMediaPlayerFactory.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "FMagicLeapCameraPreviewFactoryModule"

/**
 * Implements the MagicLeapCameraPreviewFactory module.
 */
class FMagicLeapCameraPreviewFactoryModule : public IMediaPlayerFactory, public IModuleInterface
{
public:
	/** IMediaPlayerFactory interface */
	virtual bool CanPlayUrl(const FString& Url, const IMediaOptions* /*Options*/, TArray<FText>* /*OutWarnings*/, TArray<FText>* OutErrors) const override
	{
		(void)Url;
		(void)OutErrors;
		return true;
	}

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		// Disable camera preview on Vulkan until we are able to get it working.
		// Can't call FLuminPlatformMisc::ShouldUseVulkan from here ...
		bool bUseVulkan = false;
		if (GConfig != nullptr)
		{
			GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bUseVulkan"), bUseVulkan, GEngineIni);
		}
		
		if (!bUseVulkan)
		{
			auto MagicLeapMediaModule = FModuleManager::LoadModulePtr<IMagicLeapMediaModule>("MagicLeapMedia");
			if (MagicLeapMediaModule != nullptr)
			{
				return MagicLeapMediaModule->CreateCameraPreviewPlayer(EventSink);
			}
		}
		return nullptr;
	}

	virtual FText GetDisplayName() const override
	{
		return LOCTEXT("MediaPlayerDisplayName", "MagicLeap Camera Preview");
	}

	virtual FName GetPlayerName() const override
	{
		static FName PlayerName(TEXT("MagicLeapCameraPreview"));
		return PlayerName;
	}

	virtual const TArray<FString>& GetSupportedPlatforms() const override
	{
		return SupportedPlatforms;
	}

	virtual bool SupportsFeature(EMediaFeature Feature) const
	{
		return ((Feature == EMediaFeature::AudioTracks) ||
			(Feature == EMediaFeature::VideoSamples) ||
			(Feature == EMediaFeature::VideoTracks));
	}

public:
	/** IModuleInterface interface */
	
	virtual void StartupModule() override
	{
    	// supported platforms
    	SupportedPlatforms.Add(TEXT("Lumin"));
    	// Hack until we get a separete ini platform for Lumin. Will not affect Android since this plugin is not built for it.
    	SupportedPlatforms.Add(TEXT("Android"));
		// register media player info
		auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MediaModule->RegisterPlayerFactory(*this);
		}
	}

	virtual void ShutdownModule() override
	{
		// unregister player factory
		auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
		
		if (MediaModule != nullptr)
		{
			MediaModule->UnregisterPlayerFactory(*this);
		}
	}

private:
	/** List of platforms that the media player support. */
	TArray<FString> SupportedPlatforms;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMagicLeapCameraPreviewFactoryModule, MagicLeapCameraPreviewFactory);