// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Sections/MovieScene3DConstraintSection.h"
#include "MovieScene3DAttachSection.generated.h"

class AActor;
class USceneComponent;

/**
 * A 3D Attach section
 */
UCLASS(MinimalAPI)
class UMovieScene3DAttachSection
	: public UMovieScene3DConstraintSection
{
	GENERATED_UCLASS_BODY()

public:

	/** 
	 * Sets the object to attach to
	 *
	 * @param InAttachBindingId The object binding id to the path
	 */
	void SetAttachTargetID(const FMovieSceneObjectBindingID& InAttachBindingID);

public:

	UPROPERTY(EditAnywhere, Category="Attach")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere, Category="Attach")
	FName AttachComponentName;

	UPROPERTY(EditAnywhere, Category="Attach")
	EAttachmentRule AttachmentLocationRule;

	UPROPERTY(EditAnywhere, Category="Attach")
	EAttachmentRule AttachmentRotationRule;

	UPROPERTY(EditAnywhere, Category="Attach")
	EAttachmentRule AttachmentScaleRule;

	UPROPERTY(EditAnywhere, Category="Attach")
	EDetachmentRule DetachmentLocationRule;

	UPROPERTY(EditAnywhere, Category="Attach")
	EDetachmentRule DetachmentRotationRule;

	UPROPERTY(EditAnywhere, Category="Attach")
	EDetachmentRule DetachmentScaleRule;
};
