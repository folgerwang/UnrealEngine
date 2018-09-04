// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceDirectorGeneratedClass.h"
#include "LevelSequencePlayer.h"

UWorld* ULevelSequenceDirector::GetWorld() const
{
	return GetTypedOuter<UWorld>();
}
