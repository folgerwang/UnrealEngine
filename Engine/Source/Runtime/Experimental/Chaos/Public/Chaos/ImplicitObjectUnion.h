// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/ImplicitObject.h"

#include <memory>
#include "BoundingVolumeHierarchy.h"

namespace Chaos
{
template<class T, int d>
class TImplicitObjectUnion : public TImplicitObject<T, d>
{
  public:
	TImplicitObjectUnion(TArray<TUniquePtr<TImplicitObject<T, d>>>&& Objects)
	    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::Union)
	    , MObjects(MoveTemp(Objects))
	    , MLocalBoundingBox(MObjects[0]->BoundingBox())
	{
		for (int32 i = 1; i < MObjects.Num(); ++i)
		{
			MLocalBoundingBox.GrowToInclude(MObjects[i]->BoundingBox());
		}

		CacheAllImplicitObjects();

	}
	TImplicitObjectUnion(const TImplicitObjectUnion<T, d>& Other) = delete;
	TImplicitObjectUnion(TImplicitObjectUnion<T, d>&& Other)
	    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox)
	    , MObjects(MoveTemp(Other.MObjects))
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
	}
	virtual ~TImplicitObjectUnion() {}

	FORCEINLINE static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Union;
	}

	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override
	{
		check(MObjects.Num());
		T Phi = MObjects[0]->PhiWithNormal(x, Normal);
		for (int32 i = 1; i < MObjects.Num(); ++i)
		{
			TVector<T, d> NextNormal;
			T NextPhi = MObjects[i]->PhiWithNormal(x, NextNormal);
			if (NextPhi < Phi)
			{
				Phi = NextPhi;
				Normal = NextNormal;
			}
			else if (NextPhi == Phi)
			{
				Normal += NextNormal;
			}
		}
		Normal.Normalize();
		return Phi;
	}

	virtual const TBox<T, d>& BoundingBox() const override { return MLocalBoundingBox; }

	virtual void AccumulateAllImplicitObjects(TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TRigidTransform<T, d>& ParentTM) const
	{
		for (const TUniquePtr<TImplicitObject<T, d>>& Object : MObjects)
		{
			Object->AccumulateAllImplicitObjects(Out, ParentTM);
		}
	}

	virtual void FindAllIntersectingObjects(TArray < Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TBox<T, d>& LocalBounds) const
	{
		if (Hierarchy)
		{
			TArray<int32> Overlaps = Hierarchy->FindAllIntersections(LocalBounds);
			Out.Reserve(Out.Num() + Overlaps.Num());
			for (int32 Idx : Overlaps)
			{
				const TImplicitObject<T, d>* Obj = GeomParticles.Geometry(Idx);
				Out.Add(MakePair(Obj, TRigidTransform<T, d>(GeomParticles.X(Idx), GeomParticles.R(Idx))));
			}
		}
		else
		{
			for (const TUniquePtr<TImplicitObject<T, d>>& Object : MObjects)
			{
				Object->FindAllIntersectingObjects(Out, LocalBounds);
			}
		}
	}

	virtual void CacheAllImplicitObjects()
	{
		TArray < Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> SubObjects;
		AccumulateAllImplicitObjects(SubObjects, TRigidTransform<T, d>::Identity);
		//build hierarchy
		const int32 NumObjects = SubObjects.Num();
		constexpr int32 MinSubObjectsToCache = 8;	//todo(make this tunable?)
		if (NumObjects > MinSubObjectsToCache)
		{
			GeomParticles.Resize(NumObjects);
			for (int32 i = 0; i < NumObjects; ++i)
			{
				GeomParticles.X(i) = SubObjects[i].Second.GetLocation();
				GeomParticles.R(i) = SubObjects[i].Second.GetRotation();
				GeomParticles.Geometry(i) = const_cast<TImplicitObject<T, d>*>(SubObjects[i].First);	//this is ok because this function is not const. Otherwise we'd need to duplicate a lot of logic
			}
			Hierarchy = TUniquePtr<TBoundingVolumeHierarchy<TGeometryParticles<T, d>, T, d>>(new TBoundingVolumeHierarchy<TGeometryParticles<T, d>, T, d>(GeomParticles,1));
		}
	}

private:
	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const override
	{
		check(MObjects.Num());
		auto ClosestIntersection = MObjects[0]->FindClosestIntersection(StartPoint, EndPoint, Thickness);
		T Length = ClosestIntersection.Second ? (ClosestIntersection.First - StartPoint).Size() : 0;
		for (int32 i = 1; i < MObjects.Num(); ++i)
		{
			auto NextClosestIntersection = MObjects[i]->FindClosestIntersection(StartPoint, EndPoint, Thickness);
			if (!NextClosestIntersection.Second)
				continue;
			T NewLength = (NextClosestIntersection.First - StartPoint).Size();
			if (!ClosestIntersection.Second || NewLength < Length)
			{
				Length = NewLength;
				ClosestIntersection = NextClosestIntersection;
			}
		}
		return ClosestIntersection;
	}

  private:
	TArray<TUniquePtr<TImplicitObject<T, d>>> MObjects;
	TGeometryParticles<T, d> GeomParticles;
	TUniquePtr<TBoundingVolumeHierarchy<TGeometryParticles<T, d>, T, d>> Hierarchy;
	TBox<T, d> MLocalBoundingBox;
};
}
