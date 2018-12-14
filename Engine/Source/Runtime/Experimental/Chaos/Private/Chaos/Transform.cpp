// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/Transform.h"

using namespace Chaos;

PMatrix<float, 4, 4> operator*(const TRigidTransform<float, 3>& Transform, const PMatrix<float, 4, 4>& Matrix)
{
	return Transform.ToMatrixNoScale() * static_cast<const FMatrix&>(Matrix);
}

PMatrix<float, 4, 4> operator*(const PMatrix<float, 4, 4>& Matrix, const TRigidTransform<float, 3>& Transform)
{
	return static_cast<const FMatrix&>(Matrix) * Transform.ToMatrixNoScale();
}