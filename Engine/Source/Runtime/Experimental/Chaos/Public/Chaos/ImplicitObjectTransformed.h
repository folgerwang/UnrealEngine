// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Box.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Transform.h"

namespace Chaos
{
template<class T, int d>
class TImplicitObjectTransformed final : public TImplicitObject<T, d>
{
  public:
	TImplicitObjectTransformed(const TImplicitObject<T, d>* Object, const TRigidTransform<T, d>& InTransform)
	    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::Transformed)
	    , MObject(Object)
	    , MTransform(InTransform)
	    , MLocalBoundingBox(Object->BoundingBox().TransformedBox(InTransform))
	{
		this->bIsConvex = Object->IsConvex();
	}
	TImplicitObjectTransformed(const TImplicitObjectTransformed<T, d>& Other) = delete;
	TImplicitObjectTransformed(TImplicitObjectTransformed<T, d>&& Other)
	    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::Transformed)
	    , MObject(Other.MObject)
	    , MTransform(Other.MTransform)
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
		this->bIsConvex = Other.MObject->IsConvex();
	}
	~TImplicitObjectTransformed() {}

	static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Transformed;
	}

	const TImplicitObject<T, d>* GetTransformedObject() const
	{
		return MObject;
	}

	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override
	{
		auto TransformedX = MTransform.InverseTransformPosition(x);
		auto Phi = MObject->PhiWithNormal(TransformedX, Normal);
		Normal = MTransform.TransformVector(Normal);
		return Phi;
	}

	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const override
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

	virtual TVector<T, d> Support(const TVector<T, d>& Direction, const T Thickness) const override
	{
		return MTransform.TransformPosition(MObject->Support(MTransform.InverseTransformVector(Direction), Thickness));
	}

	const TRigidTransform<T, d>& GetTransform() const { return MTransform; }
	void SetTransform(const TRigidTransform<T, d>& InTransform)
	{
		MLocalBoundingBox = MObject->BoundingBox().TransformedBox(InTransform);
		MTransform = InTransform;
	}

	virtual void AccumulateAllImplicitObjects(TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TRigidTransform<T, d>& ParentTM) const
	{
		const TRigidTransform<T, d> NewTM = MTransform * ParentTM;
		MObject->AccumulateAllImplicitObjects(Out, NewTM);
		
	}

	virtual void FindAllIntersectingObjects(TArray < Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TBox<T, d>& LocalBounds) const
	{
		const TBox<T, d> SubobjectBounds = LocalBounds.TransformedBox(MTransform.Inverse());
		MObject->FindAllIntersectingObjects(Out, SubobjectBounds);
	}

	virtual const TBox<T, d>& BoundingBox() const override { return MLocalBoundingBox; }

	const TImplicitObject<T, d>* Object() const { return MObject; }

  private:
	const TImplicitObject<T, d>* MObject;
	TRigidTransform<T, d> MTransform;
	TBox<T, d> MLocalBoundingBox;
};
}
