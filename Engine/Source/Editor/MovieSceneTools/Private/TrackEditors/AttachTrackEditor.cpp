// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/AttachTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "SequencerSectionPainter.h"
#include "GameFramework/WorldSettings.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "ActorEditorUtils.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "MovieSceneToolHelpers.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"


#define LOCTEXT_NAMESPACE "F3DAttachTrackEditor"

/**
 * Class that draws an attach section in the sequencer
 */
class F3DAttachSection
	: public ISequencerSection
{
public:

	F3DAttachSection( UMovieSceneSection& InSection, F3DAttachTrackEditor* InAttachTrackEditor )
		: Section( InSection )
		, AttachTrackEditor(InAttachTrackEditor)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetSectionTitle() const override 
	{ 
		UMovieScene3DAttachSection* AttachSection = Cast<UMovieScene3DAttachSection>(&Section);
		if (AttachSection)
		{
			TSharedPtr<ISequencer> Sequencer = AttachTrackEditor->GetSequencer();
			if (Sequencer.IsValid())
			{
				FMovieSceneSequenceID SequenceID = Sequencer->GetFocusedTemplateID();
				if (AttachSection->GetConstraintBindingID().GetSequenceID().IsValid())
				{
					// Ensure that this ID is resolvable from the root, based on the current local sequence ID
					FMovieSceneObjectBindingID RootBindingID = AttachSection->GetConstraintBindingID().ResolveLocalToRoot(SequenceID, Sequencer->GetEvaluationTemplate().GetHierarchy());
					SequenceID = RootBindingID.GetSequenceID();
				}

				TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = Sequencer->FindBoundObjects(AttachSection->GetConstraintBindingID().GetGuid(), SequenceID);
				if (RuntimeObjects.Num() == 1 && RuntimeObjects[0].IsValid())
				{
					if (AActor* Actor = Cast<AActor>(RuntimeObjects[0].Get()))
					{
						if (AttachSection->AttachSocketName.IsNone())
						{
							return FText::FromString(Actor->GetActorLabel());
						}
						else
						{
							return FText::Format(LOCTEXT("SectionTitleFormat", "{0} ({1})"), FText::FromString(Actor->GetActorLabel()), FText::FromName(AttachSection->AttachSocketName));
						}
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
			LOCTEXT("SetAttach", "Attach"), LOCTEXT("SetAttachTooltip", "Set attach"),
			FNewMenuDelegate::CreateRaw(AttachTrackEditor, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, &Section));
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The attach track editor */
	F3DAttachTrackEditor* AttachTrackEditor;
};


F3DAttachTrackEditor::F3DAttachTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FActorPickerTrackEditor( InSequencer ) 
{
}


F3DAttachTrackEditor::~F3DAttachTrackEditor()
{
}


TSharedRef<ISequencerTrackEditor> F3DAttachTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DAttachTrackEditor( InSequencer ) );
}


bool F3DAttachTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DAttachTrack::StaticClass();
}


TSharedRef<ISequencerSection> F3DAttachTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	return MakeShareable( new F3DAttachSection( SectionObject, this ) );
}


void F3DAttachTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass != nullptr && ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		UMovieSceneSection* DummySection = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddAttach", "Attach"), LOCTEXT("AddAttachTooltip", "Adds an attach track."),
			FNewMenuDelegate::CreateRaw(this, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, DummySection));
	}
}


bool F3DAttachTrackEditor::IsActorPickable(const AActor* const ParentActor, FGuid ObjectBinding, UMovieSceneSection* InSection)
{
	// Can't pick the object that this track binds
	TArrayView<TWeakObjectPtr<>> Objects = GetSequencer()->FindObjectsInCurrentSequence(ObjectBinding);
	if (Objects.Contains(ParentActor))
	{
		return false;
	}

	for (auto Object : Objects)
	{
		if (Object.IsValid())
		{
			AActor* ChildActor = Cast<AActor>(Object.Get());
			if (ChildActor)
			{
				USceneComponent* ChildRoot = ChildActor->GetRootComponent();
				USceneComponent* ParentRoot = ParentActor->GetDefaultAttachComponent();

				if (!ChildRoot || !ParentRoot || ParentRoot->IsAttachedTo(ChildRoot))
				{
					return false;
				}
			}
		}
	}

	if (ParentActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(ParentActor) &&
		!ParentActor->IsA( AWorldSettings::StaticClass() ) &&
		!ParentActor->IsPendingKill())
	{			
		return true;
	}
	return false;
}


void F3DAttachTrackEditor::ActorSocketPicked(const FName SocketName, USceneComponent* Component, FActorPickerID ActorPickerID, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	if (Section != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoSetAttach", "Set Attach"));

		UMovieScene3DAttachSection* AttachSection = (UMovieScene3DAttachSection*)(Section);

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
			AttachSection->SetConstraintBindingID(ConstraintBindingID);
		}

		AttachSection->AttachSocketName = SocketName;			
		AttachSection->AttachComponentName = Component ? Component->GetFName() : NAME_None;
	}
	else if (ObjectGuid.IsValid())
	{
		TArray<TWeakObjectPtr<>> OutObjects;
		for (TWeakObjectPtr<> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
		{
			OutObjects.Add(Object);
		}

		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &F3DAttachTrackEditor::AddKeyInternal, OutObjects, SocketName, Component ? Component->GetFName() : NAME_None, ActorPickerID) );
	}
}

FKeyPropertyResult F3DAttachTrackEditor::AddKeyInternal( FFrameNumber KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, const FName SocketName, const FName ComponentName, FActorPickerID ActorPickerID)
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

	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();

	FMovieSceneSequenceID SequenceID = GetSequencer()->GetFocusedTemplateID();
	if (ConstraintBindingID.GetSequenceID().IsValid())
	{
		// Ensure that this ID is resolvable from the root, based on the current local sequence ID
		FMovieSceneObjectBindingID RootBindingID = ConstraintBindingID.ResolveLocalToRoot(SequenceID, GetSequencer()->GetEvaluationTemplate().GetHierarchy());
		SequenceID = RootBindingID.GetSequenceID();
	}

	FTransform ParentTransform;
	TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = GetSequencer()->FindBoundObjects(ConstraintBindingID.GetGuid(), SequenceID);

	if (RuntimeObjects.Num() == 1 && RuntimeObjects[0].IsValid())
	{
		AActor* ParentActor = Cast<AActor>(RuntimeObjects[0].Get());
		if (ParentActor)
		{
			ParentTransform = ParentActor->GetActorTransform();
			
			if (ParentActor->GetRootComponent()->DoesSocketExist(SocketName))
			{
				ParentTransform = ParentActor->GetRootComponent()->GetSocketTransform(SocketName);
			}
		}
	}
		
	for (int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex)
	{
		UObject* Object = Objects[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject( ObjectHandle, UMovieScene3DAttachTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				FFrameRate TickResolution = Track->GetTypedOuter<UMovieScene>()->GetTickResolution();

				// Clamp to next attach section's start time or the end of the current movie scene range
				FFrameNumber AttachEndTime = MovieScene->GetPlaybackRange().GetUpperBoundValue();

				for (UMovieSceneSection* Section : Track->GetAllSections())
				{
					FFrameNumber StartTime = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : 0;
					if (KeyTime < StartTime)
					{
						if (AttachEndTime > StartTime)
						{
							AttachEndTime = StartTime;
						}
					}
				}

				AActor* Child = Cast<AActor>(Object);
				FTransform RelativeTransform = Child->GetRootComponent()->GetComponentTransform().GetRelativeTransform(ParentTransform);

				int32 Duration = FMath::Max(0, (AttachEndTime - KeyTime).Value);
				Cast<UMovieScene3DAttachTrack>(Track)->AddConstraint( KeyTime, Duration, SocketName, ComponentName, ConstraintBindingID);
				KeyPropertyResult.bTrackModified = true;

				// Compensate
				FName TransformPropertyName("Transform");
				TRange<FFrameNumber> AttachRange(KeyTime, AttachEndTime);

				MovieScene->Modify();

				// Create a transform track if it doesn't exist
				UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(MovieScene->FindTrack<UMovieScene3DTransformTrack>(ObjectHandle));
				if (!TransformTrack)
				{
					FFindOrCreateTrackResult TransformTrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieScene3DTransformTrack::StaticClass());
					TransformTrack = Cast<UMovieScene3DTransformTrack>(TransformTrackResult.Track);
					TransformTrack->SetPropertyNameAndPath(TransformPropertyName, TransformPropertyName.ToString());
				}

				if (!TransformTrack)
				{
					continue;
				}

				// Create a transform section if it doesn't exist
				UMovieScene3DTransformSection* TransformSection = nullptr;
				if (TransformTrack->IsEmpty())
				{
					TransformTrack->Modify();
					TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
					if (TransformSection)
					{
						TransformSection->SetRange(TRange<FFrameNumber>::All());

						TransformTrack->AddSection(*TransformSection);
					}
				}

				// Reuse the transform section if it overlaps and there's no keys
				else if (TransformTrack->GetAllSections().Num() == 1)
				{
					TRange<FFrameNumber> TransformRange = TransformTrack->GetAllSections()[0]->GetRange();
					if (TRange<FFrameNumber>::Intersection(AttachRange, TransformRange).IsEmpty())
					{
						continue;
					}

					bool bEmptyKeys = true;
					TArrayView<FMovieSceneFloatChannel*> Channels = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0])->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
					for (auto Channel : Channels)
					{
						if (Channel->GetTimes().Num() != 0)
						{
							bEmptyKeys = false;
							break;
						}
					}

					if (bEmptyKeys)
					{
						TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]);
					}
				}

				// Create a new additive transform section
				if (!TransformSection)
				{
					TransformTrack->Modify();
					TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
					if (TransformSection)
					{
						for (auto Section : TransformTrack->GetAllSections())
						{
							Section->SetRowIndex(Section->GetRowIndex() + 1);
						}

						TransformSection->SetRange(AttachRange);
						TransformSection->SetBlendType(EMovieSceneBlendType::Additive);
						TransformSection->SetRowIndex(0);
						TransformSection->SetMask(FMovieSceneTransformMask(EMovieSceneTransformChannel::Rotation | EMovieSceneTransformChannel::Translation));
						TransformTrack->AddSection(*TransformSection);

						RelativeTransform = ParentTransform.Inverse();
					}
				}

				if (!TransformSection)
				{
					continue;
				}

				if (!TransformSection->TryModify())
				{
					continue;
				}

				TArrayView<FMovieSceneFloatChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

				FVector DefaultLocation = RelativeTransform.GetLocation();
				FVector DefaultRotation = RelativeTransform.GetRotation().Euler();
				FVector DefaultScale3D = RelativeTransform.GetScale3D();

				Channels[0]->SetDefault(DefaultLocation.X);
				Channels[1]->SetDefault(DefaultLocation.Y);
				Channels[2]->SetDefault(DefaultLocation.Z);

				Channels[3]->SetDefault(DefaultRotation.X);
				Channels[4]->SetDefault(DefaultRotation.Y);
				Channels[5]->SetDefault(DefaultRotation.Z);

				Channels[6]->SetDefault(DefaultScale3D.X);
				Channels[7]->SetDefault(DefaultScale3D.Y);
				Channels[8]->SetDefault(DefaultScale3D.Z);
			}
		}
	}

	return KeyPropertyResult;
}

#undef LOCTEXT_NAMESPACE
