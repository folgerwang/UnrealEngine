// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Cylinder.h"
#include "Apeiron/ImplicitObject.h"
#include "Apeiron/ImplicitObjectUnion.h"
#include "Apeiron/Sphere.h"

namespace Apeiron
{
template<class T>
class Capsule : public TImplicitObject<T, 3>
{
  public:
	using TImplicitObject<T, 3>::SignedDistance;

	Capsule(const TVector<T, 3>& x1, const TVector<T, 3>& x2, const T Radius)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPoint(x1)
	    , MVector((x2 - x1).GetSafeNormal())
	    , MHeight((x2 - x1).Size())
	    , MRadius(Radius)
	    , MLocalBoundingBox(x1, x1)
	    , MUnionedObjects(nullptr)
	{
		this->bIsConvex = true;
		MLocalBoundingBox.GrowToInclude(x2);
		MLocalBoundingBox = TBox<T, 3>(MLocalBoundingBox.Min() - TVector<T, 3>(MRadius), MLocalBoundingBox.Max() + TVector<T, 3>(MRadius));
	}
	Capsule(const Capsule<T>& Other)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPoint(Other.MPoint)
	    , MVector(Other.MVector)
	    , MHeight(Other.MHeight)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(Other.MLocalBoundingBox)
	    , MUnionedObjects(nullptr)
	{
		this->bIsConvex = true;
	}
	Capsule(Capsule<T>&& Other)
	    : TImplicitObject<T, 3>([this](const TVector<T, 3>& x, TVector<T, 3>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, 3>& { return MLocalBoundingBox; },
	          [this](const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MPoint(std::move(Other.MPoint))
	    , MVector(std::move(Other.MVector))
	    , MHeight(Other.MHeight)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(std::move(Other.MLocalBoundingBox))
	    , MUnionedObjects(std::move(Other.MUnionedObjects))
	{
		this->bIsConvex = true;
		Other.MPhiWithNormal = nullptr;
	}
	~Capsule() {}

	T PhiWithNormal(const TVector<T, 3>& x, TVector<T, 3>& Normal) const
	{
		auto Dot = TVector<T, 3>::DotProduct(x - MPoint, MVector);
		if (Dot < 0)
		{
			Dot = 0;
		}
		if (Dot > MHeight)
		{
			Dot = MHeight;
		}
		TVector<T, 3> ProjectedPoint = Dot * MVector + MPoint;
		auto Difference = x - ProjectedPoint;
		Normal = Difference.GetSafeNormal();
		return (Difference.Size() - MRadius);
	}

	Pair<TVector<T, 3>, bool> FindClosestIntersection(const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness)
	{
		if (!MUnionedObjects)
		{
			TArray<TUniquePtr<TImplicitObject<float, 3>>> Objects;

			Objects.Add(MakeUnique<Apeiron::TCylinder<float>>(MPoint, MPoint + MVector * MHeight, MRadius));
			Objects.Add(MakeUnique<Apeiron::TSphere<float, 3>>(MPoint, MRadius));
			Objects.Add(MakeUnique<Apeiron::TSphere<float, 3>>(MPoint + MVector * MHeight, MRadius));

			MUnionedObjects.Reset(new Apeiron::TImplicitObjectUnion<float, 3>(std::move(Objects)));
		}
		return MUnionedObjects->FindClosestIntersection(StartPoint, EndPoint, Thickness);
	}

  private:
	TVector<T, 3> MPoint, MVector;
	T MHeight, MRadius;
	TBox<T, 3> MLocalBoundingBox;
	TUniquePtr<TImplicitObjectUnion<T, 3>> MUnionedObjects;
};
}
