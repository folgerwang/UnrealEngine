// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Hierarchy.h"
#include "Constraint.h"
#include "ControlRigDefines.h"
#include "RigUnit_GetJointTransform.generated.h"


UENUM()
enum class ETransformGetterType : uint8
{
	Initial,
	Current,
	Initial_Local,
	Current_Local,
	Max UMETA(Hidden), 
};

USTRUCT(meta=(DisplayName="Get Joint Transform", Category="Transforms"))
struct FRigUnit_GetJointTransform : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetJointTransform()
		: Type(ETransformGetterType::Initial)
	{}

	virtual void Execute(const FRigUnitContext& InContext) override;

	UPROPERTY(meta = (Input))
	FRigHierarchyRef HierarchyRef;

	UPROPERTY(meta = (Input))
	FName Joint;

	UPROPERTY(meta = (Input))
	ETransformGetterType Type;

	// possibly space, relative transform so on can be input
	UPROPERTY(meta=(Output))
	FTransform Output;
};
