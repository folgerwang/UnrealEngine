// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "Units/RigUnitEditor_TwoBoneIKFK.h"
#include "Units/RigUnit_TwoBoneIKFK.h"

/////////////////////////////////////////////////////
// URigUnitEditor_TwoBoneIKFK

URigUnitEditor_TwoBoneIKFK::URigUnitEditor_TwoBoneIKFK(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URigUnitEditor_TwoBoneIKFK::MatchToIK(FRigUnit_TwoBoneIKFK* RigUnit_IKFK) const
{
	RigUnit_IKFK->StartJointFKTransform = RigUnit_IKFK->StartJointIKTransform;
	RigUnit_IKFK->MidJointFKTransform = RigUnit_IKFK->MidJointIKTransform;
	RigUnit_IKFK->EndJointFKTransform = RigUnit_IKFK->EndJointIKTransform;
}

void URigUnitEditor_TwoBoneIKFK::MatchToFK(FRigUnit_TwoBoneIKFK* RigUnit_IKFK) const
{
	RigUnit_IKFK->EndEffector = RigUnit_IKFK->EndJointFKTransform;

	FVector MidPoint = (RigUnit_IKFK->StartJointIKTransform.GetLocation() + RigUnit_IKFK->EndJointIKTransform.GetLocation()) * 0.5f;
	FVector ToMidJoint = RigUnit_IKFK->MidJointIKTransform.GetLocation() - MidPoint;
	RigUnit_IKFK->PoleTarget = MidPoint + (ToMidJoint) * 3;
}

void URigUnitEditor_TwoBoneIKFK::Snap() 
{
	if (HasValidReference())
	{
		FRigUnit_TwoBoneIKFK* RigUnit_IKFK = static_cast<FRigUnit_TwoBoneIKFK*>(SourceRigUnit);
		// this is called outside of execution loop
		// so check PreviousFKIKBlend and FKIKBlend wouldn't work
		// if current FK is active, copy FK to IK
		// if current IK is active, copy IK to FK
		if (RigUnit_IKFK->IKBlend == 0.f)
		{
			// turned off IK, just copy that transform to FK
			// I need a way to override the value of control units
			// it is actually very specific to control units, because otherwise, we won't be able to override it
			// so in that case, how do we get full name?
			// SetControlValue("StartJointFKTransform");
			// we want this operation to be done in editor time
			// but available by default
			MatchToFK(RigUnit_IKFK);
			RigUnit_IKFK->IKBlend = 1.f;
			UpdateSourceProperties(TEXT("EndEffector"));
			UpdateSourceProperties(TEXT("PoleTarget"));
			UpdateSourceProperties(TEXT("IKBlend"));
		}
		else if (RigUnit_IKFK->IKBlend == 1.f)
		{
			MatchToIK(RigUnit_IKFK);
			RigUnit_IKFK->IKBlend = 0.f;
			UpdateSourceProperties(TEXT("StartJointFKTransform"));
			UpdateSourceProperties(TEXT("MidJointFKTransform"));
			UpdateSourceProperties(TEXT("EndJointFKTransform"));
			UpdateSourceProperties(TEXT("IKBlend"));
		}
	}
}

FString URigUnitEditor_TwoBoneIKFK::GetDisplayName() const
{
	const FRigUnit_TwoBoneIKFK* RigUnit_IKFK = static_cast<const FRigUnit_TwoBoneIKFK*>(SourceRigUnit);
	if (RigUnit_IKFK)
	{
		TArray<FStringFormatArg> Arguments;
		Arguments.Add(RigUnit_IKFK->StartJoint.ToString());
		Arguments.Add(RigUnit_IKFK->EndJoint.ToString());

		return FString::Format(TEXT("IKFK {0}-{1}"), Arguments);
	}

	return FString(TEXT("Invalid IKFK"));
}

