// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraVectorParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraVectorParameterSectionTemplate.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

bool UMovieSceneNiagaraVectorParameterTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneVectorSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNiagaraVectorParameterTrack::CreateNewSection()
{
	UMovieSceneVectorSection* VectorSection = NewObject<UMovieSceneVectorSection>(this, NAME_None, RF_Transactional);
	VectorSection->SetChannelsUsed(ChannelsUsed);
	return VectorSection;
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraVectorParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneVectorSection* VectorSection = Cast<UMovieSceneVectorSection>(&InSection);
	if (VectorSection != nullptr)
	{
		TArray<FMovieSceneFloatChannel> ComponentChannels;
		for (int32 i = 0; i < VectorSection->GetChannelsUsed(); i++)
		{
			ComponentChannels.Add(VectorSection->GetChannel(i));
		}
		return FMovieSceneNiagaraVectorParameterSectionTemplate(GetParameter(), MoveTemp(ComponentChannels), VectorSection->GetChannelsUsed());
	}
	return FMovieSceneEvalTemplatePtr();
}

int32 UMovieSceneNiagaraVectorParameterTrack::GetChannelsUsed() const
{
	return ChannelsUsed;
}

void UMovieSceneNiagaraVectorParameterTrack::SetChannelsUsed(int32 InChannelsUsed)
{
	ChannelsUsed = InChannelsUsed;
}