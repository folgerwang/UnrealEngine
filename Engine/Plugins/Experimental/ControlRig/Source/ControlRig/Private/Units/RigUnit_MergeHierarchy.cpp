// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_MergeHierarchy.h"
#include "Units/RigUnitContext.h"

void FRigUnit_MergeHierarchy::Execute(const FRigUnitContext& InContext)
{
	// merge input hierarchy to base
	TargetHierarchy.MergeHierarchy(SourceHierarchy);
}
