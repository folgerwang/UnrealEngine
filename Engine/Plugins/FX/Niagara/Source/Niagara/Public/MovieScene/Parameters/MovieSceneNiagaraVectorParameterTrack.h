// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene/Parameters/MovieSceneNiagaraParameterTrack.h"
#include "MovieSceneNiagaraVectorParameterTrack.generated.h"

/** A track for animating float niagara parameters. */
UCLASS(MinimalAPI)
class UMovieSceneNiagaraVectorParameterTrack : public UMovieSceneNiagaraParameterTrack
{
	GENERATED_BODY()

public:
	/** UMovieSceneTrack interface. */
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	NIAGARA_API int32 GetChannelsUsed() const;
	NIAGARA_API void SetChannelsUsed(int32 InChannelsUsed);

private:
	UPROPERTY()
	int32 ChannelsUsed;
};