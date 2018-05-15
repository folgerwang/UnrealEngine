// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit.h"
#include "Constraint.h"
#include "ControlRigControl.h"
#include "EulerTransform.h"
#include "RigUnit_Control.generated.h"

/** A control unit used to drive a transform from an external source */
USTRUCT(meta=(DisplayName="Control", Category="Controls", ShowVariableNameInTitle))
struct CONTROLRIG_API FRigUnit_Control : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_Control()
		: Transform(FEulerTransform::Identity)
		, Base(FTransform::Identity)
		, Result(FTransform::Identity)
	{
	}

	virtual void Execute(const FRigUnitContext& InContext) override;

	/** Combine Transform and Base to make the resultant transform */
	FTransform GetResultantTransform() const;

	/** Combine Transform and Base to make the resultant transform (as a matrix) */
	FMatrix GetResultantMatrix() const;

	/** Set the transform using a resultant transform (already incorporating Base) */
	void SetResultantTransform(const FTransform& InResultantTransform);

	/** Set the transform using a resultant matrix (already incorporating Base) */
	void SetResultantMatrix(const FMatrix& InResultantMatrix);

	/** Get the local transform (i.e. without base) with filter applied */
	FEulerTransform GetFilteredTransform() const;

#if WITH_EDITORONLY_DATA
	/** Actor class to use to display this in the viewport */
	UPROPERTY(EditAnywhere, Category="Control")
	TSubclassOf<AControlRigControl> ControlClass;
#endif

	/** The transform of this control */
	UPROPERTY(EditAnywhere, Category="Control", Interp)
	FEulerTransform Transform;

	/** The base that transform is relative to */
	UPROPERTY(meta=(Input))
	FTransform Base;

	/** The initial transform that The Transform is initialized to. */
	UPROPERTY(meta = (Input))
	FTransform InitTransform;

	/** The resultant transform of this unit (Base * Filter(Transform)) */
	UPROPERTY(meta=(Output))
	FTransform Result;

	/** The filter determines what axes can be manipulated by the in-viewport widgets */
	UPROPERTY(meta=(Input))
 	FTransformFilter Filter;
};