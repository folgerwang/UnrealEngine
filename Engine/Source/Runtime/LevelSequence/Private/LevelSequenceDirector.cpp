// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceDirector.h"
#include "Engine/World.h"

UWorld* ULevelSequenceDirector::GetWorld() const
{
	if (ULevel* OuterLevel = GetTypedOuter<ULevel>())
	{
		return OuterLevel->OwningWorld;
	}
	return GetTypedOuter<UWorld>();
}
