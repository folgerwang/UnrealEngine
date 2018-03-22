// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneTrack.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieScene3DConstraintTrack.generated.h"

/**
 * Base class for constraint tracks (tracks that are dependent upon other objects).
 */
UCLASS( MinimalAPI )
class UMovieScene3DConstraintTrack
	: public UMovieSceneTrack
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Adds a constraint.
	 *
	 * @param Time The time relative to the owning movie scene where the section should be.
	 * @param Duration The length of the constraint section
	 * @param SocketName The socket name for the constraint.
	 * @param ComponentName The name of the component the socket resides in.
	 * @param FMovieSceneObjectBindingID The object binding id to the constraint.
	 */
	virtual void AddConstraint(FFrameNumber Time, int32 Duration, const FName SocketName, const FName ComponentName, const FMovieSceneObjectBindingID& ConstraintBindingID) { }

public:

	// UMovieSceneTrack interface

    virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;

protected:

	/** List of all constraint sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> ConstraintSections;
};
