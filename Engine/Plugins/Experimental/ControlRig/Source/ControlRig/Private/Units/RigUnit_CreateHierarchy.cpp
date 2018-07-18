// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_CreateHierarchy.h"
#include "Units/RigUnitContext.h"

void FRigUnit_CreateHierarchy::Execute(const FRigUnitContext& InContext)
{
	if (InContext.State == EControlRigState::Init)
	{
		// create hierarchy
		if (!NewHierarchy.CreateHierarchy(Root, SourceHierarchy))
		{
			// if failed, print
		}
	}
}
