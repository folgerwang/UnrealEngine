// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "MathLibrary.h"
#include "RigUnit_Vector.generated.h"

/** Two args and a result of Vector type */
USTRUCT(meta=(Abstract))
struct FRigUnit_BinaryVectorOp : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FVector Argument0;

	UPROPERTY(meta=(Input))
	FVector Argument1;

	UPROPERTY(meta=(Output))
	FVector Result;
};

USTRUCT(meta=(DisplayName="Multiply(Vector)", Category="Math|Vector"))
struct FRigUnit_Multiply_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Multiply(Argument0, Argument1);
	}
};

USTRUCT(meta=(DisplayName="Add(Vector)", Category="Math|Vector"))
struct FRigUnit_Add_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Add(Argument0, Argument1);
	}
};

USTRUCT(meta=(DisplayName="Subtract(Vector)", Category="Math|Vector"))
struct FRigUnit_Subtract_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Subtract(Argument0, Argument1);
	}
};

USTRUCT(meta=(DisplayName="Divide(Vector)", Category="Math|Vector"))
struct FRigUnit_Divide_VectorVector : public FRigUnit_BinaryVectorOp
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = FRigMathLibrary::Divide(Argument0, Argument1);
	}
};