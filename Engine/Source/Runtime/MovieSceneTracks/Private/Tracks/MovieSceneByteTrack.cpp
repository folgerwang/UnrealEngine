// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneByteTrack.h"
#include "Sections/MovieSceneByteSection.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneByteTrack::UMovieSceneByteTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }

void UMovieSceneByteTrack::PostLoad()
{
	Super::PostLoad();

	SetEnum(Enum);
}

bool UMovieSceneByteTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneByteSection::StaticClass();
}

UMovieSceneSection* UMovieSceneByteTrack::CreateNewSection()
{
	UMovieSceneByteSection* NewByteSection = NewObject<UMovieSceneByteSection>(this, NAME_None, RF_Transactional);
	NewByteSection->ByteCurve.SetEnum(Enum);
	return NewByteSection;
}

FMovieSceneEvalTemplatePtr UMovieSceneByteTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneBytePropertySectionTemplate(*CastChecked<UMovieSceneByteSection>(&InSection), *this);
}

void UMovieSceneByteTrack::SetEnum(UEnum* InEnum)
{
	Enum = InEnum;
	for (UMovieSceneSection* Section : Sections)
	{
		if (UMovieSceneByteSection* ByteSection = Cast<UMovieSceneByteSection>(Section))
		{
			ByteSection->ByteCurve.SetEnum(Enum);
		}
	}
}


UEnum* UMovieSceneByteTrack::GetEnum() const
{
	return Enum;
}
