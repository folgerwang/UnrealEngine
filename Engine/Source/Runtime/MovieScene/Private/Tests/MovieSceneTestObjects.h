// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneTestObjects.generated.h"

USTRUCT()
struct FTestMovieSceneEvalTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	virtual UScriptStruct& GetScriptStructImpl() const { return *StaticStruct(); }
};

UCLASS(MinimalAPI)
class UTestMovieSceneTrack : public UMovieSceneTrack
{
public:

	GENERATED_BODY()

	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override { return SectionArray; }
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual FMovieSceneTrackSegmentBlenderPtr GetTrackSegmentBlender() const override;

	UPROPERTY()
	bool bHighPassFilter;

	UPROPERTY()
	TArray<UMovieSceneSection*> SectionArray;
};

UCLASS(MinimalAPI)
class UTestMovieSceneSection : public UMovieSceneSection
{
public:
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class UTestMovieSceneSequence : public UMovieSceneSequence
{
public:
	GENERATED_BODY()

	UTestMovieSceneSequence(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			MovieScene = ObjInit.CreateDefaultSubobject<UMovieScene>(this, "MovieScene");
		}
	}

	virtual UMovieScene* GetMovieScene() const override { return MovieScene; }

	UPROPERTY()
	UMovieScene* MovieScene;
};
