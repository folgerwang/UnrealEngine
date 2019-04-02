// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScene3DAttachTrack.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Evaluation/MovieScene3DAttachTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Templates/Casts.h"


#define LOCTEXT_NAMESPACE "MovieScene3DAttachTrack"


UMovieScene3DAttachTrack::UMovieScene3DAttachTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }

FMovieSceneEvalTemplatePtr UMovieScene3DAttachTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieScene3DAttachSectionTemplate(*CastChecked<UMovieScene3DAttachSection>(&InSection));
}

void UMovieScene3DAttachTrack::AddConstraint(FFrameNumber KeyTime, int32 Duration, const FName SocketName, const FName ComponentName, const FMovieSceneObjectBindingID& ConstraintBindingID)
{
	// add the section
	UMovieScene3DAttachSection* NewSection = NewObject<UMovieScene3DAttachSection>(this, NAME_None, RF_Transactional);
	NewSection->SetAttachTargetID(ConstraintBindingID);
	NewSection->InitialPlacement(ConstraintSections, KeyTime, Duration, SupportsMultipleRows());
	NewSection->AttachSocketName = SocketName;
	NewSection->AttachComponentName = ComponentName;

	ConstraintSections.Add(NewSection);
}

bool UMovieScene3DAttachTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieScene3DAttachSection::StaticClass();
}

UMovieSceneSection* UMovieScene3DAttachTrack::CreateNewSection()
{
	UMovieScene3DAttachSection* NewSection = NewObject<UMovieScene3DAttachSection>(this, NAME_None, RF_Transactional);

	ConstraintSections.Add(NewSection);

	return NewSection;
}

#if WITH_EDITORONLY_DATA
FText UMovieScene3DAttachTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Attach");
}
#endif


#undef LOCTEXT_NAMESPACE
