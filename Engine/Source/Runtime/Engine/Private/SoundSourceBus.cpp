// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundSourceBus.h"
#include "AudioDeviceManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "ActiveSound.h"

USoundSourceBus::USoundSourceBus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// This is a bus. This will result in the decompression type to be set as DTYPE_Bus. Audio won't be generated from this object but from instance data in audio mixer.
	bIsBus = true;

	Init();
}

void USoundSourceBus::PostLoad()
{
	Super::PostLoad();

	Init();
}

void USoundSourceBus::Init()
{
	// Allow users to manually set the source bus duration
	Duration = GetDuration();

	// This sound wave is looping if the source bus duration is 0.0f
	bLooping = (SourceBusDuration == 0.0f);

	// Keep playing this bus when the volume is 0
	// Note source buses can't ever be truly virtual as they are procedurally generated.
	bVirtualizeWhenSilent = !bAutoDeactivateWhenSilent;

	// Set the channels equal to the users channel count choice
	switch (SourceBusChannels)
	{
	case ESourceBusChannels::Mono:
		NumChannels = 1;
		break;

	case ESourceBusChannels::Stereo:
		NumChannels = 2;
		break;
	}
}

#if WITH_EDITOR
void USoundSourceBus::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Init();
}
#endif

bool USoundSourceBus::IsPlayable() const
{
	return true;
}

float USoundSourceBus::GetDuration()
{
	return (SourceBusDuration > 0.0f) ? SourceBusDuration : INDEFINITELY_LOOPING_DURATION;
}

