// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Pair.h"
#include "Apeiron/Vector.h"

#include <functional>

namespace Apeiron
{
template<class T, int d>
class TBox;
template<class T, int d>
class TSphere;
template<class T, int d>
class TPlane;

enum class ImplicitObjectType
{
	Sphere,
	Box,
	Plane,
	Transformed,
	Unknown
};

template<class T, int d>
class APEIRON_API TImplicitObject
{
  public:
	TImplicitObject(TFunction<T(const TVector<T, d>&, TVector<T, d>&)> InPhiWithNormal, TFunction<const TBox<T, d>&()> InBoundingBox, TFunction<Pair<TVector<T, d>, bool>(const TVector<T, d>&, const TVector<T, d>&, const T)> InFindClosestIntersection, TFunction<TVector<T, d>(const TVector<T, d>&)> InSupport, ImplicitObjectType Type = ImplicitObjectType::Unknown, void* DerivedThis = nullptr);
	TImplicitObject(const TImplicitObject<T, d>&) = delete;
	TImplicitObject(TImplicitObject<T, d>&&) = delete;
	~TImplicitObject();

	template<class T_DERIVED>
	T_DERIVED* GetObject()
	{
		if (T_DERIVED::GetType() == MType)
		{
			return reinterpret_cast<T_DERIVED*>(MDerivedThis);
		}
		return nullptr;
	}

	ImplicitObjectType GetType()
	{
		return MType;
	}

	T SignedDistance(const TVector<T, d>& x) const
	{
		TVector<T, d> Normal;
		return PhiWithNormal(x, Normal);
	}
	TVector<T, d> Normal(const TVector<T, d>& x) const
	{
		TVector<T, d> Normal;
		PhiWithNormal(x, Normal);
		return Normal;
	}
	T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const { return MPhiWithNormal(x, Normal); }
	const TBox<T, d>& BoundingBox() const { return MBoundingBox(); }
	bool HasBoundingBox() const { return MBoundingBox != nullptr; }
	bool IsConvex() const { return bIsConvex; }

	TVector<T, d> Support(const TVector<T, d>& Direction) const;
	Pair<TVector<T, d>, bool> FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const;

  protected:
	TFunction<T(const TVector<T, d>&, TVector<T, d>&)> MPhiWithNormal;
	TFunction<const TBox<T, d>&()> MBoundingBox;
	TFunction<Pair<TVector<T, d>, bool>(const TVector<T, d>&, const TVector<T, d>&, const T)> MFindClosestIntersection;
	TFunction<TVector<T, d>(const TVector<T, d>&)> MSupport;
	bool bIsConvex;
	ImplicitObjectType MType;
	void* MDerivedThis;
};
}
