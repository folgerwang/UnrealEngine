// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneCameraCutSection.generated.h"

struct FMovieSceneSequenceID;
class IMovieScenePlayer;
class UCameraComponent;

/**
 * Movie CameraCuts are sections on the CameraCuts track, that show what the viewer "sees"
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraCutSection 
	: public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneCameraCutSection(const FObjectInitializer& Init)
		: Super(Init)
	{
		EvalOptions.EnableAndSetCompletionMode
			(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
				EMovieSceneCompletionMode::RestoreState : 
				EMovieSceneCompletionMode::ProjectDefault);
	}

	/** Sets the camera binding for this CameraCut section. Evaluates from the sequence binding ID */
	void SetCameraGuid(const FGuid& InGuid)
	{
		SetCameraBindingID(FMovieSceneObjectBindingID(InGuid, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local));
	}

	/** Gets the camera binding for this CameraCut section */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	const FMovieSceneObjectBindingID& GetCameraBindingID() const
	{
		return CameraBindingID;
	}

	/** Sets the camera binding for this CameraCut section */
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	void SetCameraBindingID(const FMovieSceneObjectBindingID& InCameraBindingID)
	{
		CameraBindingID = InCameraBindingID;
	}

	//~ UMovieSceneSection interface
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
	virtual void OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap) override;
	virtual void GetReferencedBindings(TArray<FGuid>& OutBindings) override;

	/** ~UObject interface */
	virtual void PostLoad() override;

	/**
	 * Resolve a camera component for this cut section from the specified player and sequence ID
	 *
	 * @param Player     The sequence player to use to resolve the object binding for this camera
	 * @param SequenceID The sequence ID for the specific instance that this section exists within
	 *
	 * @return A camera component to be used for this cut section, or nullptr if one was not found.
	 */
	MOVIESCENETRACKS_API UCameraComponent* GetFirstCamera(IMovieScenePlayer& Player, FMovieSceneSequenceID SequenceID) const;

private:

	/** The camera possessable or spawnable that this movie CameraCut uses */
	UPROPERTY()
	FGuid CameraGuid_DEPRECATED;

	/** The camera binding that this movie CameraCut uses */
	UPROPERTY(EditAnywhere, Category="Section")
	FMovieSceneObjectBindingID CameraBindingID;

#if WITH_EDITORONLY_DATA
public:
	/** @return The thumbnail reference frame offset from the start of this section */
	float GetThumbnailReferenceOffset() const
	{
		return ThumbnailReferenceOffset;
	}

	/** Set the thumbnail reference offset */
	void SetThumbnailReferenceOffset(float InNewOffset)
	{
		Modify();
		ThumbnailReferenceOffset = InNewOffset;
	}

private:

	/** The reference frame offset for single thumbnail rendering */
	UPROPERTY()
	float ThumbnailReferenceOffset;
#endif
};
