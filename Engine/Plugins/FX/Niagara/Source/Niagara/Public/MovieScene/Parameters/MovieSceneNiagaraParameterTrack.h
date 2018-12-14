// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene/MovieSceneNiagaraTrack.h"
#include "NiagaraTypes.h"
#include "MovieSceneNiagaraParameterTrack.generated.h"

/** A base class for tracks that animate niagara parameters. */
UCLASS(MinimalAPI)
class UMovieSceneNiagaraParameterTrack : public UMovieSceneNiagaraTrack
{
	GENERATED_BODY()

public:
	/** Gets the parameter for this parameter track. */
	NIAGARA_API const FNiagaraVariable& GetParameter() const;

	/** Sets the parameter for this parameter track .*/
	NIAGARA_API void SetParameter(FNiagaraVariable InParameter);

private:
	/** The parameter for the parameter this track animates. */
	UPROPERTY()
	FNiagaraVariable Parameter;
};