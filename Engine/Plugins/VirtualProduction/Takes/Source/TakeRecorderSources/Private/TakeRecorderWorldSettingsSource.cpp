// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderWorldSettingsSource.h"

#include "TakeRecorderSources.h"
#include "TakeRecorderActorSource.h"
#include "TakeRecorderSourcesUtils.h"
#include "TakesUtils.h"

#include "LevelSequence.h"
#include "GameFramework/WorldSettings.h"

UTakeRecorderWorldSettingsSource::UTakeRecorderWorldSettingsSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	TrackTint = FColor(129, 129, 129);
}

TArray<UTakeRecorderSource*> UTakeRecorderWorldSettingsSource::PreRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer)
{
	TArray<UTakeRecorderSource*> NewSources;

	UTakeRecorderSources* Sources = InSequence->FindOrAddMetaData<UTakeRecorderSources>();

	// Get the first PIE world's world settings
	UWorld* World = TakeRecorderSourcesUtils::GetSourceWorld(InSequence);

	if (!World)
	{
		return NewSources;
	}

	AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;

	if (!WorldSettings)
	{
		return NewSources;
	}

	for (auto Source : Sources->GetSources())
	{
		if (Source->IsA<UTakeRecorderActorSource>())
		{
			UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source);
			if (ActorSource->Target.IsValid())
			{
				if (ActorSource->Target.Get() == WorldSettings)
				{
					return NewSources;
				}
			}
		}
	}

	UTakeRecorderActorSource* ActorSource = NewObject<UTakeRecorderActorSource>(Sources, UTakeRecorderActorSource::StaticClass(), NAME_None, RF_Transactional);
	ActorSource->Target = WorldSettings;
	NewSources.Add(ActorSource);

	WorldSettingsSource = ActorSource;

	return NewSources;
}

TArray<UTakeRecorderSource*> UTakeRecorderWorldSettingsSource::PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence)
{
	TArray<UTakeRecorderSource*> SourcesToRemove;

	if (WorldSettingsSource.IsValid())
	{
		SourcesToRemove.Add(WorldSettingsSource.Get());
	}
	return SourcesToRemove;
}

FText UTakeRecorderWorldSettingsSource::GetDisplayTextImpl() const
{
	return NSLOCTEXT("UTakeRecorderWorldSettingsSource", "Label", "World Settings");
}

bool UTakeRecorderWorldSettingsSource::CanAddSource(UTakeRecorderSources* InSources) const
{
	for (UTakeRecorderSource* Source : InSources->GetSources())
	{
		if (Source->IsA<UTakeRecorderWorldSettingsSource>())
		{
			return false;
		}
	}
	return true;
}