// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_GetJointTransform.h"
#include "Units/RigUnitContext.h"
#include "HelperUtil.h"

void FRigUnit_GetJointTransform::Execute(const FRigUnitContext& InContext)
{
	FRigHierarchy* Hierarchy = HierarchyRef.Get();
	if (Hierarchy)
	{
		int32 Index = Hierarchy->GetIndex(Joint);
		if (Index != INDEX_NONE)
		{
			FTransform GlobalTransform;
			switch (Type)
			{
			case ETransformGetterType::Initial_Local:
				{
					FTransform CurrentTransform = Hierarchy->GetInitialTransform(Index);
					const int32 ParentIndex = Hierarchy->GetParentIndex(Index);
					FTransform ParentTransform = (ParentIndex != INDEX_NONE)? Hierarchy->GetInitialTransform(ParentIndex) : FTransform::Identity;
					Output = CurrentTransform.GetRelativeTransform(ParentTransform);
					break;
				}
			case ETransformGetterType::Current_Local:
				{
					Output = Hierarchy->GetLocalTransform(Index);
					break;
				}
			case ETransformGetterType::Current:
				{
					Output = Hierarchy->GetGlobalTransform(Index);
					break;
				}
			case ETransformGetterType::Initial:
			default:
				{
					Output = Hierarchy->GetInitialTransform(Index);
					break;
				}
			}
		}
	}
	else
	{
		if (InContext.State == EControlRigState::Init)
		{
			UnitLogHelpers::PrintMissingHierarchy(RigUnitName);
		}
	}
}
