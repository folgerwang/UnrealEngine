// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_BlendTransform.generated.h"

USTRUCT()
struct FBlendTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "FConstraintTarget")
	FTransform	Transform;

	UPROPERTY(EditAnywhere, Category = "FConstraintTarget")
	float Weight;

	FBlendTarget()
		: Weight (1.f)
	{}
};

USTRUCT(meta = (DisplayName = "Blend(Transform)", Category = "Blend"))
struct FRigUnit_BlendTransform : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FTransform Source;

	UPROPERTY(meta = (Input))
	TArray<FBlendTarget> Targets;

	UPROPERTY(meta = (Output))
	FTransform Result;

	virtual void Execute(const FRigUnitContext& InContext) override;
};