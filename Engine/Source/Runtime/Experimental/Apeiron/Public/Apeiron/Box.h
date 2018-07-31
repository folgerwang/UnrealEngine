// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ImplicitObject.h"
#include "Apeiron/Plane.h"
#include "Apeiron/Transform.h"

#include <algorithm>
#include <utility>

namespace Apeiron
{
template<class T, int d>
class TBox : public TImplicitObject<T, d>
{
  public:
	using TImplicitObject<T, d>::SignedDistance;

	// This should never be used outside of creating a default for arrays
	TBox()
	    : TImplicitObject<T, d>(nullptr, nullptr, nullptr, nullptr)
	{
	}
	TBox(const TVector<T, d>& Min, const TVector<T, d>& Max)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return *this; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); },
	          [this](const TVector<T, d>& Direction) { return Support(Direction); },
	          ImplicitObjectType::Box, reinterpret_cast<void*>(this))
	    , MMin(Min)
	    , MMax(Max)
	{
		this->bIsConvex = true;
	}
	TBox(const TBox<T, d>& Other)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return *this; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); },
	          [this](const TVector<T, d>& Direction) { return Support(Direction); },
	          ImplicitObjectType::Box, reinterpret_cast<void*>(this))
	    , MMin(Other.MMin)
	    , MMax(Other.MMax)
	{
		this->bIsConvex = true;
	}
	TBox(TBox<T, d>&& Other)
	    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
	          [this]() -> const TBox<T, d>& { return *this; },
	          [this](const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) { return FindClosestIntersection(StartPoint, EndPoint, Thickness); },
	          [this](const TVector<T, d>& Direction) { return Support(Direction); },
	          ImplicitObjectType::Box, reinterpret_cast<void*>(this))
	    , MMin(MoveTemp(Other.MMin))
	    , MMax(MoveTemp(Other.MMax))
	{
		Other.MPhiWithNormal = nullptr;
		Other.MBoundingBox = nullptr;
		Other.MSupport = nullptr;
		Other.MFindClosestIntersection = nullptr;
	}
	TBox<T, d>& operator=(TBox<T, d>&& Other)
	{
		MMin = MoveTemp(Other.MMin);
		MMax = MoveTemp(Other.MMax);
		Other.MPhiWithNormal = nullptr;
		Other.MBoundingBox = nullptr;
		Other.MFindClosestIntersection = nullptr;
		return *this;
	}
	~TBox() {}

	TBox<T, d> TransformedBox(const TRigidTransform<T, d>& SpaceTransform) const
	{
		TArray<TVector<T, d>> Corners;
		TVector<T, d> CurrentExtents = Extents();
		Corners.Add(SpaceTransform.TransformPosition(MMin));
		Corners.Add(SpaceTransform.TransformPosition(MMax));
		for (int32 j = 0; j < d; ++j)
		{
			Corners.Add(SpaceTransform.TransformPosition(MMin + TVector<T, d>::AxisVector(j) * CurrentExtents));
			Corners.Add(SpaceTransform.TransformPosition(MMax - TVector<T, d>::AxisVector(j) * CurrentExtents));
		}
		TBox<T, d> NewBox(Corners[0], Corners[0]);
		for (int32 j = 1; j < Corners.Num(); ++j)
		{
			NewBox.GrowToInclude(Corners[j]);
		}
		return NewBox;
	}

	bool Intersects(const TBox<T, d>& Other) const
	{
		for (int32 i = 0; i < d; ++i)
		{
			if (Other.MMax[i] < MMin[i] || Other.MMin[i] > MMax[i])
				return false;
		}
		return true;
	}

	static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Box;
	}

	T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const
	{
		const TVector<T, d> MaxDists = x - MMax;
		const TVector<T, d> MinDists = MMin - x;
		if (x <= MMax && x >= MMin)
		{
			const Pair<T, int32> MaxAndAxis = TVector<T, d>::MaxAndAxis(MinDists, MaxDists);
			Normal = MaxDists[MaxAndAxis.Second] > MinDists[MaxAndAxis.Second] ? TVector<T, d>::AxisVector(MaxAndAxis.Second) : -TVector<T, d>::AxisVector(MaxAndAxis.Second);
			return MaxAndAxis.First;
		}
		else
		{
			for (int i = 0; i < d; ++i)
			{
				check(MaxDists[i] <= 0 || MinDists[i] <= 0);
				if (MaxDists[i] > 0)
					Normal[i] = MaxDists[i];
				else if (MinDists[i] > 0)
					Normal[i] = -MinDists[i];
				else
					Normal[i] = 0;
			}
			T Phi = Normal.Size();
			Normal.Normalize();
			return Phi;
		}
	}

	TVector<T, d> FindClosestPoint(const TVector<T, d>& StartPoint, const T Thickness = (T)0) const
	{
		TVector<T, d> Result(0);

		// clamp exterior to surface
		bool bIsExterior = false;
		for (int i = 0; i < 3; i++)
		{
			float v = StartPoint[i];
			if (v < MMin[i])
			{
				v = MMin[i];
				bIsExterior = true;
			}
			if (v > MMax[i])
			{
				v = MMax[i];
				bIsExterior = true;
			}
			Result[i] = v;
		}

		if (!bIsExterior)
		{
			TArray<Pair<T, TVector<T, 3>>> Intersections;

			// sum interior direction to surface
			for (int32 i = 0; i < d; ++i)
			{
				auto PlaneIntersection = TPlane<T, d>(MMin - Thickness, -TVector<T, d>::AxisVector(i)).FindClosestPoint(Result, 0);
				Intersections.Add(MakePair((PlaneIntersection - Result).Size(), -TVector<T, d>::AxisVector(i)));
				PlaneIntersection = TPlane<T, d>(MMax + Thickness, TVector<T, d>::AxisVector(i)).FindClosestPoint(Result, 0);
				Intersections.Add(MakePair((PlaneIntersection - Result).Size(), TVector<T, d>::AxisVector(i)));
			}
			Intersections.Sort([](const Pair<T, TVector<T, 3>>& Elem1, const Pair<T, TVector<T, 3>>& Elem2) { return Elem1.First < Elem2.First; });

			if (!FMath::IsNearlyEqual(Intersections[0].First, 0.f))
			{
				T SmallestDistance = Intersections[0].First;
				Result += Intersections[0].Second * Intersections[0].First;
				for (int32 i = 1; i < 3 && FMath::IsNearlyEqual(SmallestDistance, Intersections[i].First); ++i)
				{
					Result += Intersections[i].Second * Intersections[i].First;
				}
			}
		}
		return Result;
	}

	Pair<TVector<T, d>, bool> FindClosestIntersection(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const
	{
		TArray<Pair<T, TVector<T, 3>>> Intersections;
		for (int32 i = 0; i < d; ++i)
		{
			auto PlaneIntersection = TPlane<T, d>(MMin - Thickness, -TVector<T, d>::AxisVector(i)).FindClosestIntersection(StartPoint, EndPoint, 0);
			if (PlaneIntersection.Second)
				Intersections.Add(MakePair((PlaneIntersection.First - StartPoint).Size(), PlaneIntersection.First));
			PlaneIntersection = TPlane<T, d>(MMax + Thickness, TVector<T, d>::AxisVector(i)).FindClosestIntersection(StartPoint, EndPoint, 0);
			if (PlaneIntersection.Second)
				Intersections.Add(MakePair((PlaneIntersection.First - StartPoint).Size(), PlaneIntersection.First));
		}
		Intersections.Sort([](const Pair<T, TVector<T, 3>>& Elem1, const Pair<T, TVector<T, 3>>& Elem2) { return Elem1.First < Elem2.First; });
		for (const auto& Elem : Intersections)
		{
			if (SignedDistance(Elem.Second) < (Thickness + 1e-4))
			{
				return MakePair(Elem.Second, true);
			}
		}
		return MakePair(TVector<T, 3>(0), false);
	}

	TVector<T, d> Support(const TVector<T, d>& Direction)
	{
		TVector<T, d> MaxPoint;
		for (int32 i = 0; i < d; ++i)
		{
			if (FPlatformMath::Abs(Direction[i]) < SMALL_NUMBER)
			{
				MaxPoint[i] = (T)0.5 * (MMax[i] + MMin[i]);
			}
			else if (Direction[i] > 0)
			{
				MaxPoint[i] = MMax[i];
			}
			else
			{
				MaxPoint[i] = MMin[i];
			}
		}
		return MaxPoint;
	}

	void GrowToInclude(const TVector<T, d>& V)
	{
		MMin = TVector<T, d>(FGenericPlatformMath::Min(MMin[0], V[0]), FGenericPlatformMath::Min(MMin[1], V[1]), FGenericPlatformMath::Min(MMin[2], V[2]));
		MMax = TVector<T, d>(FGenericPlatformMath::Max(MMax[0], V[0]), FGenericPlatformMath::Max(MMax[1], V[1]), FGenericPlatformMath::Max(MMax[2], V[2]));
	}

	void GrowToInclude(const TBox<T, d>& Other)
	{
		MMin = TVector<T, d>(FGenericPlatformMath::Min(MMin[0], Other.MMin[0]), FGenericPlatformMath::Min(MMin[1], Other.MMin[1]), FGenericPlatformMath::Min(MMin[2], Other.MMin[2]));
		MMax = TVector<T, d>(FGenericPlatformMath::Max(MMax[0], Other.MMax[0]), FGenericPlatformMath::Max(MMax[1], Other.MMax[1]), FGenericPlatformMath::Max(MMax[2], Other.MMax[2]));
	}

	void ShrinkToInclude(const TBox<T, d>& Other)
	{
		MMin = TVector<T, d>(FGenericPlatformMath::Max(MMin[0], Other.MMin[0]), FGenericPlatformMath::Max(MMin[1], Other.MMin[1]), FGenericPlatformMath::Max(MMin[2], Other.MMin[2]));
		MMax = TVector<T, d>(FGenericPlatformMath::Min(MMax[0], Other.MMax[0]), FGenericPlatformMath::Min(MMax[1], Other.MMax[1]), FGenericPlatformMath::Min(MMax[2], Other.MMax[2]));
	}

	void Thicken(const float Thickness)
	{
		MMin -= TVector<T, d>(Thickness);
		MMax += TVector<T, d>(Thickness);
	}

	TVector<T, d> Center() const
	{
		return (MMax - MMin) / (T)2 + MMin;
	}

	TVector<T, d> Extents() const
	{
		return MMax - MMin;
	}

	int LargestAxis() const
	{
		const auto Extents = this->Extents();
		if (Extents[0] > Extents[1] && Extents[0] > Extents[2])
		{
			return 0;
		}
		else if (Extents[1] > Extents[2])
		{
			return 1;
		}
		else
		{
			return 2;
		}
	}

	const TVector<T, d>& Min() const { return MMin; }
	const TVector<T, d>& Max() const { return MMax; }

  private:
	TVector<T, d> MMin, MMax;
};
}
