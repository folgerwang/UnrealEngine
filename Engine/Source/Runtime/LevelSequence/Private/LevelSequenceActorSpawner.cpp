// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceActorSpawner.h"
#include "MovieSceneSpawnable.h"
#include "IMovieScenePlayer.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"

static const FName SequencerActorTag(TEXT("SequencerActor"));

TSharedRef<IMovieSceneObjectSpawner> FLevelSequenceActorSpawner::CreateObjectSpawner()
{
	return MakeShareable(new FLevelSequenceActorSpawner);
}

UClass* FLevelSequenceActorSpawner::GetSupportedTemplateType() const
{
	return AActor::StaticClass();
}

ULevelStreaming* GetLevelStreaming(FString SafeLevelName, UWorld& World)
{
	if (FPackageName::IsShortPackageName(SafeLevelName))
	{
		// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
		SafeLevelName = TEXT("/") + SafeLevelName;
	}

	for (ULevelStreaming* LevelStreaming : World.GetStreamingLevels())
	{
		if (LevelStreaming && LevelStreaming->GetWorldAssetPackageName().EndsWith(SafeLevelName, ESearchCase::IgnoreCase))
		{
			return LevelStreaming;
		}
	}

	return nullptr;
}

UObject* FLevelSequenceActorSpawner::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	AActor* ObjectTemplate = Cast<AActor>(Spawnable.GetObjectTemplate());
	if (!ObjectTemplate)
	{
		return nullptr;
	}

	const EObjectFlags ObjectFlags = RF_Transient;

	// @todo sequencer livecapture: Consider using SetPlayInEditorWorld() and RestoreEditorWorld() here instead
	
	// @todo sequencer actors: We need to make sure puppet objects aren't copied into PIE/SIE sessions!  They should be omitted from that duplication!

	UWorld* WorldContext = Cast<UWorld>(Player.GetPlaybackContext());
	if(WorldContext == nullptr)
	{
		WorldContext = GWorld;
	}

	ULevelStreaming* LevelStreaming = GetLevelStreaming(Spawnable.GetLevelName().ToString(), *WorldContext);
	if (LevelStreaming && LevelStreaming->GetWorldAsset().IsValid())
	{
		WorldContext = LevelStreaming->GetWorldAsset().Get();
	}
	else if (Spawnable.GetLevelName() != NAME_None)
	{
		UE_LOG(LogMovieScene, Warning, TEXT("Can't find sublevel '%s' to spawn '%s' into"), *Spawnable.GetLevelName().ToString(), *Spawnable.GetName());
	}

	// Construct the object with the same name that we will set later on the actor to avoid renaming it inside SetActorLabel
	FName SpawnName =
#if WITH_EDITOR
		MakeUniqueObjectName(WorldContext->PersistentLevel, ObjectTemplate->GetClass(), *Spawnable.GetName());
#else
		NAME_None;
#endif

	// Spawn the puppet actor
	FActorSpawnParameters SpawnInfo;
	{
		SpawnInfo.Name = SpawnName;
		SpawnInfo.ObjectFlags = ObjectFlags;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// @todo: Spawning with a non-CDO template is fraught with issues
		//SpawnInfo.Template = ObjectTemplate;
		// allow pre-construction variables to be set.
		SpawnInfo.bDeferConstruction = true;
		SpawnInfo.Template = ObjectTemplate;
		SpawnInfo.OverrideLevel = WorldContext->PersistentLevel;
	}

	//Chaos - Avoiding crash in UWorld::SendAllEndOfFrameUpdates due to duplicating template components/re-runing the construction script on a fully formed hierarchy
	ObjectTemplate->DestroyConstructedComponents();

	FTransform SpawnTransform;

	if (USceneComponent* RootComponent = ObjectTemplate->GetRootComponent())
	{
		SpawnTransform.SetTranslation(RootComponent->RelativeLocation);
		SpawnTransform.SetRotation(RootComponent->RelativeRotation.Quaternion());
	}
	else
	{
		SpawnTransform = Spawnable.SpawnTransform;
	}

	{
		// Disable all particle components so that they don't auto fire as soon as the actor is spawned. The particles should be triggered through the particle track.
		TArray<UActorComponent*> ParticleComponents = ObjectTemplate->GetComponentsByClass(UParticleSystemComponent::StaticClass());
		for (int32 ComponentIdx = 0; ComponentIdx < ParticleComponents.Num(); ++ComponentIdx)
		{
			ParticleComponents[ComponentIdx]->bAutoActivate = false;
		}
	}

	AActor* SpawnedActor = WorldContext->SpawnActorAbsolute(ObjectTemplate->GetClass(), SpawnTransform, SpawnInfo);
	if (!SpawnedActor)
	{
		return nullptr;
	}
	
	//UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	//CopyParams.bPreserveRootComponent = false;
	//CopyParams.bNotifyObjectReplacement = false;
	//SpawnedActor->UnregisterAllComponents();
	//UEngine::CopyPropertiesForUnrelatedObjects(ObjectTemplate, SpawnedActor, CopyParams);
	//SpawnedActor->RegisterAllComponents();

	// Ensure this spawnable is not a preview actor. Preview actors will not have BeginPlay() called on them.
#if WITH_EDITOR
	SpawnedActor->bIsEditorPreviewActor = false;
#endif

	// tag this actor so we know it was spawned by sequencer
	SpawnedActor->Tags.AddUnique(SequencerActorTag);

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly set RF_Transactional on spawned actors so we can undo/redo properties on them. We don't add this as a spawn flag since we don't want to transact spawn/destroy events.
		SpawnedActor->SetFlags(RF_Transactional);

		for (UActorComponent* Component : SpawnedActor->GetComponents())
		{
			if (Component)
			{
				Component->SetFlags(RF_Transactional);
			}
		}
	}	
#endif

	const bool bIsDefaultTransform = true;
	SpawnedActor->FinishSpawning(SpawnTransform, bIsDefaultTransform);

#if WITH_EDITOR
	if (GIsEditor)
	{
		SpawnedActor->SetActorLabel(Spawnable.GetName());
	}
#endif

	return SpawnedActor;
}

void FLevelSequenceActorSpawner::DestroySpawnedObject(UObject& Object)
{
	AActor* Actor = Cast<AActor>(&Object);
	if (!ensure(Actor))
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly remove RF_Transactional on spawned actors since we don't want to trasact spawn/destroy events
		Actor->ClearFlags(RF_Transactional);
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component)
			{
				Component->ClearFlags(RF_Transactional);
			}
		}
	}
#endif

	UWorld* World = Actor->GetWorld();
	if (ensure(World))
	{
		const bool bNetForce = false;
		const bool bShouldModifyLevel = false;
		World->DestroyActor(Actor, bNetForce, bShouldModifyLevel);
	}
}
