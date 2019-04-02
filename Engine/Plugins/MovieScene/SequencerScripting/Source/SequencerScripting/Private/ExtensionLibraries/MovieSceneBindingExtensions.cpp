// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneBindingExtensions.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"
#include "SequencerBindingProxy.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Algo/Find.h"

bool UMovieSceneBindingExtensions::IsValid(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		return Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid) != nullptr;
	}

	return false;
}

FGuid UMovieSceneBindingExtensions::GetId(const FSequencerBindingProxy& InBinding)
{
	return InBinding.BindingID;
}

FText UMovieSceneBindingExtensions::GetDisplayName(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		return MovieScene->GetObjectDisplayName(InBinding.BindingID);
	}

	return FText();
}

FString UMovieSceneBindingExtensions::GetName(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	if (MovieScene && InBinding.BindingID.IsValid())
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			return Spawnable->GetName();
		}

		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			return Possessable->GetName();
		}
	}

	return FString();
}

TArray<UMovieSceneTrack*> UMovieSceneBindingExtensions::GetTracks(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid);
		if (Binding)
		{
			return Binding->GetTracks();
		}
	}
	return TArray<UMovieSceneTrack*>();
}

void UMovieSceneBindingExtensions::RemoveTrack(const FSequencerBindingProxy& InBinding, UMovieSceneTrack* TrackToRemove)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (TrackToRemove && MovieScene)
	{
		MovieScene->RemoveTrack(*TrackToRemove);
	}
}

void UMovieSceneBindingExtensions::Remove(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		if (!MovieScene->RemovePossessable(InBinding.BindingID))
		{
			MovieScene->RemoveSpawnable(InBinding.BindingID);
		}
	}
}

TArray<UMovieSceneTrack*> UMovieSceneBindingExtensions::FindTracksByType(const FSequencerBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = InBinding.GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid);
		if (Binding)
		{
			bool bExactMatch = false;
			return UMovieSceneSequenceExtensions::FilterTracks(Binding->GetTracks(), DesiredClass, bExactMatch);
		}
	}
	return TArray<UMovieSceneTrack*>();
}

TArray<UMovieSceneTrack*> UMovieSceneBindingExtensions::FindTracksByExactType(const FSequencerBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = InBinding.GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid);
		if (Binding)
		{
			bool bExactMatch = true;
			return UMovieSceneSequenceExtensions::FilterTracks(Binding->GetTracks(), DesiredClass, bExactMatch);
		}
	}
	return TArray<UMovieSceneTrack*>();
}

UMovieSceneTrack* UMovieSceneBindingExtensions::AddTrack(const FSequencerBindingProxy& InBinding, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = InBinding.GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene)
	{
		const bool bBindingExists = Algo::FindBy(MovieScene->GetBindings(), InBinding.BindingID, &FMovieSceneBinding::GetObjectGuid) != nullptr;
		if (bBindingExists)
		{
			UMovieSceneTrack* NewTrack = NewObject<UMovieSceneTrack>(MovieScene, DesiredClass, NAME_None, RF_Transactional);
			if (NewTrack)
			{
				MovieScene->AddGivenTrack(NewTrack, InBinding.BindingID);
				return NewTrack;
			}
		}
	}
	return nullptr;
}

TArray<FSequencerBindingProxy> UMovieSceneBindingExtensions::GetChildPossessables(const FSequencerBindingProxy& InBinding)
{
	TArray<FSequencerBindingProxy> Result;

	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			for (const FGuid& ChildGuid : Spawnable->GetChildPossessables())
			{
				Result.Emplace(ChildGuid, InBinding.Sequence);
			}
			return Result;
		}

		const int32 Count = MovieScene->GetPossessableCount();
		for (int32 i = 0; i < Count; ++i)
		{
			FMovieScenePossessable& PossessableChild = MovieScene->GetPossessable(i);
			if (PossessableChild.GetParent() == InBinding.BindingID)
			{
				Result.Emplace(PossessableChild.GetGuid(), InBinding.Sequence);
			}
		}
	}
	return Result;
}

UObject* UMovieSceneBindingExtensions::GetObjectTemplate(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(InBinding.BindingID);
		if (Spawnable)
		{
			return Spawnable->GetObjectTemplate();
		}
	}
	return nullptr;
}

UClass* UMovieSceneBindingExtensions::GetPossessedObjectClass(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			return const_cast<UClass*>(Possessable->GetPossessedObjectClass());
		}
	}
	return nullptr;
}

FSequencerBindingProxy UMovieSceneBindingExtensions::GetParent(const FSequencerBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			return FSequencerBindingProxy(Possessable->GetParent(), InBinding.Sequence);
		}
	}
	return FSequencerBindingProxy();
}


void UMovieSceneBindingExtensions::SetParent(const FSequencerBindingProxy& InBinding, const FSequencerBindingProxy& InParentBinding)
{
	UMovieScene* MovieScene = InBinding.GetMovieScene();
	if (MovieScene)
	{
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(InBinding.BindingID);
		if (Possessable)
		{
			Possessable->SetParent(InParentBinding.BindingID);
		}
	}
}
