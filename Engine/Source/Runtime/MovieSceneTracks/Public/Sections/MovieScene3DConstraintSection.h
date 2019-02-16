// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieScene3DConstraintSection.generated.h"


/**
 * Base class for 3D constraint section
 */
UCLASS(MinimalAPI)
class UMovieScene3DConstraintSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets the constraint id for this section */
	virtual void SetConstraintId(const FGuid& InId);

	/** Sets the constraint id for this section based off of the pasted in sequence id */
	virtual void SetConstraintId(const FGuid& InConstraintId, const FMovieSceneSequenceID& SequenceID);

	/** Gets the constraint binding for this Constraint section */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	const FMovieSceneObjectBindingID& GetConstraintBindingID() const
	{
		return ConstraintBindingID;
	}

	/** Sets the constraint binding for this Constraint section */
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	void SetConstraintBindingID(const FMovieSceneObjectBindingID& InConstraintBindingID)
	{
		ConstraintBindingID = InConstraintBindingID;
	}

public:

	//~ UMovieSceneSection interface

	virtual void OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap) override;
	
	virtual void GetReferencedBindings(TArray<FGuid>& OutBindings) override;

	/** ~UObject interface */
	virtual void PostLoad() override;

protected:

	/** The possessable guid that this constraint uses */
	UPROPERTY()
	FGuid ConstraintId_DEPRECATED;

	/** The constraint binding that this movie Constraint uses */
	UPROPERTY(EditAnywhere, Category="Section")
	FMovieSceneObjectBindingID ConstraintBindingID;

};
