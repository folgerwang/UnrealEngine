// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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