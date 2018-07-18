// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "Constraint.h"
#include "ControlRigDefines.h"
#include "RigUnit_AimConstraint.generated.h"

/** 
 * Spec define: https://wiki.it.epicgames.net/pages/viewpage.action?spaceKey=TeamOnline&title=Aim+Constraint
 */

 /*
 ENUM: Aim Mode (Default: Aim At Target Transform )  # How to perform an aim
 Aim At Target Transforms
 Orient To Target Transforms
 */

UENUM()
enum class EAimMode : uint8
{
	/** Aim at Target Transform*/
	AimAtTarget,

	/** Orient to Target Transform */
	OrientToTarget,

	/** MAX - invalid */
	MAX,
};

USTRUCT()
struct FAimTarget
{
	GENERATED_BODY()

	// # Target Weight
	UPROPERTY(EditAnywhere, Category = FAimTarget)
	float Weight;

	// # Aim at/Align to this Transform
	UPROPERTY(EditAnywhere, Category = FAimTarget)
	FTransform Transform;

	//# Orient To Target Transforms mode only : Vector in the space of Target Transform to which the Aim Vector will be aligned
	UPROPERTY(EditAnywhere, Category = FAimTarget)
	FVector AlignVector;
};

USTRUCT(meta=(DisplayName="Aim Constraint", Category="Transforms"))
struct FRigUnit_AimConstraint : public FRigUnit
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override;

private:
	UPROPERTY(meta = (Input, Output))
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	FName Joint;

	//# How to perform an aim
	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	EAimMode AimMode;

	//# How to perform an upvector stabilization
	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	EAimMode UpMode;

	// # Vector in the space of Named joint which will be aligned to the aim target
	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	FVector AimVector;

	//# Vector in the space of Named joint which will be aligned to the up target for stabilization
	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	FVector UpVector;

	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	TArray<FAimTarget> AimTargets;

	UPROPERTY(EditAnywhere, Category = FRigUnit_AimConstraint, meta = (Input))
	TArray<FAimTarget> UpTargets;

	// note that Targets.Num () != ConstraintData.Num()
	TArray<FConstraintData>	ConstraintData;
};
