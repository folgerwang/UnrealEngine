// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
			case ETransformGetterType::Current:
				{
					Output = Hierarchy->GetGlobalTransform(Index).GetRelativeTransform(GetBaseTransform(Index, Hierarchy, false));
					break;
				}
			case ETransformGetterType::Initial:
			default:
				{
					Output = Hierarchy->GetInitialTransform(Index).GetRelativeTransform(GetBaseTransform(Index, Hierarchy, true));
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

FTransform FRigUnit_GetJointTransform::GetBaseTransform(int32 JointIndex, const FRigHierarchy* CurrentHierarchy, bool bUseInitial) const
{
	if (bUseInitial)
	{
		return UtilityHelpers::GetBaseTransformByMode(TransformSpace, [CurrentHierarchy](const FName& JointName) { return CurrentHierarchy->GetInitialTransform(JointName); },
			CurrentHierarchy->Joints[JointIndex].ParentName, BaseJoint, BaseTransform);
	}
	else
	{
		return UtilityHelpers::GetBaseTransformByMode(TransformSpace, [CurrentHierarchy](const FName& JointName) { return CurrentHierarchy->GetGlobalTransform(JointName); },
			CurrentHierarchy->Joints[JointIndex].ParentName, BaseJoint, BaseTransform);
	}

}