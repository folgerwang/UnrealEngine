// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MagicLeapMediaCodecPlayer.h"
#include "IMagicLeapMediaCodecModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMagicLeapMediaCodecModule"

class FMagicLeapMediaCodecModule : public IMagicLeapMediaCodecModule
{
public:
	/** IMagicLeapMediaCodecModule interface */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink)
	{
		return MakeShareable(new FMagicLeapMediaCodecPlayer(EventSink));
	}

public:
	/** IModuleInterface interface */
	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};

IMPLEMENT_MODULE(FMagicLeapMediaCodecModule, MagicLeapMediaCodec)

DEFINE_LOG_CATEGORY(LogMagicLeapMediaCodec);

#undef LOCTEXT_NAMESPACE
