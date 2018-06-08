// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "EulerTransform.h"
#include "Hierarchy.h"
#include "Units/RigUnit_Control.h"
#include "StructReference.h"
#include "RigUnit_Example.generated.h"

struct FRigUnit_Example;

/** A reference to a control unit */
USTRUCT()
struct FRigUnitReference_Example : public FStructReference
{
	GENERATED_BODY()
	
	IMPLEMENT_STRUCT_REFERENCE(FRigUnit_Example);
};

USTRUCT(meta=(DisplayName="Example Control Rig Unit", Category="Transforms"))
struct FRigUnit_Example : public FRigUnit
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override;

	// @TODO: Need a property reference here

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	FString TestInputString;

	UPROPERTY(meta=(Input, Output), VisibleAnywhere, Category=Default)
	FEulerTransform TestInOutTransform;

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	FVector TestInputVector;

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	int32 TestInputInteger;

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	UObject* TestInputObject;

	UPROPERTY(meta=(Output), VisibleAnywhere, Category=Default)
	FVector TestOutputVector;

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	float TestInputFloat;

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	TArray<float> TestInputFloatArray;

	UPROPERTY(meta=(Input), VisibleAnywhere, Category=Default)
	TArray<FTransform> TestInputTransformArray;

	UPROPERTY(meta = (Input, Output), VisibleAnywhere, Category=Default)
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(meta=(Output), VisibleAnywhere, Category=Default)
	float TestOutputFloat;

	UPROPERTY(meta=(Input))
	FRigUnitReference_Example TestUnitReferenceInput;

	UPROPERTY(meta=(Output))
	FRigUnitReference_Example TestUnitReferenceOutput;
};
