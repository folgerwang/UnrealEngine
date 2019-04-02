// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "Evaluation/MovieSceneLevelVisibilityTemplate.h"


UMovieSceneLevelVisibilitySection::UMovieSceneLevelVisibilitySection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Visibility = ELevelVisibility::Visible;
}


ELevelVisibility UMovieSceneLevelVisibilitySection::GetVisibility() const
{
	return Visibility;
}


void UMovieSceneLevelVisibilitySection::SetVisibility( ELevelVisibility InVisibility )
{
	Visibility = InVisibility;
}


FMovieSceneEvalTemplatePtr UMovieSceneLevelVisibilitySection::GenerateTemplate() const
{
	return FMovieSceneLevelVisibilitySectionTemplate(*this);
}