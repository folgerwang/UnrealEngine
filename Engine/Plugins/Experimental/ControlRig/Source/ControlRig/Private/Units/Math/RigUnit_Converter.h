// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Units/RigUnit.h"
#include "EulerTransform.h"
#include "RigUnit_Converter.generated.h"

USTRUCT(meta = (DisplayName = "ConvertToEulerTransform", Category = "Math|Convert"))
struct FRigUnit_ConvertTransform : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FTransform Input;

	UPROPERTY(meta=(Output))
	FEulerTransform Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result.FromFTransform(Input);
	}
};

USTRUCT(meta = (DisplayName = "ConvertToTransform", Category = "Math|Convert"))
struct FRigUnit_ConvertEulerTransform : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FEulerTransform Input;

	UPROPERTY(meta=(Output))
	FTransform Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.ToFTransform();
	}
};

USTRUCT(meta = (DisplayName = "ConvertToQuaternion", Category = "Math|Convert"))
struct FRigUnit_ConvertVectorRotation : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FRotator Input;

	UPROPERTY(meta = (Output))
	FQuat	Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.Quaternion();
	}
};

USTRUCT(meta = (DisplayName = "ConvertToRotation", Category = "Math|Convert"))
struct FRigUnit_ConvertQuaternion: public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FQuat	Input;

	UPROPERTY(meta = (Output))
	FRotator	Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.Rotator();
	}
};

USTRUCT(meta = (DisplayName = "ConvertVectorToRotation", Category = "Math|Convert"))
struct FRigUnit_ConvertVectorToRotation: public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Input;

	UPROPERTY(meta = (Output))
	FRotator	Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.Rotation();
	}
};

USTRUCT(meta = (DisplayName = "ConvertVectorToQuaternion", Category = "Math|Convert"))
struct FRigUnit_ConvertVectorToQuaternion: public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Input;

	UPROPERTY(meta = (Output))
	FQuat	Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.Rotation().Quaternion();
		Result.Normalize();
	}
};


USTRUCT(meta = (DisplayName = "ConvertRotationToVector", Category = "Math|Convert"))
struct FRigUnit_ConvertRotationToVector: public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FRotator Input;

	UPROPERTY(meta = (Output))
	FVector Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.RotateVector(FVector(1.f, 0.f, 0.f));
	}
};

USTRUCT(meta = (DisplayName = "ConvertQuaternionToVector", Category = "Math|Convert"))
struct FRigUnit_ConvertQuaternionToVector: public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat	Input;

	UPROPERTY(meta = (Output))
	FVector Result;

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		Result = Input.RotateVector(FVector(1.f, 0.f, 0.f));
	}
};

USTRUCT(meta = (DisplayName = "ToSwingAndTwist", Category = "Math|Transform"))
struct FRigUnit_ToSwingAndTwist : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta=(Input))
	FQuat	Input;

	UPROPERTY(meta = (Input))
	FVector TwistAxis;

	UPROPERTY(meta = (Output))
	FQuat	Swing;

	UPROPERTY(meta = (Output))
	FQuat	Twist;

	FRigUnit_ToSwingAndTwist()
		: TwistAxis(FVector(1.f, 0.f, 0.f))
	{
	}

	virtual void Execute(const FRigUnitContext& InContext) override
	{
		if (!TwistAxis.IsZero())
		{
			FVector NormalizedAxis = TwistAxis.GetSafeNormal();
			Input.ToSwingTwist(TwistAxis, Swing, Twist);
		}
	}
};