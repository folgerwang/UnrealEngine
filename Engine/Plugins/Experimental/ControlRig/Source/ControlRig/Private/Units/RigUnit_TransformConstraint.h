// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "Constraint.h"
#include "ControlRigDefines.h"
#include "RigUnit_TransformConstraint.generated.h"

/** 
 * Spec define: https://wiki.it.epicgames.net/display/TeamOnline/Transform+Constraint
 */

USTRUCT()
struct FConstraintTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "FConstraintTarget")
	FTransform	Transform;

	UPROPERTY(EditAnywhere, Category = "FConstraintTarget")
	float Weight;

	UPROPERTY(EditAnywhere, Category = "FConstraintTarget")
	bool bMaintainOffset;

	UPROPERTY(EditAnywhere, Category = "FConstraintTarget")
	FTransformFilter Filter;

	FConstraintTarget()
		: Weight (1.f)
		, bMaintainOffset(true)
	{}
};

USTRUCT(meta=(DisplayName="Constraint", Category="Transforms"))
struct FRigUnit_TransformConstraint : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_TransformConstraint()
		: BaseTransformSpace(ETransformSpaceMode::GlobalSpace)
	{}

	virtual void Execute(const FRigUnitContext& InContext) override;

	UPROPERTY(meta = (Input, Output))
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(EditAnywhere, Category = "Constraint", meta = (Input))
	FName Joint;

	UPROPERTY(EditAnywhere, Category = "Constraint", meta = (Input))
	ETransformSpaceMode BaseTransformSpace;

	// Transform op option. Use if ETransformSpace is BaseTransform
	UPROPERTY(EditAnywhere, Category = "Constraint", meta = (Input))
	FTransform BaseTransform;
	// Transform op option. Use if ETransformSpace is BaseJoint
	UPROPERTY(EditAnywhere, Category = "Constraint", meta = (Input))
	FName BaseJoint;

	UPROPERTY(EditAnywhere, Category = "Constraint", meta = (Input))
	TArray<FConstraintTarget> Targets;

private:
	// note that Targets.Num () != ConstraintData.Num()
	TArray<FConstraintData>	ConstraintData;

	void AddConstraintData(ETransformConstraintType ConstraintType, const int32 TargetIndex, const FTransform& SourceTransform, const FTransform& InBaseTransform);

	TMap<int32, int32> ConstraintDataToTargets;
};
