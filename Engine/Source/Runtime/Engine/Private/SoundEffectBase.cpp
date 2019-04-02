// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundEffectBase.h"


FSoundEffectBase::FSoundEffectBase()
	: bChanged(false)
	, Preset(nullptr)
	, bIsRunning(false)
	, bIsActive(false)
{}

FSoundEffectBase::~FSoundEffectBase()
{
}

bool FSoundEffectBase::IsActive() const
{ 
	return bIsActive; 
}

void FSoundEffectBase::SetEnabled(const bool bInIsEnabled)
{ 
	bIsActive = bInIsEnabled; 
}

void FSoundEffectBase::SetPreset(USoundEffectPreset* Inpreset)
{
	if (Preset != Inpreset)
	{
		ClearPreset();

		Preset = Inpreset;
		if (Preset)
		{
			Preset->AddEffectInstance(this);
			bChanged = true;
		}
	}
}

void FSoundEffectBase::ClearPreset()
{
	if (Preset)
	{
		Preset->RemoveEffectInstance(this);
		Preset = nullptr;
	}
}

void FSoundEffectBase::Update()
{
	PumpPendingMessages();

	if (bChanged && Preset)
	{
		OnPresetChanged();
		bChanged = false;
	}
}

bool FSoundEffectBase::IsPreset(USoundEffectPreset* InPreset) const
{
	return Preset == InPreset;
}

void FSoundEffectBase::EffectCommand(TFunction<void()> Command)
{
	CommandQueue.Enqueue(MoveTemp(Command));
}
void FSoundEffectBase::PumpPendingMessages()
{
	// Pumps the command queue
	TFunction<void()> Command;
	while (CommandQueue.Dequeue(Command))
	{
		Command();
	}
}
