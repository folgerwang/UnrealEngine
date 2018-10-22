// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMAudioBackend.h"
#include "WebMMovieCommon.h"

FWebMAudioBackendSDL::FWebMAudioBackendSDL()
	: AudioDevice(0)
	, bSDLInitialized(false)
{
}

FWebMAudioBackendSDL::~FWebMAudioBackendSDL()
{
	ShutdownPlatform();
}

bool FWebMAudioBackendSDL::InitializePlatform()
{
	int32 Result = SDL_InitSubSystem(SDL_INIT_AUDIO);
	if (Result < 0)
	{
		UE_LOG(LogWebMMoviePlayer, Error, TEXT("SDL_InitSubSystem create failed: %d"), Result);
		bSDLInitialized = false;
		return false;
	}
	else
	{
		bSDLInitialized = true;
		return true;
	}
}

void FWebMAudioBackendSDL::ShutdownPlatform()
{
	StopStreaming();

	if (bSDLInitialized)
	{
		// this is refcounted
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		bSDLInitialized = false;
	}
}

bool FWebMAudioBackendSDL::StartStreaming(int32 SampleRate, int32 NumOfChannels)
{
	if (!bSDLInitialized)
	{
		return false;
	}

	SDL_AudioSpec AudioSpec;
	memset(&AudioSpec, 0, sizeof(SDL_AudioSpec));
	AudioSpec.freq = SampleRate;
	AudioSpec.format = AUDIO_S16;
	AudioSpec.channels = NumOfChannels;
	AudioSpec.samples = 4096;

	AudioDevice = SDL_OpenAudioDevice(nullptr, 0, &AudioSpec, nullptr, 0);

	if (!AudioDevice)
	{
		UE_LOG(LogWebMMoviePlayer, Error, TEXT("SDL_OpenAudioDevice failed"));
		return false;
	}
	else
	{
		return true;
	}
}

void FWebMAudioBackendSDL::StopStreaming()
{
	if (AudioDevice)
	{
		SDL_CloseAudioDevice(AudioDevice);
		AudioDevice = 0;
	}
}

bool FWebMAudioBackendSDL::SendAudio(const uint8* Buffer, size_t BufferSize)
{
	int32 Result = SDL_QueueAudio(AudioDevice, Buffer, BufferSize);
	if (Result < 0)
	{
		UE_LOG(LogWebMMoviePlayer, Error, TEXT("SDL_QueueAudio failed: %d"), Result);
		return false;
	}
	else
	{
		SDL_PauseAudioDevice(AudioDevice, 0);
		return true;
	}
}
