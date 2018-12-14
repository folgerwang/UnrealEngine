// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PreLoadMoviePlayerModuleBase.h"

#include "MoviePlayer.h"
#include "PreLoadScreenManager.h"
#include "PreLoadMoviePlayerScreenBase.h"


void FPreLoadMoviePlayerScreenModuleBase::RegisterMovieStreamer(TSharedPtr<class IMovieStreamer> InMovieStreamer)
{
    if (MoviePreLoadScreen.IsValid())
    {
        //Pass the streamer to our screen and then init with this new information
        MoviePreLoadScreen->RegisterMovieStreamer(InMovieStreamer);
        MoviePreLoadScreen->Init();
    }
}

void FPreLoadMoviePlayerScreenModuleBase::UnRegisterMovieStreamer(TSharedPtr<class IMovieStreamer> InMovieStreamer)
{
    CleanUpMovieStreamer();
}

void FPreLoadMoviePlayerScreenModuleBase::CleanUpMovieStreamer()
{
    if (MoviePreLoadScreen.IsValid())
    {
        MoviePreLoadScreen->CleanUp();
    }

    MoviePreLoadScreen.Reset();
}

void FPreLoadMoviePlayerScreenModuleBase::StartupModule()
{
    MoviePreLoadScreen = MakeShareable(new FPreLoadMoviePlayerScreenBase());
    
    if (FPreLoadScreenManager::Get())
    {
        FPreLoadScreenManager::Get()->RegisterPreLoadScreen(MoviePreLoadScreen);
    }

    FCoreDelegates::RegisterMovieStreamerDelegate.AddRaw(this, &FPreLoadMoviePlayerScreenModuleBase::RegisterMovieStreamer);
    FCoreDelegates::UnRegisterMovieStreamerDelegate.AddRaw(this, &FPreLoadMoviePlayerScreenModuleBase::UnRegisterMovieStreamer);
}

void FPreLoadMoviePlayerScreenModuleBase::ShutdownModule()
{
    CleanUpMovieStreamer();
}

IMPLEMENT_MODULE(FPreLoadMoviePlayerScreenModuleBase, PreLoadScreenMoviePlayer);
