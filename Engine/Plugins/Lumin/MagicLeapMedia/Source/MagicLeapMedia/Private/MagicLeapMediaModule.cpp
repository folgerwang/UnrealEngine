// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MagicLeapMediaPlayer.h"
#include "MagicLeapCameraPreviewPlayer.h"
#include "IMagicLeapMediaModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMagicLeapMediaModule"

class FMagicLeapMediaModule : public IMagicLeapMediaModule
{
public:
	/** IMagicLeapMediaModule interface */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink)
	{
		return MakeShareable(new FMagicLeapMediaPlayer(EventSink));
	}

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreateCameraPreviewPlayer(IMediaEventSink& EventSink)
	{
		return MakeShareable(new FMagicLeapCameraPreviewPlayer(EventSink));
	}

public:
	/** IModuleInterface interface */
	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};

IMPLEMENT_MODULE(FMagicLeapMediaModule, MagicLeapMedia)

DEFINE_LOG_CATEGORY(LogMagicLeapMedia);

#undef LOCTEXT_NAMESPACE
