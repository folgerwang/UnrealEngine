// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "RigUnit_CreateHierarchy.generated.h"

USTRUCT(meta=(DisplayName="Create Hierarchy", Category="Hierarchy"))
struct FRigUnit_CreateHierarchy : public FRigUnit
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override;

	// hierarchy reference - currently only merge to base hierarchy and outputs
	UPROPERTY(meta = (Input, Output))
	FRigHierarchyRef NewHierarchy;

	UPROPERTY(meta = (Input))
	FRigHierarchyRef SourceHierarchy;

	UPROPERTY(meta = (Input))
	FName Root;
};
