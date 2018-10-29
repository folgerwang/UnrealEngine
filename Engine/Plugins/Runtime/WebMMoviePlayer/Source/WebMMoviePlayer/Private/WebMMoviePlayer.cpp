// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMMovieStreamer.h"

#include "MoviePlayer.h"
#include "Modules/ModuleManager.h"

TSharedPtr<FWebMMovieStreamer> WebMMovieStreamer;

class FWebMMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FWebMMovieStreamer *Streamer = new FWebMMovieStreamer;
		WebMMovieStreamer = MakeShareable(Streamer);
		GetMoviePlayer()->RegisterMovieStreamer(WebMMovieStreamer);
	}

	virtual void ShutdownModule() override
	{
		WebMMovieStreamer.Reset();
	}
};

IMPLEMENT_MODULE(FWebMMoviePlayerModule, WebMMoviePlayer)
