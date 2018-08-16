// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/ImplicitObject.h"

#include <memory>

namespace Apeiron
{
template<class T, int d>
class TImplicitObjectUnion : public TImplicitObject<T, d>
{
  public:
	TImplicitObjectUnion(TArray<TUniquePtr<TImplicitObject<T, d>>>&& Objects)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MObjects(MoveTemp(Objects))
	    , MLocalBoundingBox(MObjects[0]->BoundingBox())
	{
		for (int32 i = 1; i < MObjects.Num(); ++i)
		{
			MLocalBoundingBox.GrowToInclude(MObjects[i]->BoundingBox());
		}
	}
	TImplicitObjectUnion(const TImplicitObjectUnion<T, d>& Other) = delete;
	TImplicitObjectUnion(TImplicitObjectUnion<T, d>&& Other)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); }, nullptr)
	    , MObjects(MoveTemp(Other.MObjects))
	    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
	{
		Other.MPhiWithNormal = nullptr;
		Other.MBoundingBox = nullptr;
	}
	~TImplicitObjectUnion() {}

	T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const
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

	Pair<TVector<T, d>, bool> FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness)
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
	TBox<T, d> MLocalBoundingBox;
};
}
