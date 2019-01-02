// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Box.h"
#include "Chaos/ImplicitObject.h"

namespace Chaos
{
template<class T, int d>
class TSphere final : public TImplicitObject<T, d>
{
  public:
	TSphere(const TVector<T, d>& Center, const T Radius)
	    : TImplicitObject<T, d>(EImplicitObject::IsConvex | EImplicitObject::HasBoundingBox, ImplicitObjectType::Sphere)
	    , MCenter(Center)
	    , MRadius(Radius)
	    , MLocalBoundingBox(MCenter - MRadius, MCenter + MRadius)
	{
	}
	TSphere(const TSphere<T, d>& Other)
		: TImplicitObject<T, d>(EImplicitObject::IsConvex | EImplicitObject::HasBoundingBox, ImplicitObjectType::Sphere)
	    , MCenter(Other.MCenter)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(Other.MLocalBoundingBox)
	{
	}
	TSphere(TSphere<T, d>&& Other)
		: TImplicitObject<T, d>(EImplicitObject::IsConvex | EImplicitObject::HasBoundingBox, ImplicitObjectType::Sphere)
	    , MCenter(MoveTemp(Other.MCenter))
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
	}
	virtual ~TSphere() {}

	static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Sphere;
	}

	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override
	{
		const TVector<T, d> v = x - MCenter;
		Normal = v.GetSafeNormal();
		return v.Size() - MRadius;
	}

	bool Intersects(const TSphere<T, d>& Other) const
	{
		T CenterDistance = FVector::DistSquared(Other.Center(), Center());
		T RadialSum = Other.Radius() + Radius();
		return RadialSum >= CenterDistance;
	}

	TVector<T, d> FindClosestPoint(const TVector<T, d>& StartPoint, const T Thickness = (T)0) const
	{
		TVector<T, 3> Result = MCenter + (StartPoint - MCenter).GetSafeNormal() * (MRadius + Thickness);
		return Result;
	}

	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const override
	{
		TVector<T, d> Direction = EndPoint - StartPoint;
		T Length = Direction.Size();
		Direction = Direction.GetSafeNormal();
		TVector<T, d> SphereToStart = StartPoint - MCenter;
		T DistanceProjected = TVector<T, d>::DotProduct(Direction, SphereToStart);
		T EffectiveRadius = MRadius + Thickness;
		T UnderRoot = DistanceProjected * DistanceProjected - SphereToStart.SizeSquared() + EffectiveRadius * EffectiveRadius;
		if (UnderRoot < 0)
		{
			return MakePair(TVector<T, d>(0), false);
		}
		if (UnderRoot == 0)
		{
			if (-DistanceProjected < 0 || -DistanceProjected > Length)
			{
				return MakePair(TVector<T, d>(0), false);
			}
			return MakePair(TVector<T, d>(-DistanceProjected * Direction + StartPoint), true);
		}
		T Root1 = -DistanceProjected + sqrt(UnderRoot);
		T Root2 = -DistanceProjected - sqrt(UnderRoot);
		if (Root1 < 0 || Root1 > Length)
		{
			if (Root2 < 0 || Root2 > Length)
			{
				return MakePair(TVector<T, d>(0), false);
			}
			return MakePair(TVector<T, d>(Root2 * Direction + StartPoint), true);
		}
		if (Root2 < 0 || Root2 > Length)
		{
			return MakePair(TVector<T, d>(Root1 * Direction + StartPoint), true);
		}
		if (Root1 < Root2)
		{
			return MakePair(TVector<T, d>(Root1 * Direction + StartPoint), true);
		}
		return MakePair(TVector<T, d>(Root2 * Direction + StartPoint), true);
	}

	virtual TVector<T, d> Support(const TVector<T, d>& Direction, const T Thickness) const override
	{
		return MCenter + Direction.GetSafeNormal() * MRadius + Thickness;
	}

	virtual const TBox<T, d>& BoundingBox() const { return MLocalBoundingBox; }

	const TVector<T, d>& Center() const
	{
		return MCenter;
	}

	const T& Radius() const
	{
		return MRadius;
	}

	virtual FString ToString() const
	{
		return FString::Printf(TEXT("TSphere Center:%s, Radius:%f"), *Center().ToString(), Radius());
	}

  private:
	TVector<T, d> MCenter;
	T MRadius;
	TBox<T, d> MLocalBoundingBox;
};
}
