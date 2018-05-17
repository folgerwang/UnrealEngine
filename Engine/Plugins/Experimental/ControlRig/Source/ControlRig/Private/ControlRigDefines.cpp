// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigDefines.h"
#include "PropertyPathHelpers.h"

bool FControlRigOperator::InitializeParam(UObject* OuterObject, FRigExecutor& OutExecutor)
{
	OutExecutor.Reset();
	// @todo: this code will be redone when Tom's thing gets checked in
	if (OpCode == EControlRigOpCode::Copy)
	{
		OutExecutor.OpCode = OpCode;
		OutExecutor.Property1 = FCachedPropertyPath(PropertyPath1);
		OutExecutor.Property1.Resolve(OuterObject);
		OutExecutor.Property2 = FCachedPropertyPath(PropertyPath2);
		OutExecutor.Property2.Resolve(OuterObject);
		return (OutExecutor.Property1.GetCachedAddress() != nullptr && OutExecutor.Property2.GetCachedAddress() != nullptr);
	}
	else if (OpCode == EControlRigOpCode::Exec)
	{
		OutExecutor.OpCode = OpCode;
		OutExecutor.Property1 = FCachedPropertyPath(PropertyPath1);
		OutExecutor.Property1.Resolve(OuterObject);
		return (OutExecutor.Property1.GetCachedAddress() != nullptr);
	}
	else if (OpCode == EControlRigOpCode::Done)
	{
		return true;
	}

	return false;
}

