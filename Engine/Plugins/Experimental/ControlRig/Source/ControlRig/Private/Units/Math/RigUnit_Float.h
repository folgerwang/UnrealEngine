// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "MathLibrary.h"
#include "RigUnit_Float.generated.h"

/** Two args and a result of float type */
USTRUCT(meta=(Abstract))
struct FRigUnit_BinaryFloatOp : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	float Argument0;

	UPROPERTY(meta=(Input))
	float Argument1;

	UPROPERTY(meta=(Output))
	float Result;
};

USTRUCT(meta=(DisplayName="Multiply", Category="Math|Float"))
struct FRigUnit_Multiply_FloatFloat : public FRigUnit_BinaryFloatOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Multiply(Argument0, Argument1);
	}
};

USTRUCT(meta=(DisplayName="Add", Category="Math|Float"))
struct FRigUnit_Add_FloatFloat : public FRigUnit_BinaryFloatOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Add(Argument0, Argument1);
	}
};

USTRUCT(meta=(DisplayName="Subtract", Category="Math|Float"))
struct FRigUnit_Subtract_FloatFloat : public FRigUnit_BinaryFloatOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Subtract(Argument0, Argument1);
	}
};

USTRUCT(meta=(DisplayName="Divide", Category="Math|Float"))
struct FRigUnit_Divide_FloatFloat : public FRigUnit_BinaryFloatOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Divide(Argument0, Argument1);
	}
};