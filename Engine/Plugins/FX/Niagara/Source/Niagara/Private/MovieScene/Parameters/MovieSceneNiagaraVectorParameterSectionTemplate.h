// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene/Parameters/MovieSceneNiagaraParameterSectionTemplate.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneNiagaraVectorParameterSectionTemplate.generated.h"

USTRUCT()
struct FMovieSceneNiagaraVectorParameterSectionTemplate : public FMovieSceneNiagaraParameterSectionTemplate
{
	GENERATED_BODY()

public:
	FMovieSceneNiagaraVectorParameterSectionTemplate();

	FMovieSceneNiagaraVectorParameterSectionTemplate(FNiagaraVariable InParameter, TArray<FMovieSceneFloatChannel>&& InVectorChannels, int32 InChannelsUsed);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

protected:
	virtual void GetParameterValue(FFrameTime InTime, const TArray<uint8>& InCurrentValueData, TArray<uint8>& OutAnimatedValueData) const override;

private:
	UPROPERTY()
	FMovieSceneFloatChannel VectorChannels[4];

	UPROPERTY()
	int32 ChannelsUsed;
};