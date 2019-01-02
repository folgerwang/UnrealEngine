// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EulerTransform.generated.h"

USTRUCT(BlueprintType)
struct ANIMATIONCORE_API FEulerTransform
{
	GENERATED_BODY()

	/**
	 * The identity transformation (Rotation = FRotator::ZeroRotator, Translation = FVector::ZeroVector, Scale = (1,1,1)).
	 */
	static const FEulerTransform Identity;

	FEulerTransform()
		: Location(ForceInitToZero)
		, Rotation(ForceInitToZero)
		, Scale(ForceInitToZero)
	{
	}

	FEulerTransform(const FVector& InLocation, const FRotator& InRotation, const FVector& InScale)
		: Location(InLocation)
		, Rotation(InRotation)
		, Scale(InScale)
	{
	}

	/** The translation of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FVector Location;

	/** The rotation of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FRotator Rotation;

	/** The scale of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FVector Scale;

	/** Convert to an FTransform */
	FTransform ToFTransform() const
	{
		return FTransform(Rotation.Quaternion(), Location, Scale);
	}

	/** Convert from an FTransform */
	void FromFTransform(const FTransform& InTransform)
	{
		Location = InTransform.GetLocation();
		Rotation = InTransform.GetRotation().Rotator();
		Scale = InTransform.GetScale3D();
	}
};