// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Algo/Find.h"

TArray<UMovieSceneTrack*> UMovieSceneSequenceExtensions::FilterTracks(TArrayView<UMovieSceneTrack* const> InTracks, UClass* DesiredClass, bool bExactMatch)
{
	TArray<UMovieSceneTrack*> Tracks;

	for (UMovieSceneTrack* Track : InTracks)
	{
		UClass* TrackClass = Track->GetClass();

		if ( TrackClass == DesiredClass || (!bExactMatch && TrackClass->IsChildOf(DesiredClass)) )
		{
			Tracks.Add(Track);
		}
	}

	return Tracks;
}

UMovieScene* UMovieSceneSequenceExtensions::GetMovieScene(UMovieSceneSequence* Sequence)
{
	return Sequence->GetMovieScene();
}

TArray<UMovieSceneTrack*> UMovieSceneSequenceExtensions::GetMasterTracks(UMovieSceneSequence* Sequence)
{
	TArray<UMovieSceneTrack*> Tracks;

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		Tracks = MovieScene->GetMasterTracks();
	}

	if (UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack())
	{
		Tracks.Add(CameraCutTrack);
	}

	return Tracks;
}

TArray<UMovieSceneTrack*> UMovieSceneSequenceExtensions::FindMasterTracksByType(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = Sequence->GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene && DesiredClass)
	{
		bool bExactMatch = false;
		TArray<UMovieSceneTrack*> MatchedTracks = FilterTracks(MovieScene->GetMasterTracks(), TrackType.Get(), bExactMatch);

		// Have to check camera cut tracks separately since they're not in the master tracks array (why?)
		UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
		if (CameraCutTrack && CameraCutTrack->GetClass()->IsChildOf(DesiredClass))
		{
			MatchedTracks.Add(CameraCutTrack);
		}

		return MatchedTracks;
	}

	return TArray<UMovieSceneTrack*>();
}

TArray<UMovieSceneTrack*> UMovieSceneSequenceExtensions::FindMasterTracksByExactType(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType)
{
	UMovieScene* MovieScene   = Sequence->GetMovieScene();
	UClass*      DesiredClass = TrackType.Get();

	if (MovieScene && DesiredClass)
	{
		bool bExactMatch = true;
		TArray<UMovieSceneTrack*> MatchedTracks = FilterTracks(MovieScene->GetMasterTracks(), TrackType.Get(), bExactMatch);

		// Have to check camera cut tracks separately since they're not in the master tracks array (why?)
		UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
		if (CameraCutTrack && CameraCutTrack->GetClass() == DesiredClass)
		{
			MatchedTracks.Add(CameraCutTrack);
		}

		return MatchedTracks;
	}

	return TArray<UMovieSceneTrack*>();
}

UMovieSceneTrack* UMovieSceneSequenceExtensions::AddMasterTrack(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType)
{
	// @todo: sequencer-python: master track type compatibility with sequence. Currently that's really only loosely defined by track editors, which is not sufficient here.
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		UMovieSceneTrack* NewTrack = MovieScene->AddMasterTrack(TrackType);
		if (NewTrack)
		{
			return NewTrack;
		}
	}

	return nullptr;
}

FFrameRate UMovieSceneSequenceExtensions::GetDisplayRate(UMovieSceneSequence* Sequence)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	return MovieScene ? MovieScene->GetDisplayRate() : FFrameRate();
}

FFrameRate UMovieSceneSequenceExtensions::GetTickResolution(UMovieSceneSequence* Sequence)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	return MovieScene ? MovieScene->GetTickResolution() : FFrameRate();
}

FSequencerScriptingRange UMovieSceneSequenceExtensions::MakeRange(UMovieSceneSequence* Sequence, int32 StartFrame, int32 Duration)
{
	FFrameRate FrameRate = GetDisplayRate(Sequence);
	return FSequencerScriptingRange::FromNative(TRange<FFrameNumber>(StartFrame, StartFrame+Duration), FrameRate, FrameRate);
}

FSequencerScriptingRange UMovieSceneSequenceExtensions::MakeRangeSeconds(UMovieSceneSequence* Sequence, float StartTime, float Duration)
{
	FFrameRate FrameRate = GetDisplayRate(Sequence);
	return FSequencerScriptingRange::FromNative(TRange<FFrameNumber>((StartTime*FrameRate).FloorToFrame(), ((StartTime+Duration) * FrameRate).CeilToFrame()), FrameRate, FrameRate);
}

FSequencerScriptingRange UMovieSceneSequenceExtensions::GetPlaybackRange(UMovieSceneSequence* Sequence)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		return FSequencerScriptingRange::FromNative(MovieScene->GetPlaybackRange(), GetTickResolution(Sequence));
	}
	else
	{
		return FSequencerScriptingRange();
	}
}

FSequencerBindingProxy UMovieSceneSequenceExtensions::FindBindingByName(UMovieSceneSequence* Sequence, FString Name)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), Name, &FMovieSceneBinding::GetName);
		if (Binding)
		{
			return FSequencerBindingProxy(Binding->GetObjectGuid(), Sequence);
		}
	}
	return FSequencerBindingProxy();
}

TArray<FSequencerBindingProxy> UMovieSceneSequenceExtensions::GetBindings(UMovieSceneSequence* Sequence)
{
	TArray<FSequencerBindingProxy> AllBindings;

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			AllBindings.Emplace(Binding.GetObjectGuid(), Sequence);
		}
	}

	return AllBindings;
}

TArray<FSequencerBindingProxy> UMovieSceneSequenceExtensions::GetSpawnables(UMovieSceneSequence* Sequence)
{
	TArray<FSequencerBindingProxy> AllSpawnables;

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		int32 Count = MovieScene->GetSpawnableCount();
		AllSpawnables.Reserve(Count);
		for (int32 i=0; i < Count; ++i)
		{
			AllSpawnables.Emplace(MovieScene->GetSpawnable(i).GetGuid(), Sequence);
		}
	}

	return AllSpawnables;
}

TArray<FSequencerBindingProxy> UMovieSceneSequenceExtensions::GetPossessables(UMovieSceneSequence* Sequence)
{
	TArray<FSequencerBindingProxy> AllPossessables;

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		int32 Count = MovieScene->GetPossessableCount();
		AllPossessables.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			AllPossessables.Emplace(MovieScene->GetPossessable(i).GetGuid(), Sequence);
		}
	}

	return AllPossessables;
}

FSequencerBindingProxy UMovieSceneSequenceExtensions::AddPossessable(UMovieSceneSequence* Sequence, UObject* ObjectToPossess)
{
	FGuid NewGuid = Sequence->CreatePossessable(ObjectToPossess);
	return FSequencerBindingProxy(NewGuid, Sequence);
}

FSequencerBindingProxy UMovieSceneSequenceExtensions::AddSpawnableFromInstance(UMovieSceneSequence* Sequence, UObject* ObjectToSpawn)
{
	FGuid NewGuid = Sequence->AllowsSpawnableObjects() ? Sequence->CreateSpawnable(ObjectToSpawn) : FGuid();
	return FSequencerBindingProxy(NewGuid, Sequence);
}

FSequencerBindingProxy UMovieSceneSequenceExtensions::AddSpawnableFromClass(UMovieSceneSequence* Sequence, UClass* ClassToSpawn)
{
	FGuid NewGuid = Sequence->AllowsSpawnableObjects() ? Sequence->CreateSpawnable(ClassToSpawn) : FGuid();
	return FSequencerBindingProxy(NewGuid, Sequence);
}

TArray<UObject*> UMovieSceneSequenceExtensions::LocateBoundObjects(UMovieSceneSequence* Sequence, const FSequencerBindingProxy& InBinding, UObject* Context)
{
	TArray<UObject*> Result;
	if (Sequence)
	{
		TArray<UObject*, TInlineAllocator<1>> OutObjects;
		Sequence->LocateBoundObjects(InBinding.BindingID, Context, OutObjects);
		Result.Append(OutObjects);
	}

	return Result;
}

