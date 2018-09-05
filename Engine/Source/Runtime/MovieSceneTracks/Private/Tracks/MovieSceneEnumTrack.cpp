// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneEnumTrack.h"
#include "Sections/MovieSceneEnumSection.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneEnumTrack::UMovieSceneEnumTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }

void UMovieSceneEnumTrack::PostLoad()
{
	Super::PostLoad();
	SetEnum(Enum);
}

UMovieSceneSection* UMovieSceneEnumTrack::CreateNewSection()
{
	UMovieSceneEnumSection* NewEnumSection = NewObject<UMovieSceneEnumSection>(this, NAME_None, RF_Transactional);
	NewEnumSection->EnumCurve.SetEnum(Enum);
	return NewEnumSection;
}

FMovieSceneEvalTemplatePtr UMovieSceneEnumTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneEnumPropertySectionTemplate(*CastChecked<UMovieSceneEnumSection>(&InSection), *this);
}

void UMovieSceneEnumTrack::SetEnum(UEnum* InEnum)
{
	Enum = InEnum;

	for (UMovieSceneSection* Section : Sections)
	{
		if (UMovieSceneEnumSection* EnumSection = Cast<UMovieSceneEnumSection>(Section))
		{
			EnumSection->EnumCurve.SetEnum(Enum);
		}
	}
}


UEnum* UMovieSceneEnumTrack::GetEnum() const
{
	return Enum;
}
