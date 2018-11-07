// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Box.h"
#include "Apeiron/ImplicitObject.h"
#include "Apeiron/Transform.h"

namespace Apeiron
{
template<class T, int d>
class TImplicitObjectTransformed : public TImplicitObject<T, d>
{
  public:
	TImplicitObjectTransformed(const TImplicitObject<T, d>* Object, const TRigidTransform<T, d>& InTransform)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); },
	          [this](const TVector<T, d>& Direction) { return Support(Direction); },
	          ImplicitObjectType::Transformed, this)
	    , MObject(Object)
	    , MTransform(InTransform)
	    , MLocalBoundingBox(Object->BoundingBox().TransformedBox(InTransform))
	{
		this->bIsConvex = Object->IsConvex();
	}
	TImplicitObjectTransformed(const TImplicitObjectTransformed<T, d>& Other) = delete;
	TImplicitObjectTransformed(TImplicitObjectTransformed<T, d>&& Other)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); },
	          [this](const TVector<T, d>& Direction) { return Support(Direction); },
	          ImplicitObjectType::Transformed, this)
	    , MObject(Other.MObject)
	    , MTransform(Other.MTransform)
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
		this->bIsConvex = Other.MObject->IsConvex();
		Other.MPhiWithNormal = nullptr;
		Other.MBoundingBox = nullptr;
	}
	~TImplicitObjectTransformed() {}

	static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Transformed;
	}

	T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const
	{
		auto TransformedX = MTransform.InverseTransformPosition(x);
		auto Phi = MObject->PhiWithNormal(TransformedX, Normal);
		Normal = MTransform.TransformVector(Normal);
		return Phi;
	}

	Pair<TVector<T, d>, bool> FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness)
	{
		auto TransformedStart = MTransform.InverseTransformPosition(StartPoint);
		auto TransformedEnd = MTransform.InverseTransformPosition(EndPoint);
		auto ClosestIntersection = MObject->FindClosestIntersection(TransformedStart, TransformedEnd, Thickness);
		if (ClosestIntersection.Second)
		{
			ClosestIntersection.First = MTransform.TransformPosition(ClosestIntersection.First);
		}
		return ClosestIntersection;
	}

	TVector<T, d> Support(const TVector<T, d>& Direction) const
	{
		return MTransform.TransformPosition(MObject->Support(MTransform.InverseTransformVector(Direction)));
	}

	const TRigidTransform<T, d>& GetTransform() const { return MTransform; }
	void SetTransform(const TRigidTransform<T, d>& InTransform)
	{
		MLocalBoundingBox = MObject->BoundingBox().TransformedBox(InTransform);
		MTransform = InTransform;
	}

	const TImplicitObject<T, d>* Object() const { return MObject; }

  private:
	const TImplicitObject<T, d>* MObject;
	TRigidTransform<T, d> MTransform;
	TBox<T, d> MLocalBoundingBox;
};
}
