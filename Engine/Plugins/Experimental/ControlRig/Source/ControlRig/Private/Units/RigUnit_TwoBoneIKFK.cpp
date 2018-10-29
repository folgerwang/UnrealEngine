// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnit_TwoBoneIKFK.h"
#include "Units/RigUnitContext.h"
#include "HelperUtil.h"
#include "TwoBoneIK.h"

void FRigUnit_TwoBoneIKFK::Execute(const FRigUnitContext& InContext)
{
	if (InContext.State == EControlRigState::Init)
	{
		const FRigHierarchy* Hierarchy = HierarchyRef.Get();
		if (Hierarchy)
		{
			// reset
			StartJointIndex = MidJointIndex = EndJointIndex = INDEX_NONE;
			UpperLimbLength = LowerLimbLength = 0.f;

			// verify the chain
			int32 StartIndex = Hierarchy->GetIndex(StartJoint);
			int32 EndIndex = Hierarchy->GetIndex(EndJoint);
			if (StartIndex != INDEX_NONE && EndIndex != INDEX_NONE)
			{
				// ensure the chain
				int32 EndParentIndex = Hierarchy->GetParentIndex(EndIndex);
				if (EndParentIndex != INDEX_NONE)
				{
					int32 MidParentIndex = Hierarchy->GetParentIndex(EndParentIndex);
					if (MidParentIndex == StartIndex)
					{
						StartJointIndex = StartIndex;
						MidJointIndex = EndParentIndex;
						EndJointIndex = EndIndex;

						// set length for upper/lower length
						FTransform StartTransform = Hierarchy->GetInitialTransform(StartJointIndex);
						FTransform MidTransform = Hierarchy->GetInitialTransform(MidJointIndex);
						FTransform EndTransform = Hierarchy->GetInitialTransform(EndJointIndex);

						FVector UpperLimb = StartTransform.GetLocation() - MidTransform.GetLocation();
						FVector LowerLimb = MidTransform.GetLocation() - EndTransform.GetLocation();

						UpperLimbLength = UpperLimb.Size();
						LowerLimbLength = LowerLimb.Size();
						StartJointIKTransform = StartJointFKTransform = StartTransform;
						MidJointIKTransform = MidJointFKTransform = MidTransform;
						EndJointIKTransform = EndJointFKTransform = EndTransform;
					}
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
		if (StartJointIndex != INDEX_NONE && MidJointIndex != INDEX_NONE && EndJointIndex != INDEX_NONE)
		{
			FTransform StartJointTransform;
			FTransform MidJointTransform;
			FTransform EndJointTransform;

			// FK only
			if (FMath::IsNearlyZero(IKBlend))
			{
				StartJointTransform = StartJointFKTransform;
				MidJointTransform = MidJointFKTransform;
				EndJointTransform = EndJointFKTransform;
			}
			// IK only
			else if (FMath::IsNearlyEqual(IKBlend, 1.f))
			{
				// update transform before going through IK
				const FRigHierarchy* Hierarchy = HierarchyRef.Get();
				check(Hierarchy);

				StartJointIKTransform = Hierarchy->GetGlobalTransform(StartJointIndex);
				MidJointIKTransform = Hierarchy->GetGlobalTransform(MidJointIndex);
				EndJointIKTransform = Hierarchy->GetGlobalTransform(EndJointIndex);

				SolveIK();

				StartJointTransform = StartJointIKTransform;
				MidJointTransform = MidJointIKTransform;
				EndJointTransform = EndJointIKTransform;
			}
			else
			{
				// update transform before going through IK
				const FRigHierarchy* Hierarchy = HierarchyRef.Get();
				check(Hierarchy);

				StartJointIKTransform = Hierarchy->GetGlobalTransform(StartJointIndex);
				MidJointIKTransform = Hierarchy->GetGlobalTransform(MidJointIndex);
				EndJointIKTransform = Hierarchy->GetGlobalTransform(EndJointIndex);

				SolveIK();

				StartJointTransform.Blend(StartJointFKTransform, StartJointIKTransform, IKBlend);
				MidJointTransform.Blend(MidJointFKTransform, MidJointIKTransform, IKBlend);
				EndJointTransform.Blend(MidJointFKTransform, EndJointIKTransform, IKBlend);
			}

			FRigHierarchy* Hierarchy = HierarchyRef.Get();
			check(Hierarchy);
			Hierarchy->SetGlobalTransform(StartJointIndex, StartJointTransform);
			Hierarchy->SetGlobalTransform(MidJointIndex, MidJointTransform);
			Hierarchy->SetGlobalTransform(EndJointIndex, EndJointTransform);

			PreviousFKIKBlend = IKBlend;
		}
	}
}

void FRigUnit_TwoBoneIKFK::SolveIK()
{
	if (bUsePoleTarget)
	{
		AnimationCore::SolveTwoBoneIK(StartJointIKTransform, MidJointIKTransform, EndJointIKTransform, PoleTarget, EndEffector.GetLocation(), UpperLimbLength, LowerLimbLength, false, 1.0f, 1.05f);
	}
	else
	{
		// default IK
		// @todo: we don't have that yet, so default to using pole vector
		ensure(false);
		AnimationCore::SolveTwoBoneIK(StartJointIKTransform, MidJointIKTransform, EndJointIKTransform, PoleTarget, EndEffector.GetLocation(), UpperLimbLength, LowerLimbLength, false, 1.0f, 1.05f);
	}

	// set end effector rotation to current rotation
	EndJointIKTransform.SetRotation(EndEffector.GetRotation());
}

