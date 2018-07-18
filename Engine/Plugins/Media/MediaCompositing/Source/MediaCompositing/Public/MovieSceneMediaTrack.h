// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "MovieSceneNameableTrack.h"
#include "UObject/ObjectMacros.h"

#include "MovieSceneMediaTrack.generated.h"

class UMediaSource;


/**
 * Implements a movie scene track for media playback.
 */
UCLASS(MinimalAPI)
class UMovieSceneMediaTrack
	: public UMovieSceneNameableTrack
{
public:

	GENERATED_BODY()

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer The object initializer.
	 */
	UMovieSceneMediaTrack(const FObjectInitializer& ObjectInitializer);

public:

	/** Adds a new media source to the track. */
	virtual UMovieSceneSection* AddNewMediaSourceOnRow(UMediaSource& MediaSource, FFrameNumber Time, int32 RowIndex);

	/** Adds a new media source on the next available/non-overlapping row. */
	virtual UMovieSceneSection* AddNewMediaSource(UMediaSource& MediaSource, FFrameNumber Time) { return AddNewMediaSourceOnRow(MediaSource, Time, INDEX_NONE); }

public:

	//~ UMovieScenePropertyTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool SupportsMultipleRows() const override { return true; }

private:

	/** List of all master media sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> MediaSections;
};
