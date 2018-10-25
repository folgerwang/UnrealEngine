// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "SDL.h"
#include "SDL_audio.h"
THIRD_PARTY_INCLUDES_END

class FWebMAudioBackendSDL
{
public:
	FWebMAudioBackendSDL();
	virtual ~FWebMAudioBackendSDL();

	//~ IWebMAudioBackend interface
	virtual bool InitializePlatform();
	virtual void ShutdownPlatform();
	virtual bool StartStreaming(int32 SampleRate, int32 NumOfChannels);
	virtual void StopStreaming();
	virtual bool SendAudio(const uint8* Buffer, size_t BufferSize);

private:
	SDL_AudioDeviceID AudioDevice;
	bool bSDLInitialized;
};

using FWebMAudioBackend = FWebMAudioBackendSDL;
