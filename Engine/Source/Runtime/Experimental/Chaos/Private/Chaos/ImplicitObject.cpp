// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/ImplicitObject.h"
#include "Chaos/Box.h"
#include "HAL/IConsoleManager.h"

using namespace Chaos;

template<class T, int d>
TImplicitObject<T, d>::TImplicitObject(int32 Flags, ImplicitObjectType InType)
	: Type(InType)
	, bIsConvex(!!(Flags & EImplicitObject::IsConvex))
	, bIgnoreAnalyticCollisions(!!(Flags & EImplicitObject::IgnoreAnalyticCollisions))
	, bHasBoundingBox(!!(Flags & EImplicitObject::HasBoundingBox))
{
}

template<class T, int d>
TImplicitObject<T, d>::~TImplicitObject()
{
}

template<class T, int d>
TVector<T, d> TImplicitObject<T, d>::Support(const TVector<T, d>& Direction, const T Thickness) const
{
	check(bHasBoundingBox);
	const TBox<T, d> Box = BoundingBox();
	TVector<T, d> EndPoint = Box.Center();
	TVector<T, d> StartPoint = EndPoint + Direction.GetSafeNormal() * (Box.Extents().Max() + Thickness);
	checkSlow(SignedDistance(StartPoint) > 0);
	checkSlow(SignedDistance(EndPoint) < 0);
	// @todo(mlentine): The termination condition is slightly different here so we can probably optimize by reimplementing for this function.
	const auto& Intersection = FindClosestIntersection(StartPoint, EndPoint, Thickness);
	check(Intersection.Second);
	return Intersection.First;
}

template <typename T, int d>
const TBox<T, d>& TImplicitObject<T, d>::BoundingBox() const
{
	check(false);
	static const TBox<T, d> Unbounded(TVector<T, d>(-FLT_MAX), TVector<T, d>(FLT_MAX));
	return Unbounded;
}

template<class T, int d>
Pair<TVector<T, d>, bool> TImplicitObject<T, d>::FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const
{
	T Epsilon = (T)1e-4;
	//Consider 0 thickness with Start sitting on abs(Phi) < Epsilon. This is a common case; for example a particle sitting perfectly on a floor. In this case intersection could return false.
	//If start is in this fuzzy region we simply return that spot snapped onto the surface. This is valid because low precision means we don't really know where we are, so let's take the cheapest option
	//If end is in this fuzzy region it is also a valid hit. However, there could be multiple hits between start and end and since we want the first one, we can't simply return this point.
	//As such we move end away from start (and out of the fuzzy region) so that we always get a valid intersection if no earlier ones exist
	//When Thickness > 0 the same idea applies, but we must consider Phi = (Thickness - Epsilon, Thickness + Epsilon)
	TVector<T, d> Normal;
	T Phi = PhiWithNormal(StartPoint, Normal);

	if (FMath::IsNearlyEqual(Phi, Thickness, Epsilon))
	{
		return MakePair(StartPoint - Normal * Phi, true);	//snap to surface
	}

	TVector<T, d> ModifiedEnd = EndPoint;
	{
		const TVector<T, d> OriginalStartToEnd = (EndPoint - StartPoint);
		const T OriginalLength = OriginalStartToEnd.Size();
		if (OriginalLength < Epsilon)
		{
			return MakePair(TVector<T, d>(0), false);	//start was not close to surface, and end is very close to start so no hit
		}
		const TVector<T, d> OriginalDir = OriginalStartToEnd / OriginalLength;

		TVector<T, d> EndNormal;
		T EndPhi = PhiWithNormal(EndPoint, EndNormal);
		if (FMath::IsNearlyEqual(EndPhi, Thickness, Epsilon))
		{
			//We want to push End out of the fuzzy region. Moving along the normal direction is best since direction could be nearly parallel with fuzzy band
			//To ensure an intersection, we must go along the normal, but in the same general direction as the ray.
			const T Dot = TVector<T, d>::DotProduct(OriginalDir, EndNormal);
			if (FMath::IsNearlyZero(Dot, Epsilon))
			{
				//End is in the fuzzy region, and the direction from start to end is nearly parallel with this fuzzy band, so we should just return End since no other hits will occur
				return MakePair(EndPoint - Normal * Phi, true);	//snap to surface
			}
			else
			{
				ModifiedEnd = EndPoint + 2.f * Epsilon * FMath::Sign(Dot) * EndNormal;	//get out of fuzzy region
			}
		}
	}

	return FindClosestIntersectionImp(StartPoint, ModifiedEnd, Thickness);
}

float ClosestIntersectionStepSizeMultiplier = 0.5f;
FAutoConsoleVariableRef CVarClosestIntersectionStepSizeMultiplier(TEXT("p.ClosestIntersectionStepSizeMultiplier"), ClosestIntersectionStepSizeMultiplier, TEXT("When raycasting we use this multiplier to substep the travel distance along the ray. Smaller number gives better accuracy at higher cost"));

template<class T, int d>
Pair<TVector<T, d>, bool> TImplicitObject<T, d>::FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const
{
	T Epsilon = (T)1e-4;

	TVector<T, d> Ray = EndPoint - StartPoint;
	T Length = Ray.Size();
	TVector<T, d> Direction = Ray.GetUnsafeNormal();	//this is safe because StartPoint and EndPoint were already tested to be far enough away. In the case where ModifiedEnd is pushed, we push it along the direction so it can only get farther
	TVector<T, d> EndNormal;
	const T EndPhi = PhiWithNormal(EndPoint, EndNormal);
	TVector<T, d> ClosestPoint = StartPoint;

	TVector<T, d> Normal;
	T Phi = PhiWithNormal(ClosestPoint, Normal);

	while (Phi > Thickness + Epsilon)
	{
		ClosestPoint += Direction * (Phi - Thickness) * (T)ClosestIntersectionStepSizeMultiplier;
		if ((ClosestPoint - StartPoint).Size() > Length)
		{
			if (EndPhi < Thickness + Epsilon)
			{
				return MakePair(TVector<T, d>(EndPoint + EndNormal * (-EndPhi + Thickness)), true);
			}
			return MakePair(TVector<T, d>(0), false);
		}
		// If the Change is too small we want to nudge it forward. This makes it possible to miss intersections very close to the surface but is more efficient and shouldn't matter much.
		if ((Phi - Thickness) < (T)1e-2)
		{
			ClosestPoint += Direction * (T)1e-2;
			if ((ClosestPoint - StartPoint).Size() > Length)
			{
				if (EndPhi < Thickness + Epsilon)
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
			if (EndPhi < Thickness + Epsilon)
			{
				return MakePair(TVector<T, d>(EndPoint + EndNormal * (-EndPhi + Thickness)), true);
			}
			return MakePair(TVector<T, d>(0), false);
		}
		Phi = NewPhi;
	}
	if (Phi < Thickness + Epsilon)
	{
		ClosestPoint += Normal * (-Phi + Thickness);
	}
	return MakePair(ClosestPoint, true);
}

template<typename T,int d>
void TImplicitObject<T,d>::FindAllIntersectingObjects(TArray < Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>>& Out, const TBox<T, d>& LocalBounds) const
{
	if (!HasBoundingBox() || LocalBounds.Intersects(BoundingBox()))
	{
		Out.Add(MakePair(this, TRigidTransform<T, d>(TVector<T,d>(0), TRotation<T,d>(TVector<T,d>(0), (T)1))));
	}
}

template class Chaos::TImplicitObject<float, 3>;
