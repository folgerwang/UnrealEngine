// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreLoadMoviePlayerModule.h"
#include "PreLoadMoviePlayerScreenBase.h"

class FPreLoadMoviePlayerScreenModuleBase : public IPreLoadMoviePlayerScreenModule
{
public:

    //IPreLoadMoviePlayerScreenModule Interface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    virtual bool IsGameModule() const override
    {
        return true;
    }

    virtual void RegisterMovieStreamer(TSharedPtr<class IMovieStreamer> InMovieStreamer);
    virtual void UnRegisterMovieStreamer(TSharedPtr<class IMovieStreamer> InMovieStreamer);
    virtual void CleanUpMovieStreamer();

private:
    TSharedPtr<FPreLoadMoviePlayerScreenBase> MoviePreLoadScreen;
};
