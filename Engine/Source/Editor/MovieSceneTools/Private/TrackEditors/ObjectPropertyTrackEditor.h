// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyTrackEditor.h"
#include "Tracks/MovieSceneObjectPropertyTrack.h"

class FObjectPropertyTrackEditor : public FPropertyTrackEditor<UMovieSceneObjectPropertyTrack>
{
public:

	/** Constructor. */
	FObjectPropertyTrackEditor(TSharedRef<ISequencer> InSequencer);

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromPropertyType(UObjectPropertyBase::StaticClass()) });
	}

	/** Factory function */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	//~ ISequencerTrackEditor interface
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys) override;
	virtual void InitializeNewTrack(UMovieSceneObjectPropertyTrack* NewTrack, FPropertyChangedParams PropertyChangedParams) override;
};
