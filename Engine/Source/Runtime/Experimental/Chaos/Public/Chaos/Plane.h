// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ImplicitObject.h"

#include <algorithm>
#include <cmath>

namespace Chaos
{
template<class T, int d>
class TPlane final : public TImplicitObject<T, d>
{
  public:
	TPlane(const TVector<T, d>& InX, const TVector<T, d>& InNormal)
	    : TImplicitObject<T, d>(0, ImplicitObjectType::Plane)
	    , MX(InX)
	    , MNormal(InNormal)
	{
	}
	TPlane(const TPlane<T, d>& Other)
	    : TImplicitObject<T, d>(0, ImplicitObjectType::Plane)
	    , MX(Other.MX)
	    , MNormal(Other.MNormal)
	{
	}
	TPlane(TPlane<T, d>&& Other)
	    : TImplicitObject<T, d>(0, ImplicitObjectType::Plane)
	    , MX(MoveTemp(Other.MX))
	    , MNormal(MoveTemp(Other.MNormal))
	{
	}
	virtual ~TPlane() {}

	static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Plane;
	}

	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override
	{
		Normal = MNormal;
		return TVector<T, d>::DotProduct(x - MX, MNormal);
	}

	TVector<T, d> FindClosestPoint(const TVector<T, d>& x, const T Thickness = (T)0) const
	{
		auto Dist = TVector<T, d>::DotProduct(x - MX, MNormal) - Thickness;
		return x - TVector<T, d>(Dist * MNormal);
	}

	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const override
	{
		TVector<T, d> Direction = EndPoint - StartPoint;
		T Length = Direction.Size();
		Direction = Direction.GetSafeNormal();
		TVector<T, d> XPos = MX + MNormal * Thickness;
		TVector<T, d> XNeg = MX - MNormal * Thickness;
		TVector<T, d> EffectiveX = ((XNeg - StartPoint).Size() < (XPos - StartPoint).Size()) ? XNeg : XPos;
		TVector<T, d> PlaneToStart = EffectiveX - StartPoint;
		T Denominator = TVector<T, d>::DotProduct(Direction, MNormal);
		if (Denominator == 0)
		{
			if (TVector<T, d>::DotProduct(PlaneToStart, MNormal) == 0)
			{
				return MakePair(EndPoint, true);
			}
			return MakePair(TVector<T, d>(0), false);
		}
		T Root = TVector<T, d>::DotProduct(PlaneToStart, MNormal) / Denominator;
		if (Root < 0 || Root > Length)
		{
			return MakePair(TVector<T, d>(0), false);
		}
		return MakePair(TVector<T, d>(Root * Direction + StartPoint), true);
	}

	const TVector<T, d>& X() const { return MX; }
	const TVector<T, d>& Normal() const { return MNormal; }
	const TVector<T, d>& Normal(const TVector<T, d>&) const { return MNormal; }

  private:
	TVector<T, d> MX;
	TVector<T, d> MNormal;
};
}
