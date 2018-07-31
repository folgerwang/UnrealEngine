// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/ImplicitObject.h"
#include "Apeiron/Box.h"

using namespace Apeiron;

template<class T, int d>
TImplicitObject<T, d>::TImplicitObject(TFunction<T(const TVector<T, d>&, TVector<T, d>&)> InPhiWithNormal, TFunction<const TBox<T, d>&()> InBoundingBox, TFunction<Pair<TVector<T, d>, bool>(const TVector<T, d>&, const TVector<T, d>&, const T)> InFindClosestIntersection, TFunction<TVector<T, d>(const TVector<T, d>&)> InSupport, ImplicitObjectType Type, void* DerivedThis)
    : MPhiWithNormal(InPhiWithNormal), MBoundingBox(InBoundingBox), MFindClosestIntersection(InFindClosestIntersection), MSupport(InSupport), bIsConvex(false), MType(Type), MDerivedThis(DerivedThis)
{
}

template<class T, int d>
TImplicitObject<T, d>::~TImplicitObject()
{
}

template<class T, int d>
TVector<T, d> TImplicitObject<T, d>::Support(const TVector<T, d>& Direction) const
{
	if (MSupport)
		return MSupport(Direction);
	check(MBoundingBox);
	TVector<T, d> EndPoint = MBoundingBox().Center();
	TVector<T, d> StartPoint = EndPoint + Direction * MBoundingBox().Extents().Max();
	checkSlow(SignedDistance(StartPoint) > 0);
	checkSlow(SignedDistance(EndPoint) < 0);
	// @todo(mlentine): The termination condition is slightly different here so we can probably optimize by reimplementing for this function.
	const auto& Intersection = FindClosestIntersection(StartPoint, EndPoint, 0);
	check(Intersection.Second);
	return Intersection.First;
}

template<class T, int d>
Pair<TVector<T, d>, bool> TImplicitObject<T, d>::FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const
{
	T Epsilon = 1e-4;
	TVector<T, d> Normal;
	T Phi = PhiWithNormal(StartPoint, Normal);
	TVector<T, d> ModifiedStart = (Phi < (Thickness + Epsilon)) ? TVector<T, d>(StartPoint + Normal * (-Phi + Thickness + Epsilon)) : StartPoint;
	if (MFindClosestIntersection)
		return MFindClosestIntersection(ModifiedStart, EndPoint, Thickness);
	TVector<T, d> Ray = EndPoint - ModifiedStart;
	T Length = Ray.Size();
	TVector<T, d> Direction = Ray.GetSafeNormal();
	TVector<T, d> EndNormal;
	T EndPhi = PhiWithNormal(EndPoint, EndNormal);
	TVector<T, d> ClosestPoint = ModifiedStart;
	Phi = PhiWithNormal(ClosestPoint, Normal);
	while (Phi > Thickness)
	{
		ClosestPoint += Direction * (Phi - Thickness);
		if ((ClosestPoint - StartPoint).Size() > Length)
		{
			if (EndPhi < Thickness)
			{
				return MakePair(TVector<T, d>(EndPoint + EndNormal * (-EndPhi + Thickness)), true);
			}
			return MakePair(TVector<T, d>(0), false);
		}
		// If the Change is too small we want to nudge it forward. This makes it possible to miss intersections very close to the surface but is more efficient and shouldn't matter much.
		if ((Phi - Thickness) < 1e-2)
		{
			ClosestPoint += Direction * 1e-2;
			if ((ClosestPoint - StartPoint).Size() > Length)
			{
				if (EndPhi < Thickness)
				{
					return MakePair(TVector<T, d>(EndPoint + EndNormal * (-EndPhi + Thickness)), true);
				}
				else
				{
					return MakePair(TVector<T, d>(0), false);
				}
			}
		}
		T NewPhi = PhiWithNormal(ClosestPoint, Normal);
		if (NewPhi >= Phi)
		{
			if (EndPhi < Thickness)
			{
				return MakePair(TVector<T, d>(EndPoint + EndNormal * (-EndPhi + Thickness)), true);
			}
			return MakePair(TVector<T, d>(0), false);
		}
		Phi = NewPhi;
	}
	if (Phi < Thickness)
	{
		ClosestPoint += Normal * (-Phi + Thickness);
	}
	return MakePair(ClosestPoint, true);
}

template class Apeiron::TImplicitObject<float, 3>;
