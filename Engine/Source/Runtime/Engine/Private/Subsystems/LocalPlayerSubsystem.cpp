// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/LocalPlayerSubsystem.h"
#include "Engine/LocalPlayer.h"

ULocalPlayerSubsystem::ULocalPlayerSubsystem()
	: USubsystem()
{

}

ULocalPlayer* ULocalPlayerSubsystem::GetLocalPlayer() const
{
	return Cast<ULocalPlayer>(GetOuter());
}

