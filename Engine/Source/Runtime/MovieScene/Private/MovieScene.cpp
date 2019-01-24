// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneFolder.h"
#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "UObject/SequencerObjectVersion.h"
#include "CommonFrameRates.h"

#define LOCTEXT_NAMESPACE "MovieScene"

TOptional<TRangeBound<FFrameNumber>> GetMaxUpperBound(const UMovieSceneTrack* Track)
{
	TOptional<TRangeBound<FFrameNumber>> MaxBound;

	// Find the largest closed upper bound of all the track's sections
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		TRangeBound<FFrameNumber> SectionUpper = Section->GetRange().GetUpperBound();
		if (SectionUpper.IsClosed())
		{
			MaxBound = TRangeBound<FFrameNumber>::MaxUpper(MaxBound.Get(SectionUpper), SectionUpper);
		}
	}

	return MaxBound;
}

/* UMovieScene interface
 *****************************************************************************/

UMovieScene::UMovieScene(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvaluationType = EMovieSceneEvaluationType::WithSubFrames;
	ClockSource = EUpdateClockSource::Tick;

	if (!HasAnyFlags(RF_ClassDefaultObject) && GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::FloatToIntConversion)
	{
		TickResolution = GetLegacyConversionFrameRate();
	}

#if WITH_EDITORONLY_DATA
	bReadOnly = false;
	bPlaybackRangeLocked = false;
	PlaybackRange.MigrationDefault = FFloatRange::Empty();
	EditorData.WorkingRange_DEPRECATED = EditorData.ViewRange_DEPRECATED = TRange<float>::Empty();

	bForceFixedFrameIntervalPlayback_DEPRECATED = false;
	FixedFrameInterval_DEPRECATED = 0.0f;

	InTime_DEPRECATED    =  FLT_MAX;
	OutTime_DEPRECATED   = -FLT_MAX;
	StartTime_DEPRECATED =  FLT_MAX;
	EndTime_DEPRECATED   = -FLT_MAX;
#endif
}

bool UMovieScene::IsPostLoadThreadSafe() const
{
	return true;
}

void UMovieScene::Serialize( FArchive& Ar )
{
	Ar.UsingCustomVersion(FMovieSceneEvaluationCustomVersion::GUID);
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);

#if WITH_EDITOR

	// Perform optimizations for cooking
	if (Ar.IsCooking())
	{
		// @todo: Optimize master tracks?

		// Optimize object bindings
		OptimizeObjectArray(Spawnables);
		OptimizeObjectArray(Possessables);
	}

#endif // WITH_EDITOR

	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::FloatToIntConversion)
	{
		if (bForceFixedFrameIntervalPlayback_DEPRECATED)
		{
			EvaluationType = EMovieSceneEvaluationType::FrameLocked;
		}

		// Legacy fixed frame interval conversion to integer play rates
		if      (FixedFrameInterval_DEPRECATED == 1 / 15.0f)  { DisplayRate = FCommonFrameRates::FPS_15();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 24.0f)  { DisplayRate = FCommonFrameRates::FPS_24();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 25.0f)  { DisplayRate = FCommonFrameRates::FPS_25();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 29.97)  { DisplayRate = FCommonFrameRates::NTSC_30(); }
		else if (FixedFrameInterval_DEPRECATED == 1 / 30.0f)  { DisplayRate = FCommonFrameRates::FPS_30();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 48.0f)  { DisplayRate = FCommonFrameRates::FPS_48();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 50.0f)  { DisplayRate = FCommonFrameRates::FPS_50();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 59.94)  { DisplayRate = FCommonFrameRates::NTSC_60(); }
		else if (FixedFrameInterval_DEPRECATED == 1 / 60.0f)  { DisplayRate = FCommonFrameRates::FPS_60();  }
		else if (FixedFrameInterval_DEPRECATED == 1 / 120.0f) { DisplayRate = FCommonFrameRates::FPS_120(); }
		else if (FixedFrameInterval_DEPRECATED != 0.f)
		{
			uint32 Numerator = FMath::RoundToInt(1000.f / FixedFrameInterval_DEPRECATED);
			DisplayRate = FFrameRate(Numerator, 1000);
		}
		else
		{
			// Sequences with 0 FixedFrameInterval used to be assigned a proper interval in SSequencer::OnSequenceInstanceActivated for some reason,
			// But we don't have access to the relevant sequencer settings class here so we just have to make a hacky educated guess based on the class name.
			UObject* Outer = GetOuter();
			if (Outer && Outer->GetClass()->GetFName() == "WidgetAnimation")
			{
				// Widget animations defaulted to 0.05s
				DisplayRate = FFrameRate(20, 1);
			}
			else if (Outer && Outer->GetClass()->GetFName() == "ActorSequence")
			{
				// Actor sequences defaulted to 0.1s
				DisplayRate = FFrameRate(10, 1);
			}
			else
			{
				// Level sequences defaulted to 30fps - this is the fallback default for anything else
				DisplayRate = FFrameRate(30, 1);
			}
		}
	}
#endif
}

#if WITH_EDITOR

template<typename T>
void UMovieScene::OptimizeObjectArray(TArray<T>& ObjectArray)
{
	for (int32 ObjectIndex = ObjectArray.Num() - 1; ObjectIndex >= 0; --ObjectIndex)
	{
		FGuid ObjectGuid = ObjectArray[ObjectIndex].GetGuid();

		// Find the binding relating to this object, and optimize its tracks
		// @todo: ObjectBindings mapped by ID to avoid linear search
		for (int32 BindingIndex = 0; BindingIndex < ObjectBindings.Num(); ++BindingIndex)
		{
			FMovieSceneBinding& Binding = ObjectBindings[BindingIndex];
			if (Binding.GetObjectGuid() != ObjectGuid)
			{
				continue;
			}
			
			bool bShouldRemoveObject = false;

			// Optimize any tracks
			Binding.PerformCookOptimization(bShouldRemoveObject);

			// Remove the object if it's completely redundant
			if (bShouldRemoveObject)
			{
				ObjectBindings.RemoveAtSwap(BindingIndex, 1, false);
				ObjectArray.RemoveAtSwap(ObjectIndex, 1, false);
			}

			// Process next object
			break;
		}
	}
}

// @todo sequencer: Some of these methods should only be used by tools, and should probably move out of MovieScene!
FGuid UMovieScene::AddSpawnable( const FString& Name, UObject& ObjectTemplate )
{
	Modify();

	FMovieSceneSpawnable NewSpawnable( Name, ObjectTemplate );
	Spawnables.Add( NewSpawnable );

	// Add a new binding so that tracks can be added to it
	new (ObjectBindings) FMovieSceneBinding( NewSpawnable.GetGuid(), NewSpawnable.GetName() );

	return NewSpawnable.GetGuid();
}

void UMovieScene::AddSpawnable(const FMovieSceneSpawnable& InNewSpawnable, const FMovieSceneBinding& InNewBinding)
{
	Modify();

	FMovieSceneSpawnable NewSpawnable;
	NewSpawnable = InNewSpawnable;
	Spawnables.Add(NewSpawnable);

	FMovieSceneBinding NewBinding = InNewBinding;
	for (auto Track : NewBinding.GetTracks())
	{
		Track->Rename(nullptr, this);
	}
	ObjectBindings.Add(NewBinding);
}

bool UMovieScene::RemoveSpawnable( const FGuid& Guid )
{
	bool bAnythingRemoved = false;
	if( ensure( Guid.IsValid() ) )
	{
		for( auto SpawnableIter( Spawnables.CreateIterator() ); SpawnableIter; ++SpawnableIter )
		{
			auto& CurSpawnable = *SpawnableIter;
			if( CurSpawnable.GetGuid() == Guid )
			{
				Modify();
				RemoveBinding( Guid );

				Spawnables.RemoveAt( SpawnableIter.GetIndex() );
				
				bAnythingRemoved = true;
				break;
			}
		}
	}

	return bAnythingRemoved;
}

FMovieSceneSpawnable* UMovieScene::FindSpawnable( const TFunctionRef<bool(FMovieSceneSpawnable&)>& InPredicate )
{
	return Spawnables.FindByPredicate(InPredicate);
}

#endif //WITH_EDITOR


FMovieSceneSpawnable& UMovieScene::GetSpawnable(int32 Index)
{
	return Spawnables[Index];
}

int32 UMovieScene::GetSpawnableCount() const
{
	return Spawnables.Num();
}

FMovieSceneSpawnable* UMovieScene::FindSpawnable( const FGuid& Guid )
{
	return Spawnables.FindByPredicate([&](FMovieSceneSpawnable& Spawnable) {
		return Spawnable.GetGuid() == Guid;
	});
}


FGuid UMovieScene::AddPossessable( const FString& Name, UClass* Class )
{
	Modify();

	FMovieScenePossessable NewPossessable( Name, Class );
	Possessables.Add( NewPossessable );

	// Add a new binding so that tracks can be added to it
	new (ObjectBindings) FMovieSceneBinding( NewPossessable.GetGuid(), NewPossessable.GetName() );

	return NewPossessable.GetGuid();
}


void UMovieScene::AddPossessable(const FMovieScenePossessable& InNewPossessable, const FMovieSceneBinding& InNewBinding)
{
	Modify();

	FMovieScenePossessable NewPossessable;
	NewPossessable = InNewPossessable;
	Possessables.Add(NewPossessable);

	FMovieSceneBinding NewBinding = InNewBinding;
	for (auto Track : NewBinding.GetTracks())
	{
		Track->Rename(nullptr, this);
	}
	ObjectBindings.Add(NewBinding);
}


bool UMovieScene::RemovePossessable( const FGuid& PossessableGuid )
{
	bool bAnythingRemoved = false;

	for( auto PossesableIter( Possessables.CreateIterator() ); PossesableIter; ++PossesableIter )
	{
		auto& CurPossesable = *PossesableIter;

		if( CurPossesable.GetGuid() == PossessableGuid )
		{	
			Modify();

			// Remove the parent-child link for a parent spawnable/child possessable if necessary
			if (CurPossesable.GetParent().IsValid())
			{
				FMovieSceneSpawnable* ParentSpawnable = FindSpawnable(CurPossesable.GetParent());
				if (ParentSpawnable)
				{
					ParentSpawnable->RemoveChildPossessable(PossessableGuid);
				}
			}

			// Found it!
			Possessables.RemoveAt( PossesableIter.GetIndex() );

			RemoveBinding( PossessableGuid );

			bAnythingRemoved = true;
			break;
		}
	}

	return bAnythingRemoved;
}


bool UMovieScene::ReplacePossessable( const FGuid& OldGuid, const FMovieScenePossessable& InNewPosessable )
{
	bool bAnythingReplaced = false;

	for (auto& Possessable : Possessables)
	{
		if (Possessable.GetGuid() == OldGuid)
		{	
			Modify();

			// Found it!
			if (InNewPosessable.GetPossessedObjectClass() == nullptr)
			{
				// @todo: delete this when
				// bool ReplacePossessable(const FGuid& OldGuid, const FGuid& NewGuid, const FString& Name)
				// is removed
				Possessable.SetGuid(InNewPosessable.GetGuid());
				Possessable.SetName(InNewPosessable.GetName());
			}
			else
			{
				Possessable = InNewPosessable;
			}

			ReplaceBinding( OldGuid, InNewPosessable.GetGuid(), InNewPosessable.GetName() );
			bAnythingReplaced = true;

			break;
		}
	}

	return bAnythingReplaced;
}


FMovieScenePossessable* UMovieScene::FindPossessable( const FGuid& Guid )
{
	for (auto& Possessable : Possessables)
	{
		if (Possessable.GetGuid() == Guid)
		{
			return &Possessable;
		}
	}

	return nullptr;
}

FMovieScenePossessable* UMovieScene::FindPossessable( const TFunctionRef<bool(FMovieScenePossessable&)>& InPredicate )
{
	return Possessables.FindByPredicate(InPredicate);
}

int32 UMovieScene::GetPossessableCount() const
{
	return Possessables.Num();
}


FMovieScenePossessable& UMovieScene::GetPossessable( const int32 Index )
{
	return Possessables[Index];
}


FText UMovieScene::GetObjectDisplayName(const FGuid& ObjectId)
{
#if WITH_EDITORONLY_DATA
	FText* Result = ObjectsToDisplayNames.Find(ObjectId.ToString());

	if (Result && !Result->IsEmpty())
	{
		return *Result;
	}

	FMovieSceneSpawnable* Spawnable = FindSpawnable(ObjectId);

	if (Spawnable != nullptr)
	{
		return FText::FromString(Spawnable->GetName());
	}

	FMovieScenePossessable* Possessable = FindPossessable(ObjectId);

	if (Possessable != nullptr)
	{
		return FText::FromString(Possessable->GetName());
	}
#endif
	return FText::GetEmpty();
}



#if WITH_EDITORONLY_DATA
void UMovieScene::SetObjectDisplayName(const FGuid& ObjectId, const FText& DisplayName)
{
	if (DisplayName.IsEmpty())
	{
		ObjectsToDisplayNames.Remove(ObjectId.ToString());
	}
	else
	{
		ObjectsToDisplayNames.Add(ObjectId.ToString(), DisplayName);
	}
}


TArray<UMovieSceneFolder*>&  UMovieScene::GetRootFolders()
{
	return RootFolders;
}
#endif

void UMovieScene::SetPlaybackRange(FFrameNumber Start, int32 Duration, bool bAlwaysMarkDirty)
{
	// Inclusive lower, Exclusive upper bound
	SetPlaybackRange(TRange<FFrameNumber>(Start, Start + Duration), bAlwaysMarkDirty);
}

void UMovieScene::SetPlaybackRange(const TRange<FFrameNumber>& NewRange, bool bAlwaysMarkDirty)
{
	check(NewRange.GetLowerBound().IsClosed() && NewRange.GetUpperBound().IsClosed());

	if (PlaybackRange.Value == NewRange)
	{
		return;
	}

	if (bAlwaysMarkDirty)
	{
		Modify();
	}

	PlaybackRange.Value = NewRange;

#if WITH_EDITORONLY_DATA
	// Update the working and view ranges to encompass the new range
	const double RangeStartSeconds = NewRange.GetLowerBoundValue() / TickResolution;
	const double RangeEndSeconds   = NewRange.GetUpperBoundValue() / TickResolution;

	// Initialize the working and view range with a little bit more space
	const double OutputChange      = (RangeEndSeconds - RangeStartSeconds) * 0.1;

	double ExpandedStart = RangeStartSeconds - OutputChange;
	double ExpandedEnd   = RangeEndSeconds + OutputChange;

	if (EditorData.WorkStart >= EditorData.WorkEnd)
	{
		EditorData.WorkStart = ExpandedStart;
		EditorData.WorkEnd   = ExpandedEnd;
	}

	if (EditorData.ViewStart >= EditorData.ViewEnd)
	{
		EditorData.ViewStart = ExpandedStart;
		EditorData.ViewEnd   = ExpandedEnd;
	}
#endif
}

void UMovieScene::SetWorkingRange(float Start, float End)
{
#if WITH_EDITORONLY_DATA
	EditorData.WorkStart = Start;
	EditorData.WorkEnd   = End;
#endif
}

void UMovieScene::SetViewRange(float Start, float End)
{
#if WITH_EDITORONLY_DATA
	EditorData.ViewStart = Start;
	EditorData.ViewEnd   = End;
#endif
}

#if WITH_EDITORONLY_DATA
bool UMovieScene::IsPlaybackRangeLocked() const
{
	return bPlaybackRangeLocked;
}

void UMovieScene::SetPlaybackRangeLocked(bool bLocked)
{
	bPlaybackRangeLocked = bLocked;
}
#endif


TArray<UMovieSceneSection*> UMovieScene::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSections;

	// Add all master type sections 
	for( int32 TrackIndex = 0; TrackIndex < MasterTracks.Num(); ++TrackIndex )
	{
		OutSections.Append( MasterTracks[TrackIndex]->GetAllSections() );
	}
	
	// Add all camera cut sections
	if (CameraCutTrack != nullptr)
	{
		OutSections.Append(CameraCutTrack->GetAllSections());
	}

	// Add all object binding sections
	for (const auto& Binding : ObjectBindings)
	{
		for (const auto& Track : Binding.GetTracks())
		{
			OutSections.Append(Track->GetAllSections());
		}
	}

	return OutSections;
}


UMovieSceneTrack* UMovieScene::FindTrack(TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid, const FName& TrackName) const
{
	check( ObjectGuid.IsValid() );
	
	for (const auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() != ObjectGuid) 
		{
			continue;
		}

		for (const auto& Track : Binding.GetTracks())
		{
			if (TrackClass.GetDefaultObject() == nullptr ||  Track->GetClass() == TrackClass)
			{
				if (TrackName == NAME_None || Track->GetTrackName() == TrackName)
				{
					return Track;
				}
			}
		}
	}
	
	return nullptr;
}


UMovieSceneTrack* UMovieScene::AddTrack( TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid )
{
	UMovieSceneTrack* CreatedType = nullptr;

	check( ObjectGuid.IsValid() )

	for (auto& Binding : ObjectBindings)
	{
		if( Binding.GetObjectGuid() == ObjectGuid ) 
		{
			Modify();

			CreatedType = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
			check(CreatedType);
			
			Binding.AddTrack( *CreatedType );
		}
	}

	return CreatedType;
}

bool UMovieScene::AddGivenTrack(UMovieSceneTrack* InTrack, const FGuid& ObjectGuid)
{
	check(ObjectGuid.IsValid());

	Modify();
	for (auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() == ObjectGuid)
		{
			InTrack->Rename(nullptr, this);
			check(InTrack);
			Binding.AddTrack(*InTrack);
			return true;
		}
	}

	return false;
}

bool UMovieScene::RemoveTrack(UMovieSceneTrack& Track)
{
	Modify();

	bool bAnythingRemoved = false;

	for (auto& Binding : ObjectBindings)
	{
		if (Binding.RemoveTrack(Track))
		{
			bAnythingRemoved = true;

			// The track was removed from the current binding, stop
			// searching now as it cannot exist in any other binding
			break;
		}
	}

	return bAnythingRemoved;
}

bool UMovieScene::FindTrackBinding(const UMovieSceneTrack& InTrack, FGuid& OutGuid) const
{
	for (auto& Binding : ObjectBindings)
	{
		for(auto& Track : Binding.GetTracks())
		{
			if(Track == &InTrack)
			{
				OutGuid = Binding.GetObjectGuid();
				return true;
			}
		}
	}

	return false;
}

UMovieSceneTrack* UMovieScene::FindMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass ) const
{
	UMovieSceneTrack* FoundTrack = nullptr;

	for (const auto Track : MasterTracks)
	{
		if( Track->GetClass() == TrackClass )
		{
			FoundTrack = Track;
			break;
		}
	}

	return FoundTrack;
}


UMovieSceneTrack* UMovieScene::AddMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
{
	Modify();

	UMovieSceneTrack* CreatedType = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
	MasterTracks.Add( CreatedType );
	
	return CreatedType;
}


bool UMovieScene::AddGivenMasterTrack(UMovieSceneTrack* InTrack)
{
	if (!MasterTracks.Contains(InTrack))
	{
		Modify();
		InTrack->Rename(nullptr, this);
		MasterTracks.Add(InTrack);
		return true;
	}
	return false;
}


bool UMovieScene::RemoveMasterTrack(UMovieSceneTrack& Track) 
{
	Modify();

	return (MasterTracks.RemoveSingle(&Track) != 0);
}


bool UMovieScene::IsAMasterTrack(const UMovieSceneTrack& Track) const
{
	for ( const UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (&Track == MasterTrack)
		{
			return true;
		}
	}

	return false;
}


UMovieSceneTrack* UMovieScene::AddCameraCutTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
{
	if( !CameraCutTrack )
	{
		Modify();
		CameraCutTrack = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
	}

	return CameraCutTrack;
}


UMovieSceneTrack* UMovieScene::GetCameraCutTrack()
{
	return CameraCutTrack;
}


void UMovieScene::RemoveCameraCutTrack()
{
	if( CameraCutTrack )
	{
		Modify();
		CameraCutTrack = nullptr;
	}
}

void UMovieScene::SetCameraCutTrack(UMovieSceneTrack* InTrack)
{
	Modify();
	InTrack->Rename(nullptr, this);
	CameraCutTrack = InTrack;
}


void UMovieScene::UpgradeTimeRanges()
{
	// Legacy upgrade for playback ranges:
	// We used to optionally store a start/end and in/out time for sequences.
	// The only 2 uses were UWidgetAnimations and ULevelSequences.
	// Widget animations used to always calculate their length automatically, from the section boundaries, and always started at 0
	// Level sequences defaulted to having a fixed play range.
	// We now expose the playback range more visibly, but we need to upgrade the old data.

	bool bFiniteRangeDefined = false;

#if WITH_EDITORONLY_DATA
	if (InTime_DEPRECATED != FLT_MAX && OutTime_DEPRECATED != -FLT_MAX)
	{
		// Finite range already defined in old data
		PlaybackRange.Value = TRange<FFrameNumber>(
			TickResolution.AsFrameNumber(InTime_DEPRECATED),
			// Prefer exclusive upper bounds for playback ranges so we stop at the next frame
			++TickResolution.AsFrameNumber(OutTime_DEPRECATED)
			);
		bFiniteRangeDefined = true;
	}
#endif
	if (!bFiniteRangeDefined && PlaybackRange.Value.IsEmpty())
	{
		// No range specified, so automatically calculate one by determining the maximum upper bound of the sequence
		// In this instance (UMG), playback always started at 0
		TRangeBound<FFrameNumber> MaxFrame = TRangeBound<FFrameNumber>::Exclusive(0);

		for (const UMovieSceneTrack* Track : MasterTracks)
		{
			TOptional<TRangeBound<FFrameNumber>> MaxUpper = GetMaxUpperBound(Track);
			if (MaxUpper.IsSet())
			{
				MaxFrame = TRangeBound<FFrameNumber>::MaxUpper(MaxFrame, MaxUpper.GetValue());
			}
		}

		for (const FMovieSceneBinding& Binding : ObjectBindings)
		{
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				TOptional<TRangeBound<FFrameNumber>> MaxUpper = GetMaxUpperBound(Track);
				if (MaxUpper.IsSet())
				{
					MaxFrame = TRangeBound<FFrameNumber>::MaxUpper(MaxFrame, MaxUpper.GetValue());
				}
			}
		}

		// Playback ranges should always have exclusive upper bounds
		if (MaxFrame.IsInclusive())
		{
			MaxFrame = TRangeBound<FFrameNumber>::Exclusive(MaxFrame.GetValue() + 1);
		}

		PlaybackRange.Value = TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(0), MaxFrame);
	}
	else if (PlaybackRange.Value.GetUpperBound().IsInclusive())
	{
		// playback ranges are now always inclusive
		PlaybackRange.Value = TRange<FFrameNumber>(PlaybackRange.Value.GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(PlaybackRange.Value.GetUpperBoundValue() + 1));
	}

	// PlaybackRange must always be defined to a finite range
	if (!PlaybackRange.Value.HasLowerBound() || !PlaybackRange.Value.HasUpperBound() || PlaybackRange.Value.IsDegenerate())
	{
		PlaybackRange.Value = TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(0));
	}

#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::FloatToIntConversion)
	{
		EditorData.ViewStart    = EditorData.ViewRange_DEPRECATED.GetLowerBoundValue();
		EditorData.ViewEnd      = EditorData.ViewRange_DEPRECATED.GetUpperBoundValue();

		EditorData.WorkStart    = EditorData.WorkingRange_DEPRECATED.GetLowerBoundValue();
		EditorData.WorkEnd      = EditorData.WorkingRange_DEPRECATED.GetUpperBoundValue();
	}

	// Legacy upgrade for working range
	if (StartTime_DEPRECATED != FLT_MAX && EndTime_DEPRECATED != -FLT_MAX)
	{
		EditorData.WorkStart = StartTime_DEPRECATED;
		EditorData.WorkEnd   = EndTime_DEPRECATED;
	}
	else if (EditorData.WorkStart >= EditorData.WorkEnd)
	{
		EditorData.WorkStart = PlaybackRange.Value.GetLowerBoundValue() / TickResolution;
		EditorData.WorkEnd   = PlaybackRange.Value.GetUpperBoundValue() / TickResolution;
	}

	if (EditorData.ViewStart >= EditorData.ViewEnd)
	{
		EditorData.ViewStart = PlaybackRange.Value.GetLowerBoundValue() / TickResolution;
		EditorData.ViewEnd   = PlaybackRange.Value.GetUpperBoundValue() / TickResolution;
	}

	if (SelectionRange.Value.GetLowerBound().IsOpen() || SelectionRange.Value.GetUpperBound().IsOpen())
	{
		SelectionRange.Value = TRange<FFrameNumber>::Empty();
	}
#endif
}

/* UObject interface
 *****************************************************************************/

void UMovieScene::PostLoad()
{
	// Remove any null tracks
	for( int32 TrackIndex = 0; TrackIndex < MasterTracks.Num(); )
	{
		if (MasterTracks[TrackIndex] == nullptr)
		{
			MasterTracks.RemoveAt(TrackIndex);
		}
		else
		{
			++TrackIndex;
		}
	}

	for ( int32 ObjectBindingIndex = 0; ObjectBindingIndex < ObjectBindings.Num(); ++ObjectBindingIndex)
	{
		for( int32 TrackIndex = 0; TrackIndex < ObjectBindings[ObjectBindingIndex].GetTracks().Num(); )
		{
			if (ObjectBindings[ObjectBindingIndex].GetTracks()[TrackIndex] == nullptr)
			{
				ObjectBindings[ObjectBindingIndex].RemoveTrack(*ObjectBindings[ObjectBindingIndex].GetTracks()[TrackIndex]);
			}
			else
			{
				++TrackIndex;
			}
		}
	}

	UpgradeTimeRanges();

	for (FMovieSceneSpawnable& Spawnable : Spawnables)
	{
		if (UObject* Template = Spawnable.GetObjectTemplate())
		{
			// Spawnables are no longer marked archetype
			Template->ClearFlags(RF_ArchetypeObject);
			
			FMovieSceneSpawnable::MarkSpawnableTemplate(*Template);
		}
	}

#if WITH_EDITORONLY_DATA
	for (FFrameNumber MarkedFrame : EditorData.MarkedFrames_DEPRECATED)
	{
		MarkedFrames.Add(FMovieSceneMarkedFrame(MarkedFrame));
	}
	EditorData.MarkedFrames_DEPRECATED.Empty();
#endif

	Super::PostLoad();
}


void UMovieScene::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

#if WITH_EDITORONLY_DATA
	// compress meta data mappings prior to saving
	for (auto It = ObjectsToDisplayNames.CreateIterator(); It; ++It)
	{
		FGuid ObjectId;

		if (!FGuid::Parse(It.Key(), ObjectId) || ((FindPossessable(ObjectId) == nullptr) && (FindSpawnable(ObjectId) == nullptr)))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = ObjectsToLabels.CreateIterator(); It; ++It)
	{
		FGuid ObjectId;

		if (!FGuid::Parse(It.Key(), ObjectId) || ((FindPossessable(ObjectId) == nullptr) && (FindSpawnable(ObjectId) == nullptr)))
		{
			It.RemoveCurrent();
		}
	}
#endif
}


/* UMovieScene implementation
 *****************************************************************************/

void UMovieScene::RemoveBinding(const FGuid& Guid)
{
	// update each type
	for (int32 BindingIndex = 0; BindingIndex < ObjectBindings.Num(); ++BindingIndex)
	{
		if (ObjectBindings[BindingIndex].GetObjectGuid() == Guid)
		{
			ObjectBindings.RemoveAt(BindingIndex);
			break;
		}
	}
}


void UMovieScene::ReplaceBinding(const FGuid& OldGuid, const FGuid& NewGuid, const FString& Name)
{
	for (auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() == OldGuid)
		{
			Binding.SetObjectGuid(NewGuid);
			Binding.SetName(Name);

			// Changing a binding guid invalidates any tracks contained within the binding
			// Make sure they are written into the transaction buffer by calling modify
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				Track->Modify();
			}
			break;
		}
	}
}

void UMovieScene::ReplaceBinding(const FGuid& BindingToReplaceGuid, const FMovieSceneBinding& NewBinding)
{
	FMovieSceneBinding* Binding = ObjectBindings.FindByPredicate([BindingToReplaceGuid](const FMovieSceneBinding& CheckedBinding) { return CheckedBinding.GetObjectGuid() == BindingToReplaceGuid; });
	if (Binding)
	{
		*Binding = NewBinding;

		// We also need to change the track's owners to be the MovieScene.
		for (auto Track : Binding->GetTracks())
		{
			Track->Rename(nullptr, this);
		}
	}
}

void UMovieScene::MoveBindingContents(const FGuid& SourceBindingId, const FGuid& DestinationBindingId)
{
	FMovieSceneBinding* SourceBinding = nullptr;
	FMovieSceneBinding* DestinationBinding = nullptr;

	for (FMovieSceneBinding& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() == SourceBindingId)
		{
			SourceBinding = &Binding;
		}
		else if (Binding.GetObjectGuid() == DestinationBindingId)
		{
			DestinationBinding = &Binding;
		}

		if (SourceBinding && DestinationBinding)
		{
			break;
		}
	}

	if (SourceBinding && DestinationBinding)
	{
		// Swap the tracks round
		DestinationBinding->SetTracks(SourceBinding->StealTracks());

		// Changing a binding guid invalidates any tracks contained within the binding
		// Make sure they are written into the transaction buffer by calling modify
		for (UMovieSceneTrack* Track : DestinationBinding->GetTracks())
		{
			Track->Modify();
		}
	}

	FMovieSceneSpawnable* DestinationSpawnable = FindSpawnable(DestinationBindingId);

	for (FMovieScenePossessable& Possessable : Possessables)
	{
		if (Possessable.GetParent() == SourceBindingId)
		{
			Possessable.SetParent(DestinationBindingId);
			if (DestinationSpawnable)
			{
				DestinationSpawnable->AddChildPossessable(Possessable.GetGuid());
			}
		}
	}
}

void UMovieScene::AddMarkedFrame(const FMovieSceneMarkedFrame &InMarkedFrame)
{
	FString NewLabel;

	if (InMarkedFrame.Label.IsEmpty())
	{
		FText Characters = LOCTEXT("MarkedFrameCharacters", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

		int32 NumCharacters = 1;
		bool bFound = false;
		while (!bFound)
		{
			int32 CharacterIndex = 0;
			for (; CharacterIndex < Characters.ToString().Len() && !bFound; ++CharacterIndex)
			{
				NewLabel = FString::ChrN(NumCharacters, Characters.ToString()[CharacterIndex]);
				bool bExists = false;
				for (const FMovieSceneMarkedFrame& MarkedFrame : MarkedFrames)
				{
					if (MarkedFrame.Label == NewLabel)
					{
						bExists = true;
						break;
					}
				}

				if (!bExists)
				{
					bFound = true;
				}
			}
			++NumCharacters;
		}
	}

	int32 MarkedIndex = MarkedFrames.Add(InMarkedFrame);

	if (!NewLabel.IsEmpty())
	{
		MarkedFrames[MarkedIndex].Label = NewLabel;
	}
}

void UMovieScene::RemoveMarkedFrame(int32 RemoveIndex)
{
	MarkedFrames.RemoveAt(RemoveIndex);
}

void UMovieScene::ClearMarkedFrames()
{
	MarkedFrames.Empty();
}

int32 UMovieScene::FindMarkedFrameByLabel(const FString& InLabel) const
{
	for (int32 Index = 0; Index < MarkedFrames.Num(); ++Index)
	{
		if (MarkedFrames[Index].Label == InLabel)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UMovieScene::FindMarkedFrameByFrameNumber(FFrameNumber InFrameNumber) const
{
	for (int32 Index = 0; Index < MarkedFrames.Num(); ++Index)
	{
		if (MarkedFrames[Index].FrameNumber == InFrameNumber)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UMovieScene::FindNextMarkedFrame(FFrameNumber InFrameNumber, bool bForwards)
{
	if (MarkedFrames.Num() == 0)
	{
		return INDEX_NONE;
	}

	MarkedFrames.Sort([&](const FMovieSceneMarkedFrame& A, const FMovieSceneMarkedFrame& B) { return A.FrameNumber < B.FrameNumber; });

	if (bForwards)
	{
		for (int32 Index = MarkedFrames.Num() - 2; Index >= 0; --Index)
		{
			if (InFrameNumber >= MarkedFrames[Index].FrameNumber)
			{
				return Index + 1;
			}
		}
		return 0;
	}
	else
	{
		for (int32 Index = 1; Index < MarkedFrames.Num(); ++Index)
		{
			if (InFrameNumber <= MarkedFrames[Index].FrameNumber)
			{
				return Index - 1;
			}
		}
		return MarkedFrames.Num() - 1;
	}
	return INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE
