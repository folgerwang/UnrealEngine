// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Vector.h"

#include <cmath>
#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Math/Quat.h"
#else
#include <array>

struct FQuat
{
  public:
	const float operator[](const int32 i) const
	{
		return angles[i];
	}
	float& operator[](const int32 i)
	{
		return angles[i];
	}
	std::array<float, 3> angles;
	static MakeFromEuler(const Vector<float, 3>& InAngles)
	{
		FQuat Quat;
		Quat.angles = InAngles;
		return Quat;
	}
};
#endif

namespace Apeiron
{
template<class T, int d>
class TRotation
{
  private:
	TRotation() {}
	~TRotation() {}
};

template<>
class TRotation<float, 3> : public FQuat
{
  public:
	TRotation()
	    : FQuat() {}
	TRotation(const FVector& Vec, const float Scalar)
	    : FQuat(Vec[0], Vec[1], Vec[2], Scalar) {}
	TRotation(const FQuat& Quat)
	    : FQuat(Quat) {}
	TRotation(const FMatrix& Matrix)
	    : FQuat(Matrix) {}

	static TRotation<float, 3> FromVector(const ::Apeiron::TVector<float, 3>& V)
	{
		TRotation<float, 3> Rot;
		float HalfSize = 0.5f * V.Size();
		float sinc = (FMath::Abs(HalfSize) > 1e-8) ? FMath::Sin(HalfSize) / HalfSize : 1;
		auto RotV = 0.5f * sinc * V;
		Rot.X = RotV.X;
		Rot.Y = RotV.Y;
		Rot.Z = RotV.Z;
		Rot.W = FMath::Cos(HalfSize);
		return Rot;
	}
};
}
