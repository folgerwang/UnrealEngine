// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PathTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "SequencerSectionPainter.h"
#include "GameFramework/WorldSettings.h"
#include "Tracks/MovieScene3DPathTrack.h"
#include "Sections/MovieScene3DPathSection.h"
#include "ISectionLayoutBuilder.h"
#include "ActorEditorUtils.h"
#include "Components/SplineComponent.h"
#include "MovieSceneToolHelpers.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"


#define LOCTEXT_NAMESPACE "FPathTrackEditor"

/**
 * Class that draws a path section in the sequencer
 */
class F3DPathSection
	: public ISequencerSection
{
public:
	F3DPathSection( UMovieSceneSection& InSection, F3DPathTrackEditor* InPathTrackEditor )
		: Section( InSection )
		, PathTrackEditor(InPathTrackEditor)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}
	
	virtual FText GetSectionTitle() const override 
	{ 
		UMovieScene3DPathSection* PathSection = Cast<UMovieScene3DPathSection>(&Section);
		if (PathSection)
		{
			TSharedPtr<ISequencer> Sequencer = PathTrackEditor->GetSequencer();
			if (Sequencer.IsValid())
			{
				FMovieSceneSequenceID SequenceID = Sequencer->GetFocusedTemplateID();
				if (PathSection->GetConstraintBindingID().GetSequenceID().IsValid())
				{
					// Ensure that this ID is resolvable from the root, based on the current local sequence ID
					FMovieSceneObjectBindingID RootBindingID = PathSection->GetConstraintBindingID().ResolveLocalToRoot(SequenceID, Sequencer->GetEvaluationTemplate().GetHierarchy());
					SequenceID = RootBindingID.GetSequenceID();
				}

				TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = Sequencer->FindBoundObjects(PathSection->GetConstraintBindingID().GetGuid(), SequenceID);
				if (RuntimeObjects.Num() == 1 && RuntimeObjects[0].IsValid())
				{
					if (AActor* Actor = Cast<AActor>(RuntimeObjects[0].Get()))
					{
						return FText::FromString(Actor->GetActorLabel());
					}
				}
			}
		}

		return FText::GetEmpty(); 
	}

	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override 
	{
		return InPainter.PaintSectionBackground();
	}
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SetPath", "Path"), LOCTEXT("SetPathTooltip", "Set path"),
			FNewMenuDelegate::CreateRaw(PathTrackEditor, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, &Section));
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The path track editor */
	F3DPathTrackEditor* PathTrackEditor;
};


F3DPathTrackEditor::F3DPathTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FActorPickerTrackEditor( InSequencer ) 
{ 
}


TSharedRef<ISequencerTrackEditor> F3DPathTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DPathTrackEditor( InSequencer ) );
}


bool F3DPathTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DPathTrack::StaticClass();
}


TSharedRef<ISequencerSection> F3DPathTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	return MakeShareable( new F3DPathSection( SectionObject, this ) );
}


void F3DPathTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass && ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		if (MovieSceneToolHelpers::HasHiddenMobility(ObjectClass))
		{
			return;
		}

		UMovieSceneSection* DummySection = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddPath", "Path"), LOCTEXT("AddPathTooltip", "Adds a path track."),
			FNewMenuDelegate::CreateRaw(this, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, DummySection));
	}
}

bool F3DPathTrackEditor::IsActorPickable(const AActor* const ParentActor, FGuid ObjectBinding, UMovieSceneSection* InSection)
{
	// Can't pick the object that this track binds
	if (GetSequencer()->FindObjectsInCurrentSequence(ObjectBinding).Contains(ParentActor))
	{
		return false;
	}

	// Can't pick the object that this track attaches to
	UMovieScene3DPathSection* PathSection = Cast<UMovieScene3DPathSection>(InSection);
	if (PathSection != nullptr)
	{
		FMovieSceneSequenceID SequenceID = GetSequencer()->GetFocusedTemplateID();
		if (PathSection->GetConstraintBindingID().GetSequenceID().IsValid())
		{
			// Ensure that this ID is resolvable from the root, based on the current local sequence ID
			FMovieSceneObjectBindingID RootBindingID = PathSection->GetConstraintBindingID().ResolveLocalToRoot(SequenceID, GetSequencer()->GetEvaluationTemplate().GetHierarchy());
			SequenceID = RootBindingID.GetSequenceID();
		}

		TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = GetSequencer()->FindBoundObjects(PathSection->GetConstraintBindingID().GetGuid(), SequenceID);
		
		if (RuntimeObjects.Contains(ParentActor))
		{
			return false;
		}
	}

	if (ParentActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(ParentActor) &&
		!ParentActor->IsA( AWorldSettings::StaticClass() ) &&
		!ParentActor->IsPendingKill())
	{			
		TArray<USplineComponent*> SplineComponents;
		ParentActor->GetComponents(SplineComponents);
		if (SplineComponents.Num())
		{
			return true;
		}
	}
	return false;
}


void F3DPathTrackEditor::ActorSocketPicked(const FName SocketName, USceneComponent* Component, FActorPickerID ActorPickerID, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	if (Section != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoSetPath", "Set Path"));

		UMovieScene3DPathSection* PathSection = (UMovieScene3DPathSection*)(Section);

		FMovieSceneObjectBindingID ConstraintBindingID;

		if (ActorPickerID.ExistingBindingID.IsValid())
		{
			ConstraintBindingID = ActorPickerID.ExistingBindingID;
		}
		else if (ActorPickerID.ActorPicked.IsValid())
		{
			FGuid ParentActorId = FindOrCreateHandleToObject(ActorPickerID.ActorPicked.Get()).Handle;
			ConstraintBindingID = FMovieSceneObjectBindingID(ParentActorId, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
		}

		if (ConstraintBindingID.IsValid())
		{
			PathSection->SetConstraintBindingID(ConstraintBindingID);
		}
	}
	else if (ObjectGuid.IsValid())
	{
		TArray<TWeakObjectPtr<>> OutObjects;
		for (TWeakObjectPtr<> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
		{
			OutObjects.Add(Object);
		}
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &F3DPathTrackEditor::AddKeyInternal, OutObjects, ActorPickerID) );
	}
}

FKeyPropertyResult F3DPathTrackEditor::AddKeyInternal( FFrameNumber KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, FActorPickerID ActorPickerID)
{
	FKeyPropertyResult KeyPropertyResult;

	FMovieSceneObjectBindingID ConstraintBindingID;

	if (ActorPickerID.ExistingBindingID.IsValid())
	{
		ConstraintBindingID = ActorPickerID.ExistingBindingID;
	}
	else if (ActorPickerID.ActorPicked.IsValid())
	{
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(ActorPickerID.ActorPicked.Get());
		FGuid ParentActorId = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
		ConstraintBindingID = FMovieSceneObjectBindingID(ParentActorId, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
	}

	if (!ConstraintBindingID.IsValid())
	{
		return KeyPropertyResult;
	}

	for( int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex )
	{
		UObject* Object = Objects[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieScene3DPathTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				// Clamp to next path section's start time or the end of the current sequencer view range
				FFrameNumber PathEndTime = ( GetSequencer()->GetViewRange().GetUpperBoundValue() * Track->GetTypedOuter<UMovieScene>()->GetTickResolution() ).FrameNumber;
	
				for (UMovieSceneSection* Section : Track->GetAllSections())
				{
					FFrameNumber StartTime = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : TNumericLimits<int32>::Lowest();
					if (KeyTime < StartTime)
					{
						if (PathEndTime > StartTime)
						{
							PathEndTime = StartTime;
						}
					}
				}

				int32 Duration = FMath::Max(0, (PathEndTime - KeyTime).Value);
				Cast<UMovieScene3DPathTrack>(Track)->AddConstraint( KeyTime, Duration, NAME_None, NAME_None, ConstraintBindingID );
				KeyPropertyResult.bTrackModified = true;
			}
		}
	}

	return KeyPropertyResult;
}

#undef LOCTEXT_NAMESPACE
