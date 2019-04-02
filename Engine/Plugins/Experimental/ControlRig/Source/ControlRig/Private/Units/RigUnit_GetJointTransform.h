// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "Constraint.h"
#include "ControlRigDefines.h"
#include "RigUnit_GetJointTransform.generated.h"


USTRUCT(meta=(DisplayName="Get Joint Transform", Category="Transforms"))
struct FRigUnit_GetJointTransform : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetJointTransform()
		: Type(ETransformGetterType::Current)
		, TransformSpace(ETransformSpaceMode::GlobalSpace)
	{}

	virtual void Execute(const FRigUnitContext& InContext) override;

	UPROPERTY(meta = (Input))
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(meta = (Input))
	FName Joint;

	UPROPERTY(meta = (Input))
	ETransformGetterType Type;

	UPROPERTY(meta = (Input))
	ETransformSpaceMode TransformSpace;

	// Transform op option. Use if ETransformSpace is BaseTransform
	UPROPERTY(meta = (Input))
	FTransform BaseTransform;

	// Transform op option. Use if ETransformSpace is BaseJoint
	UPROPERTY(meta = (Input))
	FName BaseJoint;

	// possibly space, relative transform so on can be input
	UPROPERTY(meta=(Output))
	FTransform Output;

private:
	FTransform GetBaseTransform(int32 JointIndex, const FRigHierarchy* CurrentHierarchy, bool bUseInitial) const;
};
