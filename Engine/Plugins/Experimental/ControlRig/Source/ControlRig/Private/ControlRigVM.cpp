// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigVM.h"
#include "Units/RigUnit.h"
#include "ControlRigDefines.h"

namespace ControlRigVM
{
	void Execute(UObject* OuterObject, const FRigUnitContext& Context, TArray<FRigExecutor>& InExecution, const ERigExecutionType ExecutionType)
	{
		const int32 TotalExec = InExecution.Num();
		for (int32 Index = 0; Index < InExecution.Num(); ++Index)
		{
			FRigExecutor Executor = InExecution[Index];
			if (!ExecOp(OuterObject, Context, ExecutionType, Executor))
			{
				// @todo: print warning?
				break;
			}
		}
	}

	bool ExecOp(UObject* OuterObject, const FRigUnitContext& Context, const ERigExecutionType ExecutionType, FRigExecutor& Executor)
	{
		check(OuterObject);
		switch (Executor.OpCode)
		{
		case EControlRigOpCode::Copy:
			PropertyPathHelpers::CopyPropertyValueFast(OuterObject, Executor.Property2, Executor.Property1);
			return true;
		case EControlRigOpCode::Exec:
		{
			FRigUnit* RigUnit = static_cast<FRigUnit*>(Executor.Property1.GetCachedAddress());
			bool bShouldExecute = (RigUnit->ExecutionType != EUnitExecutionType::Disable) &&
				// if rig unit is set to always OR execution type is editing time
				((RigUnit->ExecutionType == EUnitExecutionType::Always) || (ExecutionType == ERigExecutionType::Editing));
			if (bShouldExecute)
			{
				RigUnit->Execute(Context);
			}
		}
		return true;
		case EControlRigOpCode::Done: // @do I need it?
			return false;
		}

		// invalid op code
		return false;
	}
}
