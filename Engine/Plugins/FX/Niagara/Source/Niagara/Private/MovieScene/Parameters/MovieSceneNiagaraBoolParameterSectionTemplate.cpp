// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraBoolParameterSectionTemplate.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "IMovieScenePlayer.h"

FMovieSceneNiagaraBoolParameterSectionTemplate::FMovieSceneNiagaraBoolParameterSectionTemplate()
{
}

FMovieSceneNiagaraBoolParameterSectionTemplate::FMovieSceneNiagaraBoolParameterSectionTemplate(FNiagaraVariable InParameter, const FMovieSceneBoolChannel& InBoolChannel)
	: FMovieSceneNiagaraParameterSectionTemplate(InParameter)
	, BoolChannel(InBoolChannel)
{
}

void FMovieSceneNiagaraBoolParameterSectionTemplate::GetParameterValue(FFrameTime InTime, const TArray<uint8>& InCurrentValueData, TArray<uint8>& OutAnimatedValueData) const
{
	FNiagaraBool const* CurrentValue = (FNiagaraBool const*)InCurrentValueData.GetData();
	FNiagaraBool AnimatedNiagaraValue = *CurrentValue;

	bool AnimatedValue;
	if (BoolChannel.Evaluate(InTime, AnimatedValue))
	{
		AnimatedNiagaraValue.SetValue(AnimatedValue);
	}
	
	OutAnimatedValueData.AddUninitialized(sizeof(FNiagaraBool));
	FMemory::Memcpy(OutAnimatedValueData.GetData(), (uint8*)&AnimatedValue, sizeof(FNiagaraBool));
}