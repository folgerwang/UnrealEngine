// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "Constraint.h"
#include "ControlRigDefines.h"
#include "RigUnit_ApplyFK.generated.h"

/**
* Spec define: https://wiki.it.epicgames.net/display/TeamOnline/Apply+Fk
*/

UENUM()
enum class EApplyTransformMode : uint8
{
	/** Override existing motion */
	Override,

	/** Additive to existing motion*/
	Additive,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

USTRUCT(meta=(DisplayName="Apply FK", Category="Transforms"))
struct FRigUnit_ApplyFK : public FRigUnit
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override;

	UPROPERTY(meta = (Input, Output))
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(EditAnywhere, Category = "ApplyFK", meta = (Input))
	FName Joint;

	UPROPERTY(meta=(Input))
	FTransform Transform;

	/** The filter determines what axes can be manipulated by the in-viewport widgets */
	UPROPERTY(EditAnywhere, Category = "ApplyFK", meta = (Input))
	FTransformFilter Filter;

	UPROPERTY(EditAnywhere, Category = "ApplyFK", meta = (Input))
	EApplyTransformMode ApplyTransformMode;

	UPROPERTY(EditAnywhere, Category = "ApplyFK", meta = (Input))
	ETransformSpaceMode ApplyTransformSpace;

	// Transform op option. Use if ETransformSpace is BaseTransform
	UPROPERTY(EditAnywhere, Category = "ApplyFK", meta = (Input))
	FTransform BaseTransform;

	// Transform op option. Use if ETransformSpace is BaseJoint
	UPROPERTY(EditAnywhere, Category = "ApplyFK", meta = (Input))
	FName BaseJoint;

private:
	FTransform GetBaseTransform(int32 JointIndex, const FRigHierarchy* CurrentHierarchy) const;
};
