// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "MathLibrary.h"
#include "RigUnit_Transform.generated.h"

/** Two args and a result of Transform type */
USTRUCT(meta=(Abstract))
struct FRigUnit_BinaryTransformOp : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FTransform Argument0;

	UPROPERTY(meta=(Input))
	FTransform Argument1;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

USTRUCT(meta=(DisplayName="Multiply(Transform)", Category="Math|Transform"))
struct FRigUnit_MultiplyTransform : public FRigUnit_BinaryTransformOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Argument0*Argument1;
	}
};

USTRUCT(meta = (DisplayName = "GetRelativeTransform", Category = "Math|Transform"))
struct FRigUnit_GetRelativeTransform : public FRigUnit_BinaryTransformOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Argument0.GetRelativeTransform(Argument1);
	}
};

