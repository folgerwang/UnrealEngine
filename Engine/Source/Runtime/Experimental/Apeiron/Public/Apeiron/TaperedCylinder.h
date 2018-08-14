// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ImplicitObject.h"
#include "Apeiron/Plane.h"

namespace Apeiron
{
template<class T>
class TTaperedCylinder : public TImplicitObject<T, 3>
{
  public:
	TTaperedCylinder(const TVector<T, 3>& x1, const TVector<T, 3>& x2, const T Radius1, const T Radius2)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPlane1(x1, (x2 - x1).GetSafeNormal())
	    , MPlane2(x2, -MPlane1.Normal())
	    , MHeight((x2 - x1).Size())
	    , MRadius1(Radius1)
	    , MRadius2(Radius2)
	    , MLocalBoundingBox(x1, x1)
	{
		this->bIsConvex = true;
		MLocalBoundingBox.GrowToInclude(x2);
		T MaxRadius = MRadius1;
		if (MaxRadius < MRadius2)
			MaxRadius = MRadius2;
		MLocalBoundingBox = TBox<T, 3>(MLocalBoundingBox.Min() - TVector<T, 3>(MaxRadius), MLocalBoundingBox.Max() + TVector<T, 3>(MaxRadius));
	}
	TTaperedCylinder(const TTaperedCylinder<T>& Other)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPlane1(Other.MPlane1)
	    , MPlane2(Other.MPlane2)
	    , MHeight(Other.MHeight)
	    , MRadius1(Other.MRadius1)
	    , MRadius2(Other.MRadius2)
	    , MLocalBoundingBox(Other.MLocalBoundingBox)
	{
		this->bIsConvex = true;
	}
	TTaperedCylinder(TTaperedCylinder<T>&& Other)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPlane1(MoveTemp(Other.MPlane1))
	    , MPlane2(MoveTemp(Other.MPlane2))
	    , MHeight(Other.MHeight)
	    , MRadius1(Other.MRadius1)
	    , MRadius2(Other.MRadius2)
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
		this->bIsConvex = true;
		Other.MPhiWithNormal = nullptr;
	}
	~TTaperedCylinder() {}

	T PhiWithNormal(const TVector<T, 3>& x, TVector<T, 3>& Normal) const
	{
		TVector<T, 3> Normal1, Normal2;
		const T Distance1 = MPlane1.PhiWithNormal(x, Normal1);
		const T Distance2 = MPlane2.PhiWithNormal(x, Normal2);
		if (Distance1 < 0)
		{
			check(Distance2 > 0);
			const TVector<T, 3> v = x - TVector<T, 3>(Normal1 * Distance1 + MPlane1.X());
			if (v.Size() > MRadius1)
			{
				const TVector<T, 3> Corner = v.GetSafeNormal() * MRadius1 + MPlane1.X();
				const TVector<T, 3> CornerVector = x - Corner;
				Normal = CornerVector.GetSafeNormal();
				return CornerVector.Size();
			}
			else
			{
				Normal = -Normal1;
				return -Distance1;
			}
		}
		if (Distance2 < 0)
		{
			check(Distance1 > 0);
			const TVector<T, 3> v = x - TVector<T, 3>(Normal2 * Distance2 + MPlane2.X());
			if (v.Size() > MRadius2)
			{
				const TVector<T, 3> Corner = v.GetSafeNormal() * MRadius2 + MPlane2.X();
				const TVector<T, 3> CornerVector = x - Corner;
				Normal = CornerVector.GetSafeNormal();
				return CornerVector.Size();
			}
			else
			{
				Normal = -Normal2;
				return -Distance2;
			}
		}
		check(Distance1 <= MHeight && Distance2 <= MHeight);
		const TVector<T, 3> SideVector = (x - TVector<T, 3>(Normal1 * Distance1 + MPlane1.X()));
		const T SideDistance = SideVector.Size() - GetRadius(Distance1);
		if (SideDistance < 0)
		{
			const T TopDistance = Distance1 < Distance2 ? Distance1 : Distance2;
			if (TopDistance < -SideDistance)
			{
				Normal = Distance1 < Distance2 ? -Normal1 : -Normal2;
				return -TopDistance;
			}
		}
		Normal = SideVector.GetSafeNormal();
		return SideDistance;
	}

	Pair<TVector<T, 3>, bool> FindClosestIntersection(const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness)
	{
		TArray<Pair<T, TVector<T, 3>>> Intersections;
		T DeltaRadius = FGenericPlatformMath::Abs(MRadius2 - MRadius1);
		if (DeltaRadius == 0)
			return TCylinder<T>(MPlane1.X(), MPlane2.X(), MRadius1).FindClosestIntersection(StartPoint, EndPoint, Thickness);
		TVector<T, 3> BaseNormal;
		T BaseRadius;
		TVector<T, 3> BaseCenter;
		if (MRadius2 > MRadius1)
		{
			BaseNormal = MPlane2.Normal();
			BaseRadius = MRadius2 + Thickness;
			BaseCenter = MPlane2.X();
		}
		else
		{
			BaseNormal = MPlane1.Normal();
			BaseRadius = MRadius1 + Thickness;
			BaseCenter = MPlane1.X();
		}
		TVector<T, 3> Top = BaseRadius / DeltaRadius * MHeight * BaseNormal + BaseCenter;
		T theta = atan2(BaseRadius, (Top - BaseCenter).Size());
		T costheta = cos(theta);
		T cossqtheta = costheta * costheta;
		check(theta > 0 && theta < PI / 2);
		TVector<T, 3> Direction = EndPoint - StartPoint;
		T Length = Direction.Size();
		Direction = Direction.GetSafeNormal();
		auto DDotN = TVector<T, 3>::DotProduct(Direction, -BaseNormal);
		auto SMT = StartPoint - Top;
		auto SMTDotN = TVector<T, 3>::DotProduct(SMT, -BaseNormal);
		T a = DDotN * DDotN - cossqtheta;
		T b = 2 * (DDotN * SMTDotN - TVector<T, 3>::DotProduct(Direction, SMT) * cossqtheta);
		T c = SMTDotN * SMTDotN - SMT.SizeSquared() * cossqtheta;
		T Determinant = b * b - 4 * a * c;
		if (Determinant == 0)
		{
			T Root = -b / (2 * a);
			auto RootPoint = Root * Direction + StartPoint;
			if (Root >= 0 && Root <= Length && TVector<T, 3>::DotProduct(RootPoint - Top, -BaseNormal) >= 0)
			{
				Intersections.Add(MakePair(Root, RootPoint));
			}
		}
		if (Determinant > 0)
		{
			T Root1 = (-b - sqrt(Determinant)) / (2 * a);
			T Root2 = (-b + sqrt(Determinant)) / (2 * a);
			auto Root1Point = Root1 * Direction + StartPoint;
			auto Root2Point = Root2 * Direction + StartPoint;
			if (Root1 < 0 || Root1 > Length || TVector<T, 3>::DotProduct(Root1Point - Top, -BaseNormal) < 0)
			{
				if (Root2 >= 0 && Root2 <= Length && TVector<T, 3>::DotProduct(Root2Point - Top, -BaseNormal) >= 0)
				{
					Intersections.Add(MakePair(Root2, Root2Point));
				}
			}
			else if (Root2 < 0 || Root2 > Length || TVector<T, 3>::DotProduct(Root2Point - Top, -BaseNormal) < 0)
			{
				Intersections.Add(MakePair(Root1, Root1Point));
			}
			else if (Root1 < Root2 && TVector<T, 3>::DotProduct(Root1Point - Top, -BaseNormal) >= 0)
			{
				Intersections.Add(MakePair(Root1, Root1Point));
			}
			else if (TVector<T, 3>::DotProduct(Root2Point - Top, -BaseNormal) >= 0)
			{
				Intersections.Add(MakePair(Root2, Root2Point));
			}
		}
		auto Plane1Intersection = MPlane1.FindClosestIntersection(StartPoint, EndPoint, Thickness);
		if (Plane1Intersection.Second)
			Intersections.Add(MakePair((Plane1Intersection.First - StartPoint).Size(), Plane1Intersection.First));
		auto Plane2Intersection = MPlane2.FindClosestIntersection(StartPoint, EndPoint, Thickness);
		if (Plane2Intersection.Second)
			Intersections.Add(MakePair((Plane2Intersection.First - StartPoint).Size(), Plane2Intersection.First));
		Intersections.Sort([](const Pair<T, TVector<T, 3>>& Elem1, const Pair<T, TVector<T, 3>>& Elem2) { return Elem1.First < Elem2.First; });
		for (const auto& Elem : Intersections)
		{
			if (SignedDistance(Elem.Second) <= (Thickness + 1e-4))
			{
				return MakePair(Elem.Second, true);
			}
		}
		return MakePair(TVector<T, 3>(0), false);
	}

  private:
	//Phi is distance from closest point on plane1
	T GetRadius(const T& Phi) const
	{
		T Alpha = Phi / MHeight;
		return MRadius1 * (1 - Alpha) + MRadius2 * Alpha;
	}

	TPlane<T, 3> MPlane1, MPlane2;
	T MHeight, MRadius1, MRadius2;
	TBox<T, 3> MLocalBoundingBox;
};
}
