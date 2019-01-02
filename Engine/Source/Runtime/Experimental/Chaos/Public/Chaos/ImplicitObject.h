// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Pair.h"
#include "Chaos/Vector.h"

#include <functional>
#include "Transform.h"

namespace Chaos
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
	Union,
	LevelSet,
	Unknown
};

namespace EImplicitObject
{
	enum Flags
	{
		IsConvex = 1,
		HasBoundingBox = 1 << 1,
		IgnoreAnalyticCollisions = 1 << 2,
	};

	const int32 FiniteConvex = IsConvex | HasBoundingBox;
}

template<class T, int d>
class CHAOS_API TImplicitObject
{
public:
	TImplicitObject(int32 Flags = 0, ImplicitObjectType InType = ImplicitObjectType::Unknown);
	TImplicitObject(const TImplicitObject<T, d>&) = delete;
	TImplicitObject(TImplicitObject<T, d>&&) = delete;
	virtual ~TImplicitObject();

	template<class T_DERIVED>
	T_DERIVED* GetObject()
	{
		if (T_DERIVED::GetType() == Type)
		{
			return static_cast<T_DERIVED*>(this);
		}
		return nullptr;
	}

	template<class T_DERIVED>
	const T_DERIVED* GetObject() const
	{
		if (T_DERIVED::GetType() == Type)
		{
			return static_cast<const T_DERIVED*>(this);
		}
		return nullptr;
	}

	ImplicitObjectType GetType() const
	{
		if (bIgnoreAnalyticCollisions)
		{
			return ImplicitObjectType::Unknown;
		}
		return Type;
	}

	//This is strictly used for optimization purposes
	bool IsUnderlyingUnion() const
	{
		return Type == ImplicitObjectType::Union;
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
	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const = 0;
	virtual const class TBox<T, d>& BoundingBox() const;
	virtual TVector<T, d> Support(const TVector<T, d>& Direction, const T Thickness) const;
	bool HasBoundingBox() const { return bHasBoundingBox; }
	bool IsConvex() const { return bIsConvex; }
	void IgnoreAnalyticCollisions(const bool Ignore = true) { bIgnoreAnalyticCollisions = Ignore; }
	void SetConvex(const bool Convex = true) { bIsConvex = Convex; }

	Pair<TVector<T, d>, bool> FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const;

	virtual void AccumulateAllImplicitObjects(TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TRigidTransform<T, d>& ParentTM) const
	{
		Out.Add(MakePair(this, ParentTM));
	}

	virtual void FindAllIntersectingObjects(TArray < Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TBox<T, d>& LocalBounds) const;

	virtual FString ToString() const
	{
		return FString::Printf(TEXT("ImplicitObject bIsConvex:%d, bIgnoreAnalyticCollision:%d, bHasBoundingBox:%d"), bIsConvex, bIgnoreAnalyticCollisions, bHasBoundingBox);
	}

protected:
	ImplicitObjectType Type;
	bool bIsConvex;
	bool bIgnoreAnalyticCollisions;
	bool bHasBoundingBox;

private:
	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const;
};
}
