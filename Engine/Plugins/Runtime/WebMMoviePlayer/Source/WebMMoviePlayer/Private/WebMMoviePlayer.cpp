// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMMovieStreamer.h"

#include "MoviePlayer.h"
#include "Modules/ModuleManager.h"

#if WITH_WEBM_LIBS
TSharedPtr<FWebMMovieStreamer> WebMMovieStreamer;
#endif // WITH_WEBM_LIBS

class FWebMMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
#if WITH_WEBM_LIBS
		FWebMMovieStreamer *Streamer = new FWebMMovieStreamer;
		WebMMovieStreamer = MakeShareable(Streamer);
		GetMoviePlayer()->RegisterMovieStreamer(WebMMovieStreamer);
#endif // WITH_WEBM_LIBS
	}

	virtual void ShutdownModule() override
	{
#if WITH_WEBM_LIBS
		WebMMovieStreamer.Reset();
#endif // WITH_WEBM_LIBS
	}
};

IMPLEMENT_MODULE(FWebMMoviePlayerModule, WebMMoviePlayer)
