// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneStringTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneStringSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"


#define LOCTEXT_NAMESPACE "MovieSceneStringTrack"


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneStringTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


UMovieSceneSection* UMovieSceneStringTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneStringSection::StaticClass(), NAME_None, RF_Transactional);
}


FMovieSceneEvalTemplatePtr UMovieSceneStringTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneStringPropertySectionTemplate(*CastChecked<UMovieSceneStringSection>(&InSection), *this);
}

const TArray<UMovieSceneSection*>& UMovieSceneStringTrack::GetAllSections() const
{
	return Sections;
}


bool UMovieSceneStringTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneStringTrack::IsEmpty() const
{
	return (Sections.Num() == 0);
}


void UMovieSceneStringTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneStringTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneStringTrack::GetDefaultDisplayName() const
{ 
	return LOCTEXT("TrackName", "Strings"); 
}

#endif


#undef LOCTEXT_NAMESPACE
