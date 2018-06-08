// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "RigUnit_MergeHierarchy.generated.h"

USTRUCT(meta=(DisplayName="Merge Hierarchy", Category="Hierarchy"))
struct FRigUnit_MergeHierarchy : public FRigUnit
{
	GENERATED_BODY()

	virtual void Execute(const FRigUnitContext& InContext) override;

	// hierarchy reference - currently only merge to base hierarchy and outputs
	UPROPERTY(meta = (Input, Output))
	FRigHierarchyRef TargetHierarchy;

	// hierarchy reference - currently only merge to base hierarchy and outputs
	// @todo : do we allow copying base to something else? Maybe... that doesn't work right now
	UPROPERTY(meta = (Input))
	FRigHierarchyRef SourceHierarchy;
};
