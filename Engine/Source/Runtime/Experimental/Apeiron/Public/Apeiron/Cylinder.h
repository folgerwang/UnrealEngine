// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ImplicitObject.h"
#include "Apeiron/Plane.h"
#include "Apeiron/Sphere.h"

namespace Apeiron
{
template<class T>
class TCylinder : public TImplicitObject<T, 3>
{
  public:
	using TImplicitObject<T, 3>::SignedDistance;

	TCylinder(const TVector<T, 3>& x1, const TVector<T, 3>& x2, const T Radius)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPlane1(x1, (x2 - x1).GetSafeNormal())
	    , MPlane2(x2, -MPlane1.Normal())
	    , MHeight((x2 - x1).Size())
	    , MRadius(Radius)
	    , MLocalBoundingBox(x1, x1)
	{
		this->bIsConvex = true;
		MLocalBoundingBox.GrowToInclude(x2);
		MLocalBoundingBox = TBox<T, 3>(MLocalBoundingBox.Min() - TVector<T, 3>(MRadius), MLocalBoundingBox.Max() + TVector<T, 3>(MRadius));
	}
	TCylinder(const TCylinder<T>& Other)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPlane1(Other.MPlane1)
	    , MPlane2(Other.MPlane2)
	    , MHeight(Other.MHeight)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(Other.MLocalBoundingBox)
	{
		this->bIsConvex = true;
	}
	TCylinder(TCylinder<T>&& Other)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPlane1(MoveTemp(Other.MPlane1))
	    , MPlane2(MoveTemp(Other.MPlane2))
	    , MHeight(Other.MHeight)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
		this->bIsConvex = true;
		Other.MPhiWithNormal = nullptr;
	}
	~TCylinder() {}

	T PhiWithNormal(const TVector<T, 3>& x, TVector<T, 3>& Normal) const
	{
		TVector<T, 3> Normal1, Normal2;
		const T Distance1 = MPlane1.PhiWithNormal(x, Normal1);
		const T Distance2 = MPlane2.PhiWithNormal(x, Normal2);
		if (Distance1 < 0)
		{
			check(Distance2 > 0);
			const TVector<T, 3> v = x - TVector<T, 3>(Normal1 * Distance1 + MPlane1.X());
			if (v.Size() > MRadius)
			{
				const TVector<T, 3> Corner = v.GetSafeNormal() * MRadius + MPlane1.X();
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
			if (v.Size() > MRadius)
			{
				const TVector<T, 3> Corner = v.GetSafeNormal() * MRadius + MPlane2.X();
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
		const T SideDistance = SideVector.Size() - MRadius;
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
		// Flatten to Plane defined by StartPoint and MPlane1.Normal()
		// Project End and Center into Plane
		TVector<T, 3> ProjectedEnd = EndPoint - TVector<T, 3>::DotProduct(EndPoint - StartPoint, MPlane1.Normal()) * MPlane1.Normal();
		TVector<T, 3> ProjectedCenter = MPlane1.X() - TVector<T, 3>::DotProduct(MPlane1.X() - StartPoint, MPlane1.Normal()) * MPlane1.Normal();
		auto ProjectedSphere = TSphere<T, 3>(ProjectedCenter, MRadius);
		auto InfiniteCylinderIntersection = ProjectedSphere.FindClosestIntersection(StartPoint, ProjectedEnd, Thickness);
		if (InfiniteCylinderIntersection.Second)
		{
			auto UnprojectedIntersection = TPlane<T, 3>(InfiniteCylinderIntersection.First, (StartPoint - InfiniteCylinderIntersection.First).GetSafeNormal()).FindClosestIntersection(StartPoint, EndPoint, 0);
			check(UnprojectedIntersection.Second);
			Intersections.Add(MakePair((UnprojectedIntersection.First - StartPoint).Size(), UnprojectedIntersection.First));
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
	TPlane<T, 3> MPlane1, MPlane2;
	T MHeight, MRadius;
	TBox<T, 3> MLocalBoundingBox;
};
}
