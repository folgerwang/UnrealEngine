// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "UVMapSettings.generated.h"

USTRUCT(BlueprintType)
struct MESHDESCRIPTIONOPERATIONS_API FUVMapSettings
{
	GENERATED_BODY()

	/** Length, width, height of the UV mapping gizmo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = UVMapSettings)
	FVector Size;

	/** Tiling of the UV mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = UVMapSettings)
	FVector2D UVTile;

	/** Axis of the UV mapping gizmo that is aligned with the local up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = UVMapSettings)
	int Axis;

	/** Position of the UV mapping gizmo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GizmoTransform)
	FVector Position;

	/** Rotation axis of the UV mapping gizmo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GizmoTransform)
	FVector RotationAxis;

	/** Rotation angle (in degrees) of the UV mapping gizmo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GizmoTransform)
	float RotationAngle;

	/** Scale of the UV mapping gizmo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GizmoTransform)
	FVector Scale;

	/** Default settings */
	FUVMapSettings()
		: Size(1.0f)
		, UVTile(1.0f, 1.0f)
		, Axis(0)
		, Position(0.0f)
		, RotationAxis(FVector::UpVector)
		, RotationAngle(0.f)
		, Scale(1.0f)
	{
	}
};