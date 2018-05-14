// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnit_Control.h"
#include "Units/RigUnitContext.h"

void FRigUnit_Control::Execute(const FRigUnitContext& InContext)
{
	if (InContext.State == EControlRigState::Init)
	{
		Transform.FromFTransform(InitTransform);
	}

	Result = GetResultantTransform();
}

FTransform FRigUnit_Control::GetResultantTransform() const
{
	return GetFilteredTransform().ToFTransform() * Base;
}

FMatrix FRigUnit_Control::GetResultantMatrix() const
{
	const FEulerTransform FilteredTransform = GetFilteredTransform();
	return FScaleRotationTranslationMatrix(FilteredTransform.Scale, FilteredTransform.Rotation, FilteredTransform.Location) * Base.ToMatrixWithScale();
}

void FRigUnit_Control::SetResultantTransform(const FTransform& InResultantTransform)
{
	Transform.FromFTransform(InResultantTransform.GetRelativeTransform(Base));
}

void FRigUnit_Control::SetResultantMatrix(const FMatrix& InResultantMatrix)
{
	const FMatrix RelativeTransform = InResultantMatrix * Base.ToMatrixWithScale().Inverse();

	Transform.Location = RelativeTransform.GetOrigin();
	Transform.Rotation = RelativeTransform.Rotator();
	Transform.Scale = RelativeTransform.GetScaleVector();
}

FEulerTransform FRigUnit_Control::GetFilteredTransform() const
{
	FEulerTransform FilteredTransform = Transform;
	Filter.FilterTransform(FilteredTransform);
	return FilteredTransform;
}