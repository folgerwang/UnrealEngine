// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_ApplyFK.h"
#include "Units/RigUnitContext.h"
#include "HelperUtil.h"

void FRigUnit_ApplyFK::Execute(const FRigUnitContext& InContext)
{
	if (InContext.State == EControlRigState::Update)
	{
		FRigHierarchy* Hierarchy = HierarchyRef.Get();
		if (Hierarchy)
		{
			int32 Index = Hierarchy->GetIndex(Joint);
			if (Index != INDEX_NONE)
			{
				// first filter input transform
				FTransform InputTransform = Transform;
				Filter.FilterTransform(InputTransform);

				// now get override or additive
				// whether I'd like to apply whole thing or not
				if (ApplyTransformMode == EApplyTransformMode::Override)
				{
					// get base transform
					FTransform InputBaseTransform = GetBaseTransform(Index, Hierarchy);
					FTransform ApplyTransform = InputTransform * InputBaseTransform;
					Hierarchy->SetGlobalTransform(Index, ApplyTransform);
				}
				else
				{
					// if additive, we get current transform and calculate base transform and apply in their local space
					FTransform CurrentTransform = Hierarchy->GetGlobalTransform(Index);
					FTransform InputBaseTransform = GetBaseTransform(Index, Hierarchy);
					FTransform LocalTransform = InputTransform * CurrentTransform.GetRelativeTransform(InputBaseTransform);
					// apply additive
					Hierarchy->SetGlobalTransform(Index, LocalTransform * InputBaseTransform);
				}
			}
		}
	}
}

FTransform FRigUnit_ApplyFK::GetBaseTransform(int32 JointIndex, const FRigHierarchy* CurrentHierarchy) const
{
	return UtilityHelpers::GetBaseTransformByMode(ApplyTransformSpace, [CurrentHierarchy](const FName& JointName) { return CurrentHierarchy->GetGlobalTransform(JointName); },
		CurrentHierarchy->Joints[JointIndex].ParentName, BaseJoint, BaseTransform);
}