// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE:  All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law.  Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY.  Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure  of  this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of  COMPANY.   ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC  PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE  OF THIS
// SOURCE CODE  WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES.  THE RECEIPT OR POSSESSION OF  THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT  MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------*/
// %BANNER_END%

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IMediaModule.h"
#include "IMagicLeapMediaModule.h"
#include "MagicLeapMediaFactoryPrivate.h"
#include "IMediaPlayerFactory.h"

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
		auto MagicLeapMediaModule = FModuleManager::LoadModulePtr<IMagicLeapMediaModule>("MagicLeapMedia");
		return (MagicLeapMediaModule != nullptr) ? MagicLeapMediaModule->CreateCameraPreviewPlayer(EventSink) : nullptr;
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