// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_AimConstraint.h"
#include "Units/RigUnitContext.h"
#include "HelperUtil.h"
#include "AnimationCoreLibrary.h"

void FRigUnit_AimConstraint::Execute(const FRigUnitContext& InContext)
{
	if (InContext.State == EControlRigState::Init)
	{
		ConstraintData.Reset();

		FRigHierarchy* Hierarchy = HierarchyRef.Get();
		if (Hierarchy)
		{
			int32 JointIndex = Hierarchy->GetIndex(Joint);
			if (JointIndex != INDEX_NONE)
			{
				const int32 TargetNum = AimTargets.Num();
				if (TargetNum > 0)
				{
					const FTransform SourceTransform = Hierarchy->GetGlobalTransform(JointIndex);
					for (int32 TargetIndex = 0; TargetIndex < TargetNum; ++TargetIndex)
					{
						const FAimTarget& Target = AimTargets[TargetIndex];

						int32 NewIndex = ConstraintData.AddDefaulted();
						check(NewIndex != INDEX_NONE);
						FConstraintData& NewData = ConstraintData[NewIndex];
						NewData.Constraint = FAimConstraintDescription();
						NewData.bMaintainOffset = false; // for now we don't support maintain offset for aim
						NewData.Weight = Target.Weight;
					}
				}
			}
		}
	}
	else if (InContext.State == EControlRigState::Update)
	{
		FRigHierarchy* Hierarchy = HierarchyRef.Get();
		if (Hierarchy)
		{
			int32 JointIndex = Hierarchy->GetIndex(Joint);
			if (JointIndex != INDEX_NONE)
			{
				const int32 TargetNum = AimTargets.Num();
				if (TargetNum > 0)
				{
					for (int32 ConstraintIndex= 0; ConstraintIndex< ConstraintData.Num(); ++ConstraintIndex)
					{
						FAimConstraintDescription* AimConstraintDesc = ConstraintData[ConstraintIndex].Constraint.GetTypedConstraint<FAimConstraintDescription>();
						AimConstraintDesc->LookAt_Axis = FAxis(AimVector);

						if (UpTargets.IsValidIndex(ConstraintIndex))
						{
							AimConstraintDesc->LookUp_Axis = FAxis(UpVector);
							AimConstraintDesc->bUseLookUp = UpVector.Size() > 0.f;
							AimConstraintDesc->LookUpTarget = UpTargets[ConstraintIndex].Transform.GetLocation();
						}

						ConstraintData[ConstraintIndex].CurrentTransform = AimTargets[ConstraintIndex].Transform;
						ConstraintData[ConstraintIndex].Weight = AimTargets[ConstraintIndex].Weight;
					}

					FTransform BaseTransform = FTransform::Identity;
					int32 ParentIndex = Hierarchy->GetParentIndex(JointIndex);
					if (ParentIndex != INDEX_NONE)
					{
						BaseTransform = Hierarchy->GetGlobalTransform(ParentIndex);
					}

					FTransform SourceTransform = Hierarchy->GetGlobalTransform(JointIndex);

					// @todo: ignore maintain offset for now
					FTransform ConstrainedTransform = AnimationCore::SolveConstraints(SourceTransform, BaseTransform, ConstraintData);

					Hierarchy->SetGlobalTransform(JointIndex, ConstrainedTransform);
				}
			}
		}
	}
}

/*
void FRigUnit_AimConstraint::AddConstraintData(EAimConstraintType ConstraintType, const int32 TargetIndex, const FTransform& SourceTransform, const FTransform& InBaseTransform)
{
	const FConstraintTarget& Target = Targets[TargetIndex];

	int32 NewIndex = ConstraintData.AddDefaulted();
	check(NewIndex != INDEX_NONE);
	FConstraintData& NewData = ConstraintData[NewIndex];
	NewData.Constraint = FAimConstraintDescription(ConstraintType);
	NewData.bMaintainOffset = Target.bMaintainOffset;
	NewData.Weight = Target.Weight;

	if (Target.bMaintainOffset)
	{
		NewData.SaveInverseOffset(SourceTransform, Target.Transform, InBaseTransform);
	}

	ConstraintDataToTargets.Add(NewIndex, TargetIndex);
}*/