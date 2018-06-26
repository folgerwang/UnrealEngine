// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyPathHelpers.h"
#include "ControlRigDefines.generated.h"

UENUM()
enum class ETransformSpaceMode : uint8
{
	/** Apply in parent space */
	LocalSpace,

	/** Apply in rig space*/
	GlobalSpace,

	/** Apply in Base space */
	BaseSpace,

	/** Apply in base joint */
	BaseJoint,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

UENUM()
enum class EControlRigOpCode : uint8
{
	Done,
	Copy,
	Exec,
	Invalid,
};

struct FRigExecutor
{
	EControlRigOpCode OpCode;

	FCachedPropertyPath Property1;
	FCachedPropertyPath Property2;

	void Reset()
	{
		OpCode = EControlRigOpCode::Invalid;
	}
};

USTRUCT()
struct FControlRigOperator
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "FControlRigBlueprintOperator")
	EControlRigOpCode OpCode;
	
	/** Path to the property we are linking from */
	UPROPERTY(VisibleAnywhere, Category = "FControlRigBlueprintOperator")
	FString PropertyPath1;

	/** Path to the property we are linking to */
	UPROPERTY(VisibleAnywhere, Category = "FControlRigBlueprintOperator")
	FString PropertyPath2;

	FControlRigOperator(EControlRigOpCode Op = EControlRigOpCode::Invalid)
		: OpCode(Op)
	{
	}

	FControlRigOperator(EControlRigOpCode Op, const FString& InProperty1, const FString& InProperty2)
		: OpCode(Op)
		, PropertyPath1(InProperty1)
		, PropertyPath2(InProperty2)
	{

	}

	// fill up runtime operation code for the instance
	bool InitializeParam(UObject* OuterObject, FRigExecutor& OutExecutor);

	FString ToString()
	{
		TArray<FStringFormatArg> Arguments;
		Arguments.Add(FStringFormatArg((int32)OpCode));
		Arguments.Add(PropertyPath1);
		Arguments.Add(PropertyPath2);

		return FString::Format(TEXT("Opcode {0} : Property1 {1}, Property2 {2}"), Arguments);
	}
};

// thought of mixing this with execution on
// the problem is execution on is transient state, and 
// this execution type is something to be set per rig
UENUM()
enum class ERigExecutionType : uint8
{
	Runtime,
	Editing, // editing time
	Max UMETA(Hidden),
};
