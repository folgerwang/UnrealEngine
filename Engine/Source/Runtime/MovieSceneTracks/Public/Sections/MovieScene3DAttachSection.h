// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	FName AttachSocketName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	FName AttachComponentName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	EAttachmentRule AttachmentLocationRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	EAttachmentRule AttachmentRotationRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	EAttachmentRule AttachmentScaleRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	EDetachmentRule DetachmentLocationRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	EDetachmentRule DetachmentRotationRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Attach")
	EDetachmentRule DetachmentScaleRule;
};
