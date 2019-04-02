// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigBindingTrack.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "Templates/Casts.h"
#include "Sequencer/ControlRigBindingTemplate.h"

#define LOCTEXT_NAMESPACE "ControlRigBindingTrack"

FMovieSceneEvalTemplatePtr UControlRigBindingTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneSpawnSection* Section = CastChecked<const UMovieSceneSpawnSection>(&InSection);
	return FControlRigBindingTemplate(*Section);
}

#if WITH_EDITORONLY_DATA

FText UControlRigBindingTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Bound");
}

#endif

#undef LOCTEXT_NAMESPACE
