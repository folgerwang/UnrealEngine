// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "ControlRigDefines.h"
#include "FABRIK.h"
#include "RigUnit_FABRIK.generated.h"

/** 
 * Spec define: TBD
 */

USTRUCT(meta=(DisplayName="FABRIK", Category="Transforms"))
struct CONTROLRIG_API FRigUnit_FABRIK : public FRigUnit
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override;

	FRigUnit_FABRIK()
		: Precision(1.f)
		, MaxIterations(10)
		, FullLimbLength(0.f)
	{}

	UPROPERTY(meta = (Input, Output))
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(EditAnywhere, Category = "FABRIK", meta = (Input))
	FName StartJoint;

	UPROPERTY(EditAnywhere, Category = "FABRIK", meta = (Input))
	FName EndJoint;

	/** Tolerance for final tip location delta from EffectorLocation*/
	UPROPERTY(EditAnywhere, Category = Solver)
	float Precision;

	/** Maximum number of iterations allowed, to control performance. */
	UPROPERTY(EditAnywhere, Category = Solver)
	int32 MaxIterations;

private:
	TArray<FABRIKChainLink> ChainLink;

	// by default, it is full skeleton length
	// we can support stretch option
	float FullLimbLength;
};
