// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioEncoder.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Engine/GameEngine.h"
#include "Streamer.h"
#include "PixelStreamingCommon.h"

FAudioEncoder::FAudioEncoder(FStreamer& Outer)
	: Outer(Outer)
	, bInitialized(false)
	, bFormatChecked(false)
{
}

FAudioEncoder::~FAudioEncoder()
{
	if (bInitialized)
	{
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			FAudioDevice* AudioDevice = GameEngine->GetMainAudioDevice();
			if (AudioDevice)
			{
				AudioDevice->UnregisterSubmixBufferListener(this);
			}
		}
	}
}

void FAudioEncoder::Init()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("AudioMixer")))
	{
		UE_LOG(PixelStreaming, Warning, TEXT("No audio supported. Needs -audiomixer parameter"));
		return;
	}

	if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		FAudioDevice* AudioDevice = GameEngine->GetMainAudioDevice();
		if (AudioDevice)
		{
			AudioDevice->RegisterSubmixBufferListener(this);
			bInitialized = true;
		}
	}
}

void FAudioEncoder::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	if (!bInitialized)
	{
		return;
	}

	// Only 48000hz supported for now
	if (SampleRate != 48000)
	{
		// Only report the problem once
		if (!bFormatChecked)
		{
			bFormatChecked = true;
			UE_LOG(PixelStreaming, Warning, TEXT("Audio samplerate needs to be 48000hz"));
		}
		return;
	}

	Audio::TSampleBuffer<float> Buffer(AudioData, NumSamples, NumChannels, SampleRate);
	// Mix to stereo if required, since PixelStreaming only accept stereo at the moment
	if (Buffer.GetNumChannels() != 2)
	{
		Buffer.MixBufferToChannels(2);
	}

	// Convert to signed PCM 16-bits
	PCM16.Reset(Buffer.GetNumSamples());
	PCM16.AddZeroed(Buffer.GetNumSamples());
	const float* Ptr = Buffer.GetData();
	for (int16& S : PCM16)
	{
		int32 N = *Ptr >= 0 ? *Ptr * int32(MAX_int16) : *Ptr * (int32(MAX_int16)+1);
		S = static_cast<int16>(FMath::Clamp(N, int32(MIN_int16), int32(MAX_int16)));
		Ptr++;
	}

	Outer.OnAudioPCMPacketReady(reinterpret_cast<const uint8*>(PCM16.GetData()), PCM16.Num() * sizeof(int16));
}
