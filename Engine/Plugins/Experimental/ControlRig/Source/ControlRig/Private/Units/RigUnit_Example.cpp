// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_Example.h"
#include "Units/RigUnitContext.h"

void FRigUnit_Example::Execute(const FRigUnitContext& InContext)
{
	const FRigUnit_Example* ExampleRigUnit = TestUnitReferenceInput.Get();
	if(ExampleRigUnit != nullptr)
	{
		TestOutputVector = ExampleRigUnit->TestInputVector;
	}

	TestUnitReferenceOutput.Set(this);
}
