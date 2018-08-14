// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Math/Matrix.h"
#else
#include <array>

struct FMatrix
{
  public:
	std::array<std::array<float, 4>, 4> M;
};
#endif

namespace Apeiron
{
template<class T, int m, int n>
class PMatrix
{
  private:
	PMatrix() {}
	~PMatrix() {}
};

template<>
class PMatrix<float, 3, 2>
{
public:
	float M[6];

	PMatrix(const TVector<float, 3>& C1, const TVector<float, 3>& C2)
	{
		M[0] = C1.X;
		M[1] = C1.Y;
		M[2] = C1.Z;
		M[3] = C2.X;
		M[4] = C2.Y;
		M[5] = C2.Z;
	}

	PMatrix(const float x00, const float x10, const float x20, const float x01, const float x11, const float x21)
	{
		M[0] = x00;
		M[1] = x10;
		M[2] = x20;
		M[3] = x01;
		M[4] = x11;
		M[5] = x21;
	}

	TVector<float, 3> operator*(const TVector<float, 2>& Other)
	{
		return TVector<float, 3>(
			M[0] * Other.X + M[3] * Other.Y,
			M[1] * Other.X + M[4] * Other.Y,
			M[2] * Other.X + M[5] * Other.Y);
	}
};

template<>
class PMatrix<float, 2, 2>
{
public:
	float M[4];

	PMatrix(const float x00, const float x10, const float x01, const float x11)
	{
		M[0] = x00;
		M[1] = x10;
		M[2] = x01;
		M[3] = x11;
	}

	PMatrix(const float x00, const float x10, const float x11)
	{
		M[0] = x00;
		M[1] = x10;
		M[2] = x10;
		M[3] = x11;
	}

	PMatrix<float, 2, 2> SubtractDiagonal(const float Scalar) const
	{
		return PMatrix<float, 2, 2>(
			M[0] - Scalar,
			M[1],
			M[2],
			M[3] - Scalar);
	}
};

template<>
class PMatrix<float, 4, 4> : public FMatrix
{
  public:
	PMatrix()
	    : FMatrix() {}
	PMatrix(const float x00, const float x10, const float x20, const float x30, const float x01, const float x11, const float x21, const float x31, const float x02, const float x12, const float x22, const float x32, const float x03, const float x13, const float x23, const float x33)
	    : FMatrix()
	{
		M[0][0] = x00;
		M[1][0] = x10;
		M[2][0] = x20;
		M[3][0] = x30;
		M[0][1] = x01;
		M[1][1] = x11;
		M[2][1] = x21;
		M[3][1] = x31;
		M[0][2] = x02;
		M[1][2] = x12;
		M[2][2] = x22;
		M[3][2] = x32;
		M[0][3] = x03;
		M[1][3] = x13;
		M[2][3] = x23;
		M[3][3] = x33;
	}
#if COMPILE_WITHOUT_UNREAL_SUPPORT
	Vector<float, 4> operator*(const Vector<float, 4>& Other)
	{
		return Vector<float, 4>(
		    M[0][0] * Other[0] + M[0][1] * Other[1] + M[0][2] * Other[2] + M[0][3] * Other[3],
		    M[1][0] * Other[0] + M[1][1] * Other[1] + M[1][2] * Other[2] + M[1][3] * Other[3],
		    M[2][0] * Other[0] + M[2][1] * Other[1] + M[2][2] * Other[2] + M[2][3] * Other[3],
		    M[3][0] * Other[0] + M[3][1] * Other[1] + M[3][2] * Other[2] + M[3][3] * Other[3]);
	}
#endif
};

// TODO(mlentine): Do not use 4x4 matrix for 3x3 implementation
template<>
class PMatrix<float, 3, 3> : public FMatrix
{
  public:
	PMatrix()
	    : FMatrix() {}
	PMatrix(FMatrix&& Other)
	    : FMatrix(MoveTemp(Other)) {}
	PMatrix(const FMatrix& Other)
	    : FMatrix(Other) {}
	PMatrix(const float x00, const float x10, const float x20, const float x11, const float x21, const float x22)
	    : FMatrix()
	{
		M[0][0] = x00;
		M[1][0] = x10;
		M[2][0] = x20;
		M[0][1] = x10;
		M[1][1] = x11;
		M[2][1] = x21;
		M[0][2] = x20;
		M[1][2] = x21;
		M[2][2] = x22;
		// Fill in the remainder with reasonable values.
		M[3][0] = 0;
		M[3][1] = 0;
		M[3][2] = 0;
		M[3][3] = 1;
		M[0][3] = 0;
		M[1][3] = 0;
		M[2][3] = 0;
	}
	PMatrix(const float x00, const float x10, const float x20, const float x01, const float x11, const float x21, const float x02, const float x12, const float x22)
	    : FMatrix()
	{
		M[0][0] = x00;
		M[1][0] = x10;
		M[2][0] = x20;
		M[0][1] = x01;
		M[1][1] = x11;
		M[2][1] = x21;
		M[0][2] = x02;
		M[1][2] = x12;
		M[2][2] = x22;
		// Fill in the remainder with reasonable values.
		M[3][0] = 0;
		M[3][1] = 0;
		M[3][2] = 0;
		M[3][3] = 1;
		M[0][3] = 0;
		M[1][3] = 0;
		M[2][3] = 0;
	}
	PMatrix(const float x)
	    : FMatrix()
	{
		M[0][0] = x;
		M[1][0] = x;
		M[2][0] = x;
		M[0][1] = x;
		M[1][1] = x;
		M[2][1] = x;
		M[0][2] = x;
		M[1][2] = x;
		M[2][2] = x;
		// Fill in the remainder with reasonable values.
		M[3][0] = 0;
		M[3][1] = 0;
		M[3][2] = 0;
		M[3][3] = 1;
		M[0][3] = 0;
		M[1][3] = 0;
		M[2][3] = 0;
	}
	PMatrix(const TVector<float, 3>& C1, const TVector<float, 3>& C2, const TVector<float, 3>& C3)
	{
		M[0][0] = C1.X;
		M[1][0] = C1.Y;
		M[2][0] = C1.Z;
		M[0][1] = C2.X;
		M[1][1] = C2.Y;
		M[2][1] = C2.Z;
		M[0][2] = C3.X;
		M[1][2] = C3.Y;
		M[2][2] = C3.Z;
		// Fill in the remainder with reasonable values.
		M[3][0] = 0;
		M[3][1] = 0;
		M[3][2] = 0;
		M[3][3] = 1;
		M[0][3] = 0;
		M[1][3] = 0;
		M[2][3] = 0;
	}
#if COMPILE_WITHOUT_UNREAL_SUPPORT
	PMatrix<float, 3, 3> GetTransposed()
	{
		return PMatrix<float, 3, 3>(M[0][0], M[0][1], M[0][2], M[1][0], M[1][1], M[1][2], M[2][0], M[2][1], M[2][2]);
	}
	float Determinant()
	{
		return M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
	}
	PMatrix<float, 3, 3>& operator+=(const PMatrix<float, 3, 3>& Other)
	{
		M[0][0] += Other.M[0][0];
		M[0][1] += Other.M[0][1];
		M[0][2] += Other.M[0][2];
		M[1][0] += Other.M[1][0];
		M[1][1] += Other.M[1][1];
		M[1][2] += Other.M[1][2];
		M[2][0] += Other.M[2][0];
		M[2][1] += Other.M[2][1];
		M[2][2] += Other.M[2][2];
		return *this;
	}
#endif
	// TDOD(mlentine): This should really be a vector multiply and sum for each entry using sse
	TVector<float, 3> operator*(const TVector<float, 3>& Other) const
	{
		return TVector<float, 3>(
		    M[0][0] * Other[0] + M[0][1] * Other[1] + M[0][2] * Other[2],
		    M[1][0] * Other[0] + M[1][1] * Other[1] + M[1][2] * Other[2],
		    M[2][0] * Other[0] + M[2][1] * Other[1] + M[2][2] * Other[2]);
	}
	PMatrix<float, 3, 3> operator-(const PMatrix<float, 3, 3>& Other) const
	{
		return PMatrix<float, 3, 3>(
		    M[0][0] - Other.M[0][0],
		    M[1][0] - Other.M[1][0],
		    M[2][0] - Other.M[2][0],
		    M[0][1] - Other.M[0][1],
		    M[1][1] - Other.M[1][1],
		    M[2][1] - Other.M[2][1],
		    M[0][2] - Other.M[0][2],
		    M[1][2] - Other.M[1][2],
		    M[2][2] - Other.M[2][2]);
	}
	PMatrix<float, 3, 3> operator*(const float& Other) const
	{
		return PMatrix<float, 3, 3>(
		    M[0][0] * Other,
		    M[1][0] * Other,
		    M[2][0] * Other,
		    M[0][1] * Other,
		    M[1][1] * Other,
		    M[2][1] * Other,
		    M[0][2] * Other,
		    M[1][2] * Other,
		    M[2][2] * Other);
	}
	PMatrix<float, 3, 2> operator*(const PMatrix<float, 3, 2>& Other) const
	{
		return PMatrix<float, 3, 2>(
			M[0][0] * Other.M[0] + M[0][1] * Other.M[1] + M[0][2] * Other.M[2],
			M[1][0] * Other.M[0] + M[1][1] * Other.M[1] + M[1][2] * Other.M[2],
			M[2][0] * Other.M[0] + M[2][1] * Other.M[1] + M[2][2] * Other.M[2],
			M[0][0] * Other.M[3] + M[0][1] * Other.M[4] + M[0][2] * Other.M[5],
			M[1][0] * Other.M[3] + M[1][1] * Other.M[4] + M[1][2] * Other.M[5],
			M[2][0] * Other.M[3] + M[2][1] * Other.M[4] + M[2][2] * Other.M[5]);
	}
	PMatrix<float, 3, 3> SubtractDiagonal(const float Scalar) const
	{
		return PMatrix<float, 3, 3>(
			M[0][0] - Scalar,
			M[1][0],
			M[2][0],
			M[0][1],
			M[1][1] - Scalar,
			M[2][1],
			M[0][2],
			M[1][2],
			M[2][2] - Scalar);
	}
	PMatrix<float, 3, 3> SymmetricCofactorMatrix() const
	{
		return PMatrix<float, 3, 3>(
			M[1][1] * M[2][2] - M[2][1] * M[2][1],
			M[2][1] * M[2][0] - M[1][0] * M[2][2],
			M[1][0] * M[2][1] - M[1][1] * M[2][0],
			M[0][0] * M[2][2] - M[2][0] * M[2][0],
			M[1][0] * M[2][0] - M[0][0] * M[2][1],
			M[0][0] * M[1][1] - M[1][0] * M[1][0]);
	}
	TVector<float, 3> LargestColumnNormalized() const
	{
		float m10 = M[1][0] * M[1][0];
		float m20 = M[2][0] * M[2][0];
		float m21 = M[2][1] * M[2][1];
		float c0 = M[0][0] * M[0][0] + m10 + m20;
		float c1 = m10 + M[1][1] * M[1][1] + m21;
		float c2 = m20 + m21 + M[2][2] * M[2][2];
		if(c0 > c1 && c0 > c2)
		{
			return TVector<float, 3>(M[0][0],M[1][0],M[2][0]) / sqrt(c0);
		}
		if (c1 > c2)
		{
			return TVector<float, 3>(M[1][0], M[1][1], M[2][1]) / sqrt(c1);
		}
		if (c2 > 0)
		{
			return TVector<float, 3>(M[2][0], M[2][1], M[2][2]) / sqrt(c2);
		}
		return TVector<float, 3>(1, 0, 0);
	}
#if COMPILE_WITHOUT_UNREAL_SUPPORT
	// TODO(mlentine): Document which one is row and which one is column
	PMatrix<float, 3, 3> operator*(const PMatrix<float, 3, 3>& Other)
	{
		return PMatrix<float, 3, 3>(
		    M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0],
		    M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0],
		    M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0],
		    M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1],
		    M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1],
		    M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1],
		    M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2],
		    M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2],
		    M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2]);
	}
	PMatrix<float, 3, 3> operator*(const float Scalar)
	{
		return PMatrix<float, 3, 3>(
		    M[0][0] * Scalar + M[0][1] * Scalar + M[0][2] * Scalar,
		    M[1][0] * Scalar + M[1][1] * Scalar + M[1][2] * Scalar,
		    M[2][0] * Scalar + M[2][1] * Scalar + M[2][2] * Scalar,
		    M[0][0] * Scalar + M[0][1] * Scalar + M[0][2] * Scalar,
		    M[1][0] * Scalar + M[1][1] * Scalar + M[1][2] * Scalar,
		    M[2][0] * Scalar + M[2][1] * Scalar + M[2][2] * Scalar,
		    M[0][0] * Scalar + M[0][1] * Scalar + M[0][2] * Scalar,
		    M[1][0] * Scalar + M[1][1] * Scalar + M[1][2] * Scalar,
		    M[2][0] * Scalar + M[2][1] * Scalar + M[2][2] * Scalar);
	}
#endif
};
}
