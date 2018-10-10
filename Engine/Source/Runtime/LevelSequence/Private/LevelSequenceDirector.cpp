// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceDirector.h"
#include "Engine/World.h"

UWorld* ULevelSequenceDirector::GetWorld() const
{
	return GetTypedOuter<UWorld>();
}
