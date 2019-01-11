// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieScene.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

#include "TakeRecorderActorSource.h"
#include "TakeRecorderSources.h"
#include "TakesUtils.h"

#include "LevelSequence.h"

class UWorld;
class ULevelSequence;
class UMovieScene;

namespace TakeRecorderSourcesUtils
{
	/*
	* Get the first PIE world, or first world from the actor sources, or the first world
	*/
	static UWorld* GetSourceWorld(ULevelSequence* InSequence)
	{
		// Get the first PIE world's world settings
		UWorld* World = TakesUtils::GetFirstPIEWorld();

		if (World)
		{
			return World;
		}

		// Otherwise any of the source's worlds
		UTakeRecorderSources* Sources = InSequence->FindOrAddMetaData<UTakeRecorderSources>();
		for (auto Source : Sources->GetSources())
		{
			if (Source->IsA<UTakeRecorderActorSource>())
			{
				UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source);
				if (ActorSource->Target.IsValid())
				{
					World = ActorSource->Target.Get()->GetWorld();
					if (World)
					{
						return World;
					}
				}
			}
		}

		// Otherwise, get the first world?
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			World = Context.World();
			if (World)
			{
				return World;
			}
		}

		return nullptr;
	}
}