// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_FABRIK.h"
#include "Units/RigUnitContext.h"

void FRigUnit_FABRIK::Execute(const FRigUnitContext& InContext)
{
	if (InContext.State == EControlRigState::Init)
	{
		const FRigHierarchy* Hierarchy = HierarchyRef.Get();
		if (Hierarchy)
		{
			FullLimbLength = 0.f;

			// verify the chain
			const int32 RootIndex = Hierarchy->GetIndex(StartJoint);
			if (RootIndex != INDEX_NONE)
			{
				int32 CurrentIndex = Hierarchy->GetIndex(EndJoint);
				while (CurrentIndex != INDEX_NONE)
				{
					// ensure the chain
					int32 ParentIndex = Hierarchy->GetParentIndex(CurrentIndex);
					if (ParentIndex != INDEX_NONE)
					{
						// set length for upper/lower length
						FTransform ParentTransform = Hierarchy->GetGlobalTransform(ParentIndex);
						FTransform CurrentTransform = Hierarchy->GetGlobalTransform(CurrentIndex);
						FVector Length = ParentTransform.GetLocation() - CurrentTransform.GetLocation();
						FullLimbLength += Length.Size();
					}

					if (ParentIndex == RootIndex)
					{
						break;
					}

					CurrentIndex = ParentIndex;
				}
			}
		}
		else
		{
			UnitLogHelpers::PrintMissingHierarchy(RigUnitName);
		}
	}
	else  if (InContext.State == EControlRigState::Update)
	{
		if (FullLimbLength > 0.f)
		{
			UnitLogHelpers::PrintUnimplemented(RigUnitName);
		}
	}
}

