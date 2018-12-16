// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"

UGameInstanceSubsystem::UGameInstanceSubsystem()
	: USubsystem()
{

}

UGameInstance* UGameInstanceSubsystem::GetGameInstance() const
{
	return Cast<UGameInstance>(GetOuter());
}