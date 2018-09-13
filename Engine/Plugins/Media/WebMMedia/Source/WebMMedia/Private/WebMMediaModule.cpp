// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMMediaPrivate.h"

#include "Modules/ModuleManager.h"

#include "IWebMMediaModule.h"
#include "Player/WebMMediaPlayer.h"


DEFINE_LOG_CATEGORY(LogWebMMedia);


/**
 * Implements the WebMMedia module.
 */
class FWebMMediaModule : public IWebMMediaModule
{
public:

	//~ IWebMMediaModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		return MakeShared<FWebMMediaPlayer, ESPMode::ThreadSafe>(EventSink);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FWebMMediaModule, WebMMedia)
