// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MovieSceneSection.h"
#include "MovieSceneEventSectionBase.generated.h"


/**
 * Base class for all event sections. Manages dirtying the section and track on recompilation of the director blueprint.
 */
UCLASS(MinimalAPI)
class UMovieSceneEventSectionBase
	: public UMovieSceneSection
{
public:
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	/**
	 * Assign the director blueprint pointer for this event section.
	 */
	MOVIESCENETRACKS_API void SetDirectorBlueprint(UBlueprint* InBlueprint);

protected:

	virtual void Serialize(FArchive& Ar) override;
	virtual void OnBlueprintRecompiled(UBlueprint*) {}

private:

	/** A handle to the blueprint recompiled delegate binding for this section's director blueprint */
	UPROPERTY()
	TWeakObjectPtr<UBlueprint> DirectorBlueprint;

	/** A handle to the blueprint recompiled delegate binding for this section's director blueprint */
	FDelegateHandle OnBlueprintCompiledHandle;

#endif // WITH_EDITORONLY_DATA
};