// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Cylinder.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Sphere.h"

namespace Chaos
{
template<class T>
class TCapsule final : public TImplicitObject<T, 3>
{
  public:
	using TImplicitObject<T, 3>::SignedDistance;

	TCapsule(const TVector<T, 3>& x1, const TVector<T, 3>& x2, const T Radius)
	    : TImplicitObject<T, 3>(EImplicitObject::FiniteConvex)
	    , MPoint(x1)
	    , MVector((x2 - x1).GetSafeNormal())
	    , MHeight((x2 - x1).Size())
	    , MRadius(Radius)
	    , MLocalBoundingBox(x1, x1)
	    , MUnionedObjects(nullptr)
	{
		MLocalBoundingBox.GrowToInclude(x2);
		MLocalBoundingBox = TBox<T, 3>(MLocalBoundingBox.Min() - TVector<T, 3>(MRadius), MLocalBoundingBox.Max() + TVector<T, 3>(MRadius));
		InitUnionedObjects();
	}
	TCapsule(const TCapsule<T>& Other)
		: TImplicitObject<T, 3>(EImplicitObject::FiniteConvex)
	    , MPoint(Other.MPoint)
	    , MVector(Other.MVector)
	    , MHeight(Other.MHeight)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(Other.MLocalBoundingBox)
	    , MUnionedObjects(nullptr)
	{
		InitUnionedObjects();
	}
	TCapsule(TCapsule<T>&& Other)
	    : TImplicitObject<T, 3>(EImplicitObject::FiniteConvex)
	    , MPoint(std::move(Other.MPoint))
	    , MVector(std::move(Other.MVector))
	    , MHeight(Other.MHeight)
	    , MRadius(Other.MRadius)
	    , MLocalBoundingBox(std::move(Other.MLocalBoundingBox))
	    , MUnionedObjects(std::move(Other.MUnionedObjects))
	{
		InitUnionedObjects();
	}
	~TCapsule() {}

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

	virtual const TBox<T, 3>& BoundingBox() const override { return MLocalBoundingBox; }

	const T& GetRadius() const { return MRadius; }

	const T& GetHeight() const { return MHeight; }

private:

	void InitUnionedObjects()
	{
		TArray<TUniquePtr<TImplicitObject<float, 3>>> Objects;

		Objects.Add(MakeUnique<Chaos::TCylinder<float>>(MPoint, MPoint + MVector * MHeight, MRadius));
		Objects.Add(MakeUnique<Chaos::TSphere<float, 3>>(MPoint, MRadius));
		Objects.Add(MakeUnique<Chaos::TSphere<float, 3>>(MPoint + MVector * MHeight, MRadius));

		MUnionedObjects.Reset(new Chaos::TImplicitObjectUnion<float, 3>(std::move(Objects)));
	}

	virtual Pair<TVector<T, 3>, bool> FindClosestIntersectionImp(const TVector<T, 3>& StartPoint, const TVector<T, 3>& EndPoint, const T Thickness) const override
	{
		return MUnionedObjects->FindClosestIntersection(StartPoint, EndPoint, Thickness);
	}

  private:
	TVector<T, 3> MPoint, MVector;
	T MHeight, MRadius;
	TBox<T, 3> MLocalBoundingBox;
	TUniquePtr<TImplicitObjectUnion<T, 3>> MUnionedObjects;
};
}
