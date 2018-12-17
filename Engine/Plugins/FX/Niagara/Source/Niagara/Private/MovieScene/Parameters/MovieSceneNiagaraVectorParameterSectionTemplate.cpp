// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraVectorParameterSectionTemplate.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "IMovieScenePlayer.h"

FMovieSceneNiagaraVectorParameterSectionTemplate::FMovieSceneNiagaraVectorParameterSectionTemplate() : ChannelsUsed(0)
{
}

FMovieSceneNiagaraVectorParameterSectionTemplate::FMovieSceneNiagaraVectorParameterSectionTemplate(FNiagaraVariable InParameter, TArray<FMovieSceneFloatChannel>&& InVectorChannels, int32 InChannelsUsed)
	: FMovieSceneNiagaraParameterSectionTemplate(InParameter)
	, ChannelsUsed(InChannelsUsed)
{
	for (int32 i = 0; i < ChannelsUsed; i++)
	{
		VectorChannels[i] = InVectorChannels[i];
	}
}

void FMovieSceneNiagaraVectorParameterSectionTemplate::GetParameterValue(FFrameTime InTime, const TArray<uint8>& InCurrentValueData, TArray<uint8>& OutAnimatedValueData) const
{
	if (ChannelsUsed == 2)
	{
		FVector2D const* CurrentValue = (FVector2D const*)InCurrentValueData.GetData();
		FVector2D AnimatedValue = *CurrentValue;

		VectorChannels[0].Evaluate(InTime, AnimatedValue.X);
		VectorChannels[1].Evaluate(InTime, AnimatedValue.Y);

		OutAnimatedValueData.AddUninitialized(sizeof(FVector2D));
		FMemory::Memcpy(OutAnimatedValueData.GetData(), (uint8*)&AnimatedValue, sizeof(FVector2D));
	}
	else if (ChannelsUsed == 3)
	{
		FVector const* CurrentValue = (FVector const*)InCurrentValueData.GetData();
		FVector AnimatedValue = *CurrentValue;

		VectorChannels[0].Evaluate(InTime, AnimatedValue.X);
		VectorChannels[1].Evaluate(InTime, AnimatedValue.Y);
		VectorChannels[2].Evaluate(InTime, AnimatedValue.Z);

		OutAnimatedValueData.AddUninitialized(sizeof(FVector));
		FMemory::Memcpy(OutAnimatedValueData.GetData(), (uint8*)&AnimatedValue, sizeof(FVector));
	}
	else if (ChannelsUsed == 4)
	{
		FVector4 const* CurrentValue = (FVector4 const*)InCurrentValueData.GetData();
		FVector4 AnimatedValue = *CurrentValue;

		VectorChannels[0].Evaluate(InTime, AnimatedValue.X);
		VectorChannels[1].Evaluate(InTime, AnimatedValue.Y);
		VectorChannels[2].Evaluate(InTime, AnimatedValue.Z);
		VectorChannels[3].Evaluate(InTime, AnimatedValue.W);

		OutAnimatedValueData.AddUninitialized(sizeof(FVector4));
		FMemory::Memcpy(OutAnimatedValueData.GetData(), (uint8*)&AnimatedValue, sizeof(FVector4));
	}
}