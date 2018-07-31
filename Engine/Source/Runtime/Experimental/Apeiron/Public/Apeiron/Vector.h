// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Math/Vector.h"
#include "Math/Vector4.h"
#endif

#include "Apeiron/Array.h"
#include "Apeiron/Defines.h"
#include "Apeiron/Pair.h"

#include <iostream>
#include <utility>

namespace Apeiron
{
template<class T, int d>
class TVector : public TArray<T>
{
  public:
	using TArray<T>::SetNum;

	TVector()
	    : TArray<T>()
	{
		SetNum(d);
	}
	TVector(const T element)
	    : TArray<T>()
	{
		SetNum(d);
		for (int32 i = 0; i < d; ++i)
			(*this)[i] = element;
	}
	TVector(const T s1, const T s2)
	{
		check(d == 2);
		SetNum(2);
		(*this)[0] = s1;
		(*this)[1] = s2;
	}
	TVector(const T s1, const T s2, const T s3)
	{
		check(d == 3);
		SetNum(3);
		(*this)[0] = s1;
		(*this)[1] = s2;
		(*this)[2] = s3;
	}
	TVector(const T s1, const T s2, const T s3, const T s4)
	{
		check(d == 4);
		SetNum(4);
		(*this)[0] = s1;
		(*this)[1] = s2;
		(*this)[2] = s3;
		(*this)[3] = s4;
	}
#if !COMPILE_WITHOUT_UNREAL_SUPPORT
	TVector(const FVector& Other)
	{
		check(d == 3);
		SetNum(3);
		(*this)[0] = static_cast<T>(Other.X);
		(*this)[1] = static_cast<T>(Other.Y);
		(*this)[2] = static_cast<T>(Other.Z);
	}
#endif
	template<class T2>
	TVector(const TVector<T2, d>& Other)
	{
		SetNum(d);
		for (int32 i = 0; i < d; ++i)
		{
			(*this)[i] = static_cast<T>(Other[i]);
		}
	}
	TVector(std::initializer_list<T> l)
	    : TArray<T>(l) {}
	TVector(std::istream& Stream)
	{
		for (int32 i = 0; i < d; ++i)
		{
			Stream.read(reinterpret_cast<char*>(&(*this)[i]), sizeof(T));
		}
	}
	~TVector() {}
	void Write(std::ostream& Stream) const
	{
		for (int32 i = 0; i < d; ++i)
		{
			Stream.write(reinterpret_cast<const char*>(&(*this)[i]), sizeof(T));
		}
	}
	TVector<T, d>& operator=(const TVector<T, d>& Other)
	{
		for (int32 i = 0; i < d; ++i)
		{
			(*this)[i] = Other[i];
		}
		return *this;
	}
	T Size() const
	{
		T SquaredSum = 0;
		for (int32 i = 0; i < d; ++i)
		{
			SquaredSum += ((*this)[i] * (*this)[i]);
		}
		return sqrt(SquaredSum);
	}
	T Product() const
	{
		T Result = 1;
		for (int32 i = 0; i < d; ++i)
		{
			Result *= (*this)[i];
		}
		return Result;
	}
	static TVector<T, d> AxisVector(const int32 Axis)
	{
		check(Axis >= 0 && Axis < d);
		TVector<T, d> Result(0);
		Result[Axis] = (T)1;
		return Result;
	}
	T SizeSquared() const
	{
		T Result = 0;
		for (int32 i = 0; i < d; ++i)
		{
			Result += ((*this)[i] * (*this)[i]);
		}
		return Result;
	}
	TVector<T, d> GetSafeNormal() const
	{
		T SizeSqr = SizeSquared();
		if (SizeSqr < (T)1e-4)
			return AxisVector(0);
		return (*this) / sqrt(SizeSqr);
	}
	TVector<T, d> operator-() const
	{
		TVector<T, d> Result;
		for (int32 i = 0; i < d; ++i)
		{
			Result[i] = -(*this)[i];
		}
		return Result;
	}
	TVector<T, d> operator*(const TVector<T, d>& Other) const
	{
		TVector<T, d> Result;
		for (int32 i = 0; i < d; ++i)
		{
			Result[i] = (*this)[i] * Other[i];
		}
		return Result;
	}
	TVector<T, d> operator/(const TVector<T, d>& Other) const
	{
		TVector<T, d> Result;
		for (int32 i = 0; i < d; ++i)
		{
			Result[i] = (*this)[i] / Other[i];
		}
		return Result;
	}
	TVector<T, d> operator+(const TVector<T, d>& Other) const
	{
		TVector<T, d> Result;
		for (int32 i = 0; i < d; ++i)
		{
			Result[i] = (*this)[i] + Other[i];
		}
		return Result;
	}
	TVector<T, d> operator-(const TVector<T, d>& Other) const
	{
		TVector<T, d> Result;
		for (int32 i = 0; i < d; ++i)
		{
			Result[i] = (*this)[i] - Other[i];
		}
		return Result;
	}
	TVector<T, d>& operator+=(const TVector<T, d>& Other)
	{
		for (int32 i = 0; i < d; ++i)
		{
			(*this)[i] += Other[i];
		}
		return *this;
	}
	TVector<T, d>& operator-=(const TVector<T, d>& Other)
	{
		for (int32 i = 0; i < d; ++i)
		{
			(*this)[i] -= Other[i];
		}
		return *this;
	}
	TVector<T, d>& operator/=(const TVector<T, d>& Other)
	{
		for (int32 i = 0; i < d; ++i)
		{
			(*this)[i] /= Other[i];
		}
		return *this;
	}
	TVector<T, d> operator*(const T& S) const
	{
		TVector<T, d> Result;
		for (int32 i = 0; i < d; ++i)
		{
			Result[i] = (*this)[i] * S;
		}
		return Result;
	}
	TVector<T, d>& operator*=(const T& S)
	{
		for (int32 i = 0; i < d; ++i)
		{
			(*this)[i] *= S;
		}
		return *this;
	}
#if COMPILE_WITHOUT_UNREAL_SUPPORT
	static inline float DotProduct(const Vector<float, 3>& V1, const Vector<float, 3>& V2)
	{
		return V1[0] * V2[0] + V1[1] * V2[1] + V1[2] * V2[2];
	}
	static inline Vector<float, 3> CrossProduct(const Vector<float, 3>& V1, const Vector<float, 3>& V2)
	{
		Vector<float, 3> Result;
		Result[0] = V1[1] * V2[2] - V1[2] * V2[1];
		Result[1] = V1[2] * V2[0] - V1[0] * V2[2];
		Result[2] = V1[0] * V2[1] - V1[1] * V2[0];
		return Result;
	}
#endif
};
template<class T, int d>
inline TVector<T, d> operator*(const T S, const TVector<T, d>& V)
{
	TVector<T, d> Ret;
	for (int32 i = 0; i < d; ++i)
	{
		Ret[i] = S * V[i];
	}
	return Ret;
}
template<class T, int d>
inline TVector<T, d> operator/(const T S, const TVector<T, d>& V)
{
	TVector<T, d> Ret;
	for (int32 i = 0; i < d; ++i)
	{
		Ret = S / V[i];
	}
	return Ret;
}

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
template<>
class TVector<float, 4> : public FVector4
{
  public:
	using FVector4::W;
	using FVector4::X;
	using FVector4::Y;
	using FVector4::Z;

	TVector()
	    : FVector4() {}
	TVector(const float x)
	    : FVector4(x, x, x, x) {}
	TVector(const float x, const float y, const float z, const float w)
	    : FVector4(x, y, z, w) {}
	TVector(const FVector4& vec)
	    : FVector4(vec) {}
};

template<>
class TVector<float, 3> : public FVector
{
  public:
	using FVector::X;
	using FVector::Y;
	using FVector::Z;

	TVector()
	    : FVector() {}
	TVector(const float x)
	    : FVector(x, x, x) {}
	TVector(const float x, const float y, const float z)
	    : FVector(x, y, z) {}
	TVector(const FVector& vec)
	    : FVector(vec) {}
	TVector(const FVector4& vec)
	    : FVector(vec.X, vec.Y, vec.Z) {}
	TVector(std::istream& Stream)
	{
		Stream.read(reinterpret_cast<char*>(&X), sizeof(X));
		Stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
		Stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
	}
	~TVector() {}
	void Write(std::ostream& Stream) const
	{
		Stream.write(reinterpret_cast<const char*>(&X), sizeof(X));
		Stream.write(reinterpret_cast<const char*>(&Y), sizeof(Y));
		Stream.write(reinterpret_cast<const char*>(&Z), sizeof(Z));
	}
	static inline TVector<float, 3> CrossProduct(const TVector<float, 3>& V1, const TVector<float, 3>& V2) { return FVector::CrossProduct(V1, V2); }
	static inline float DotProduct(const TVector<float, 3>& V1, const TVector<float, 3>& V2) { return FVector::DotProduct(V1, V2); }
	bool operator<=(const TVector<float, 3>& V) const
	{
		return X <= V.X && Y <= V.Y && Z <= V.Z;
	}
	bool operator>=(const TVector<float, 3>& V) const
	{
		return X >= V.X && Y >= V.Y && Z >= V.Z;
	}
	TVector<float, 3> operator-() const
	{
		return TVector<float, 3>(-X, -Y, -Z);
	}
	TVector<float, 3> operator-(const float Other) const
	{
		return TVector<float, 3>(X - Other, Y - Other, Z - Other);
	}
	TVector<float, 3> operator*(const float Other) const
	{
		return TVector<float, 3>(X * Other, Y * Other, Z * Other);
	}
	TVector<float, 3> operator/(const float Other) const
	{
		return TVector<float, 3>(X / Other, Y / Other, Z / Other);
	}
	template<class T2>
	TVector<float, 3> operator-(const TVector<T2, 3>& Other) const
	{
		return TVector<float, 3>(X - Other[0], Y - Other[1], Z - Other[2]);
	}
	template<class T2>
	TVector<float, 3> operator*(const TVector<T2, 3>& Other) const
	{
		return TVector<float, 3>(X * Other[0], Y * Other[1], Z * Other[2]);
	}
	template<class T2>
	TVector<float, 3> operator/(const TVector<T2, 3>& Other) const
	{
		return TVector<float, 3>(X / Other[0], Y / Other[1], Z / Other[2]);
	}
	float Max() const
	{
		return (X > Y && X > Z) ? X : (Y > Z ? Y : Z);
	}
	float Min() const
	{
		return (X < Y && X < Z) ? X : (Y < Z ? Y : Z);
	}
	static TVector<float, 3> Max(const TVector<float, 3>& V1, const TVector<float, 3>& V2)
	{
		return TVector<float, 3>(V1.X > V2.X ? V1.X : V2.X, V1.Y > V2.Y ? V1.Y : V2.Y, V1.Z > V2.Z ? V1.Z : V2.Z);
	}
	static TVector<float, 3> AxisVector(const int32 Axis)
	{
		check(Axis >= 0 && Axis <= 2);
		return Axis == 0 ? TVector<float, 3>(1.f, 0.f, 0.f) : (Axis == 1 ? TVector<float, 3>(0.f, 1.f, 0.f) : TVector<float, 3>(0.f, 0.f, 1.f));
	}
	static Pair<float, int32> MaxAndAxis(const TVector<float, 3>& V1, const TVector<float, 3>& V2)
	{
		const TVector<float, 3> max = Max(V1, V2);
		if (max.X > max.Y)
		{
			if (max.X > max.Z)
				return MakePair(max.X, 0);
			else
				return MakePair(max.Z, 2);
		}
		else
		{
			if (max.Y > max.Z)
				return MakePair(max.Y, 1);
			else
				return MakePair(max.Z, 2);
		}
	}
	TVector<float, 3> GetOrthogonalVector()
	{
		TVector<float, 3> AbsVector = (FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
		if (AbsVector.X < AbsVector.Y && AbsVector.X < AbsVector.Z)
		{
			return TVector<float, 3>(0, AbsVector.Z, -AbsVector.Y);
		}
		if (AbsVector.X < AbsVector.Y)
		{
			return TVector<float, 3>(AbsVector.Y, -AbsVector.Z, 0);
		}
		if (AbsVector.Y < AbsVector.Z)
		{
			return TVector<float, 3>(-AbsVector.Z, 0, AbsVector.X);
		}
		return TVector<float, 3>(AbsVector.Y, -AbsVector.X, 0);
	}
};
template<>
inline TVector<float, 3> operator/(const float S, const TVector<float, 3>& V)
{
	return TVector<float, 3>(S / V.X, S / V.Y, S / V.Z);
}

template<>
class TVector<float, 2> : public FVector2D
{
public:
	TVector()
		: FVector2D() {}
	TVector(const float x, const float y)
	    : FVector2D(x, y) {}
	TVector(const FVector2D& vec)
	    : FVector2D(vec) {}
};
#endif
}

template<class T>
static uint32 GetTypeHash(const Apeiron::TVector<T, 2>& V)
{
	uint32 Seed = GetTypeHash(V[0]);
	Seed ^= GetTypeHash(V[1]) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	return Seed;
}