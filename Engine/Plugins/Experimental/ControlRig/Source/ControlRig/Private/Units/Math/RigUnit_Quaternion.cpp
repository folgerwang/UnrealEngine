// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RigUnit_Quaternion.h"

void FRigUnit_QuaternionToAngle::Execute(const FRigUnitContext& InContext)
{
	FQuat Swing, Twist;
	FVector SafeAxis = Axis.GetSafeNormal();

	FQuat Input = Argument;
	Input.Normalize();
	Input.ToSwingTwist(SafeAxis, Swing, Twist);

	FVector TwistAxis;
	float Radian;
	Twist.ToAxisAndAngle(TwistAxis, Radian);
	// Our range here is from [0, 360)
	Angle = FMath::Fmod(FMath::RadiansToDegrees(Radian), 360);
	if ((TwistAxis | SafeAxis) < 0)
	{
		Angle = 360 - Angle;
	}
}

