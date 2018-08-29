// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequenceEditorSpawnRegister.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "MovieScene.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneSequence.h"
#include "AssetData.h"
#include "Engine/Selection.h"
#include "ActorFactories/ActorFactory.h"
#include "Editor.h"
#include "ISequencer.h"
#include "LevelEditor.h"
#include "AssetSelection.h"
#include "Evaluation/MovieSceneSpawnTemplate.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Sections/MovieScene3DTransformSection.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorSpawnRegister"

/* FLevelSequenceEditorSpawnRegister structors
 *****************************************************************************/

FLevelSequenceEditorSpawnRegister::FLevelSequenceEditorSpawnRegister()
{
	bShouldClearSelectionCache = true;

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddRaw(this, &FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged);

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnObjectsReplaced);

	OnObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnObjectModified);
	OnObjectSavedHandle    = FCoreUObjectDelegates::OnObjectSaved.AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnPreObjectSaved);
#endif
}


FLevelSequenceEditorSpawnRegister::~FLevelSequenceEditorSpawnRegister()
{
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->OnPreSave().RemoveAll(this);
		Sequencer->OnActivateSequence().RemoveAll(this);
	}

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().RemoveAll(this);
	FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandle);
	FCoreUObjectDelegates::OnObjectSaved.Remove(OnObjectSavedHandle);
#endif
}


/* FLevelSequenceSpawnRegister interface
 *****************************************************************************/

UObject* FLevelSequenceEditorSpawnRegister::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);

	UObject* NewObject = FLevelSequenceSpawnRegister::SpawnObject(Spawnable, TemplateID, Player);
	
	if (AActor* NewActor = Cast<AActor>(NewObject))
	{
		// Add an entry to the modified objects map to keep track of when this object has been modified
		ModifiedObjects.Add(NewActor, FTrackedObjectState(TemplateID, Spawnable.GetGuid()));

		// Select the actor if we think it should be selected
		if (SelectedSpawnedObjects.Contains(FMovieSceneSpawnRegisterKey(TemplateID, Spawnable.GetGuid())))
		{
			GEditor->SelectActor(NewActor, true /*bSelected*/, true /*bNotify*/);
		}
	}

	return NewObject;
}


void FLevelSequenceEditorSpawnRegister::PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	UMovieSceneSequence*  Sequence      = Sequencer.IsValid() ? Sequencer->GetEvaluationTemplate().GetSequence(TemplateID) : nullptr;
	FMovieSceneSpawnable* Spawnable     = Sequence ? Sequence->GetMovieScene()->FindSpawnable(BindingId) : nullptr;
	UObject*              SpawnedObject = FindSpawnedObject(BindingId, TemplateID);

	if (SpawnedObject && Spawnable)
	{
		const FTrackedObjectState* TrackedState = ModifiedObjects.Find(&Object);
		if (TrackedState && TrackedState->bHasBeenModified)
		{
			// SaveDefaultSpawnableState will reset bHasBeenModified to false
			SaveDefaultSpawnableStateImpl(*Spawnable, Sequence, SpawnedObject, *Sequencer);

			Sequence->MarkPackageDirty();
		}
	}

	// Cache its selection state
	AActor* Actor = Cast<AActor>(&Object);
	if (Actor && GEditor->GetSelectedActors()->IsSelected(Actor))
	{
		SelectedSpawnedObjects.Add(FMovieSceneSpawnRegisterKey(TemplateID, BindingId));
		GEditor->SelectActor(Actor, false /*bSelected*/, true /*bNotify*/);
	}

	FObjectKey ThisObject(&Object);
	ModifiedObjects.Remove(ThisObject);

	FLevelSequenceSpawnRegister::PreDestroyObject(Object, BindingId, TemplateID);
}

void FLevelSequenceEditorSpawnRegister::SaveDefaultSpawnableState(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	UMovieSceneSequence* Sequence = Player.GetEvaluationTemplate().GetSequence(TemplateID);

	UObject* Object = FindSpawnedObject(Spawnable.GetGuid(), TemplateID);
	if (Object && Sequence)
	{
		SaveDefaultSpawnableStateImpl(Spawnable, Sequence, Object, Player);
		Sequence->MarkPackageDirty();
	}
}

void FLevelSequenceEditorSpawnRegister::SaveDefaultSpawnableState(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetEvaluationTemplate().GetSequence(TemplateID);
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(BindingId);

	if (Spawnable)
	{
		UObject* Object = FindSpawnedObject(Spawnable->GetGuid(), TemplateID);
		if (Object)
		{
			SaveDefaultSpawnableStateImpl(*Spawnable, Sequence, Object, *Sequencer);
			Sequence->MarkPackageDirty();
		}
	}
}

void FLevelSequenceEditorSpawnRegister::SaveDefaultSpawnableStateImpl(FMovieSceneSpawnable& Spawnable, UMovieSceneSequence* Sequence, UObject* SpawnedObject, IMovieScenePlayer& Player)
{
	auto RestorePredicate = [](FMovieSceneAnimTypeID TypeID){ return TypeID != FMovieSceneSpawnSectionTemplate::GetAnimTypeID(); };

	if (AActor* Actor = Cast<AActor>(SpawnedObject))
	{
		// Restore state on any components
		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(Actor))
		{
			if (Component)
			{
				Player.RestorePreAnimatedState(*Component, RestorePredicate);
			}
		}
	}

	// Restore state on the object itself
	Player.RestorePreAnimatedState(*SpawnedObject, RestorePredicate);

	// Copy the template
	Spawnable.CopyObjectTemplate(*SpawnedObject, *Sequence);

	if (FTrackedObjectState* TrackedState = ModifiedObjects.Find(SpawnedObject))
	{
		TrackedState->bHasBeenModified = false;
	}
}

/* FLevelSequenceEditorSpawnRegister implementation
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::SetSequencer(const TSharedPtr<ISequencer>& Sequencer)
{
	WeakSequencer = Sequencer;
}


/* FLevelSequenceEditorSpawnRegister callbacks
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (bShouldClearSelectionCache)
	{
		SelectedSpawnedObjects.Reset();
	}
}

void FLevelSequenceEditorSpawnRegister::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	for (auto& Pair : Register)
	{
		TWeakObjectPtr<>& WeakObject = Pair.Value.Object;
		UObject* SpawnedObject = WeakObject.Get();
		if (UObject* NewObject = OldToNewInstanceMap.FindRef(SpawnedObject))
		{
			// Reassign the object
			WeakObject = NewObject;

			// It's a spawnable, so ensure it's transient
			NewObject->SetFlags(RF_Transient);

			// Invalidate the binding - it will be resolved if it's ever asked for again
			Sequencer->State.Invalidate(Pair.Key.BindingId, Pair.Key.TemplateID);
		}
	}
}

void FLevelSequenceEditorSpawnRegister::OnObjectModified(UObject* ModifiedObject)
{
	FTrackedObjectState* TrackedState = ModifiedObjects.Find(ModifiedObject);
	while (!TrackedState && ModifiedObject)
	{
		TrackedState = ModifiedObjects.Find(ModifiedObject);
		ModifiedObject = ModifiedObject->GetOuter();
	}

	if (TrackedState)
	{
		TrackedState->bHasBeenModified = true;

		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		UMovieSceneSequence*   OwningSequence = Sequencer.IsValid() ? Sequencer->GetEvaluationTemplate().GetSequence(TrackedState->TemplateID) : nullptr;
		if (OwningSequence)
		{
			OwningSequence->MarkPackageDirty();
			SequencesWithModifiedObjects.Add(OwningSequence);
		}
	}
}

void FLevelSequenceEditorSpawnRegister::OnPreObjectSaved(UObject* Object)
{
	UMovieSceneSequence* SequenceBeingSaved = Cast<UMovieSceneSequence>(Object);
	if (SequenceBeingSaved && SequencesWithModifiedObjects.Contains(SequenceBeingSaved))
	{
		UMovieScene* MovieSceneBeingSaved = SequenceBeingSaved->GetMovieScene();

		// The object being saved is a movie scene sequence that we've tracked as having a modified spawnable in the world.
		// We need to go through all templates in the current sequence that reference this sequence, saving default state
		// for any spawned objects that have been modified.
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

		if (Sequencer.IsValid())
		{
			TArray<FObjectKey> ObjectsThatHaveBeenSaved;

			for (const TTuple<FObjectKey, FTrackedObjectState>& Pair : ModifiedObjects)
			{
				UObject* SpawnedObject = Pair.Key.ResolveObjectPtr();
				UMovieSceneSequence*  ThisSequence = Sequencer->GetEvaluationTemplate().GetSequence(Pair.Value.TemplateID);
				FMovieSceneSpawnable* Spawnable    = MovieSceneBeingSaved->FindSpawnable(Pair.Value.ObjectBindingID);

				if (SpawnedObject && Spawnable && ThisSequence == SequenceBeingSaved)
				{
					SaveDefaultSpawnableStateImpl(*Spawnable, ThisSequence, SpawnedObject, *Sequencer);

					ObjectsThatHaveBeenSaved.Add(Pair.Key);
				}
			}

			for (FObjectKey ObjectKey : ObjectsThatHaveBeenSaved)
			{
				ModifiedObjects.Remove(ObjectKey);
			}
		}
	}
}

#if WITH_EDITOR

TValueOrError<FNewSpawnable, FText> FLevelSequenceEditorSpawnRegister::CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene, UActorFactory* ActorFactory)
{
	for (TSharedPtr<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		TValueOrError<FNewSpawnable, FText> Result = MovieSceneObjectSpawner->CreateNewSpawnableType(SourceObject, OwnerMovieScene, ActorFactory);
		if (Result.IsValid())
		{
			return Result;
		}
	}

	return MakeError(LOCTEXT("NoSpawnerFound", "No spawner found to create new spawnable type"));
}

void FLevelSequenceEditorSpawnRegister::SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const TOptional<FTransformData>& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings)
{
	for (TSharedPtr<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		if (MovieSceneObjectSpawner->CanSetupDefaultsForSpawnable(SpawnedObject))
		{
			MovieSceneObjectSpawner->SetupDefaultsForSpawnable(SpawnedObject, Guid, TransformData, Sequencer, Settings);
			return;
		}
	}
}

void FLevelSequenceEditorSpawnRegister::HandleConvertPossessableToSpawnable(UObject* OldObject, IMovieScenePlayer& Player, TOptional<FTransformData>& OutTransformData)
{
	// @TODO: this could probably be handed off to a spawner if we need anything else to be convertible between spawnable/posessable

	AActor* OldActor = Cast<AActor>(OldObject);
	if (OldActor)
	{
		OutTransformData.Emplace();
		OutTransformData->Translation = OldActor->GetActorLocation();
		OutTransformData->Rotation = OldActor->GetActorRotation();
		OutTransformData->Scale = OldActor->GetActorScale();

		GEditor->SelectActor(OldActor, false, true);
		UWorld* World = Cast<UWorld>(Player.GetPlaybackContext());
		if (World)
		{
			World->EditorDestroyActor(OldActor, true);

			GEditor->BroadcastLevelActorListChanged();
		}
	}
}

bool FLevelSequenceEditorSpawnRegister::CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const
{
	for (TSharedPtr<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		if (Spawnable.GetObjectTemplate()->IsA(MovieSceneObjectSpawner->GetSupportedTemplateType()))
		{
			return MovieSceneObjectSpawner->CanConvertSpawnableToPossessable(Spawnable);
		}
	}

	return false;
}

#endif


#undef LOCTEXT_NAMESPACE
