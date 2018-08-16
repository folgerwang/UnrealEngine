// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Matrix.h"
#include "Apeiron/Rotation.h"
#include "Apeiron/Vector.h"

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Math/Transform.h"
#else
//TODO(mlentine): If we use this class in engine we need to make it more efficient.
//TODO(mlentine): This should really be a class as there is a lot of functionality but static anlysis current forbids this.
struct FTransform
{
  public:
	FTransform() {}
	FTransform(const Apeiron::TRotation<float, 3>& Rotation, const Apeiron::TVector<float, 3>& Translation)
	    : MRotation(Rotation), MTranslation(Translation)
	{
	}
	FTransform(const FMatrix& Matrix)
	{
		MTranslation[0] = Matrix.M[0][3];
		MTranslation[1] = Matrix.M[1][3];
		MTranslation[2] = Matrix.M[2][3];

		float angle = sqrt(Matrix.M[0][0] * Matrix.M[0][0] + Matrix.M[1][0] * Matrix.M[1][0]);
		if (angle > 1e-6)
		{
			MRotation[0] = atan2(Matrix.M[2][1], Matrix.M[2][2]);
			MRotation[1] = atan2(-Matrix.M[2][0], angle);
			MRotation[2] = atan2(Matrix.M[1][0], Matrix.M[0][0]);
		}
		else
		{
			MRotation[0] = atan2(-Matrix.M[1][2], Matrix.M[1][1]);
			MRotation[1] = atan2(-Matrix.M[2][0], angle);
			MRotation[2] = 0;
		}
	}
	FTransform(const FTransform& Transform)
	    : MRotation(Transform.MRotation), MTranslation(Transform.MTranslation)
	{
	}
	Apeiron::TVector<float, 3> InverseTransformPosition(const Apeiron::TVector<float, 3>& Position)
	{
		Apeiron::TVector<float, 4> Position4(Position[0], Position[1], Position[2], 1);
		Apeiron::TVector<float, 4> NewPosition = ToInverseMatrix() * Position4;
		return Apeiron::TVector<float, 3>(NewPosition[0], NewPosition[1], NewPosition[2]);
	}
	Apeiron::TVector<float, 3> TransformVector(const Apeiron::TVector<float, 3>& Vector)
	{
		Apeiron::TVector<float, 4> Vector4(Vector[0], Vector[1], Vector[2], 0);
		Apeiron::TVector<float, 4> NewVector = ToMatrix() * Vector4;
		return Apeiron::TVector<float, 3>(NewVector[0], NewVector[1], NewVector[2]);
	}
	Apeiron::TVector<float, 3> InverseTransformVector(const Apeiron::TVector<float, 3>& Vector)
	{
		Apeiron::TVector<float, 4> Vector4(Vector[0], Vector[1], Vector[2], 0);
		Apeiron::TVector<float, 4> NewVector = ToInverseMatrix() * Vector4;
		return Apeiron::TVector<float, 3>(NewVector[0], NewVector[1], NewVector[2]);
	}
	Apeiron::PMatrix<float, 3, 3> ToRotationMatrix()
	{
		return Apeiron::PMatrix<float, 3, 3>(
		           cos(MRotation[0]), sin(MRotation[0]), 0,
		           -sin(MRotation[0]), cos(MRotation[0]), 0,
		           0, 0, 1) *
		    Apeiron::PMatrix<float, 3, 3>(
		        cos(MRotation[1]), 0, -sin(MRotation[1]),
		        0, 1, 0,
		        sin(MRotation[1]), 0, cos(MRotation[1])) *
		    Apeiron::PMatrix<float, 3, 3>(
		        1, 0, 0,
		        0, cos(MRotation[2]), sin(MRotation[2]),
		        0, -sin(MRotation[2]), cos(MRotation[2]));
	}
	Apeiron::PMatrix<float, 4, 4> ToMatrix()
	{
		auto RotationMatrix = ToRotationMatrix();
		return Apeiron::PMatrix<float, 4, 4>(
		    RotationMatrix.M[0][0], RotationMatrix.M[1][0], RotationMatrix.M[2][0], 0,
		    RotationMatrix.M[0][1], RotationMatrix.M[1][1], RotationMatrix.M[2][1], 0,
		    RotationMatrix.M[0][2], RotationMatrix.M[1][2], RotationMatrix.M[2][2], 0,
		    MTranslation[0], MTranslation[1], MTranslation[2], 1);
	}
	Apeiron::PMatrix<float, 4, 4> ToInverseMatrix()
	{
		auto RotationMatrix = ToRotationMatrix().GetTransposed();
		auto Vector = (RotationMatrix * MTranslation) * -1;
		return Apeiron::PMatrix<float, 4, 4>(
		    RotationMatrix.M[0][0], RotationMatrix.M[1][0], RotationMatrix.M[2][0], 0,
		    RotationMatrix.M[0][1], RotationMatrix.M[1][1], RotationMatrix.M[2][1], 0,
		    RotationMatrix.M[0][2], RotationMatrix.M[1][2], RotationMatrix.M[2][2], 0,
		    Vector[0], Vector[1], Vector[2], 1);
	}

  private:
	Apeiron::TRotation<float, 3> MRotation;
	Apeiron::TVector<float, 3> MTranslation;
};
#endif

namespace Apeiron
{
template<class T, int d>
class TRigidTransform
{
  private:
	TRigidTransform() {}
	~TRigidTransform() {}
};

template<>
class TRigidTransform<float, 3> : public FTransform
{
  public:
	TRigidTransform()
	    : FTransform() {}
	TRigidTransform(const TVector<float, 3>& Translation, const TRotation<float, 3>& Rotation)
	    : FTransform(Rotation, Translation) {}
	TRigidTransform(const FMatrix& Matrix)
	    : FTransform(Matrix) {}
	TRigidTransform(const FTransform& Transform)
	    : FTransform(Transform) {}
};
}
