// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheTracksModule.h"
#if WITH_EDITOR
#include "GeometryCacheSequencerModule.h"
#endif // WITH_EDITOR

IMPLEMENT_MODULE(FGeometryCacheTracksModule, GeometryCacheTracks)

void FGeometryCacheTracksModule::StartupModule()
{
#if WITH_EDITOR
	FGeometryCacheSequencerModule& Module = FModuleManager::LoadModuleChecked<FGeometryCacheSequencerModule>(TEXT("GeometryCacheSequencer"));
#endif
}

void FGeometryCacheTracksModule::ShutdownModule()
{
}
