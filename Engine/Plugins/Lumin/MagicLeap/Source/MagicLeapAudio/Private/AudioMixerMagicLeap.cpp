// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformMagicLeap.h"


class FAudioMixerModuleMagicLeap : public IAudioDeviceModule
{
public:
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformMagicLeap());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleMagicLeap, MagicLeapAudio);
