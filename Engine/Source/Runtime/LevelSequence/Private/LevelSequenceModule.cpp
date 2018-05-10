// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceModule.h"
#include "Modules/ModuleManager.h"
#include "LevelSequenceActorSpawner.h"

void FLevelSequenceModule::StartupModule()
{
	OnCreateMovieSceneObjectSpawnerDelegateHandle = RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FLevelSequenceActorSpawner::CreateObjectSpawner));
}

void FLevelSequenceModule::ShutdownModule()
{
	UnregisterObjectSpawner(OnCreateMovieSceneObjectSpawnerDelegateHandle);
}

FDelegateHandle FLevelSequenceModule::RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner InOnCreateMovieSceneObjectSpawner)
{
	OnCreateMovieSceneObjectSpawnerDelegates.Add(InOnCreateMovieSceneObjectSpawner);
	return OnCreateMovieSceneObjectSpawnerDelegates.Last().GetHandle();
}

void FLevelSequenceModule::UnregisterObjectSpawner(FDelegateHandle InHandle)
{
	OnCreateMovieSceneObjectSpawnerDelegates.RemoveAll([=](const FOnCreateMovieSceneObjectSpawner& Delegate) { return Delegate.GetHandle() == InHandle; });
}

void FLevelSequenceModule::GenerateObjectSpawners(TArray<TSharedRef<IMovieSceneObjectSpawner>>& OutSpawners) const
{
	for (const FOnCreateMovieSceneObjectSpawner& SpawnerFactory : OnCreateMovieSceneObjectSpawnerDelegates)
	{
		check(SpawnerFactory.IsBound());
		OutSpawners.Add(SpawnerFactory.Execute());
	}

	// Now sort the spawners. Editor spawners should come first so they override runtime versions of the same supported type in-editor.
	// @TODO: we could also sort by most-derived type here to allow for type specific behaviors
	OutSpawners.Sort([](TSharedRef<IMovieSceneObjectSpawner> LHS, TSharedRef<IMovieSceneObjectSpawner> RHS)
	{
		return LHS->IsEditor() > RHS->IsEditor();
	});
}

IMPLEMENT_MODULE(FLevelSequenceModule, LevelSequence);
