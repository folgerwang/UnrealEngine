// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ActorLayerUtilities.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"

TArray<AActor*> ULayersBlueprintLibrary::GetActors(UObject* WorldContextObject, const FActorLayer& ActorLayer)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return TArray<AActor*>();
	}

	TArray<AActor*> AllActors;

	// Iterate over all actors, looking for actors in the specified layers.
	for (const TWeakObjectPtr<AActor>& WeakActor : FActorRange(World))
	{
		AActor* Actor = WeakActor.Get();
		if (Actor && Actor->Layers.Contains(ActorLayer.Name))
		{
			AllActors.Add(Actor);
		}
	}

	return AllActors;
}

IMPLEMENT_MODULE(FDefaultModuleImpl, ActorLayerUtilities)