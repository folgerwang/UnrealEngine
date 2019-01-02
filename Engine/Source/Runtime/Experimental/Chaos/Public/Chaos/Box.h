// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ImplicitObject.h"
#include "Chaos/Plane.h"
#include "Chaos/Transform.h"

#include <algorithm>
#include <utility>

namespace Chaos
{
template <typename T, int d>
struct TBoxSpecializeSamplingHelper
{
	static FORCEINLINE TArray<TVector<T, d>> ComputeLocalSamplePoints(const class TBox<T, d>& Box)
	{
		check(false);
		return TArray<TVector<T, d>>();
	}
};

template<class T, int d>
class TBox final : public TImplicitObject<T, d>
{
  public:
	using TImplicitObject<T, d>::SignedDistance;

	// This should never be used outside of creating a default for arrays
	FORCEINLINE TBox() = default;
	FORCEINLINE TBox(const TVector<T, d>& Min, const TVector<T, d>& Max)
	    : TImplicitObject<T, d>(EImplicitObject::FiniteConvex, ImplicitObjectType::Box)
	    , MMin(Min)
	    , MMax(Max)
	{
	}

	FORCEINLINE TBox(const TBox<T, d>& Other)
	    : TImplicitObject<T, d>(EImplicitObject::FiniteConvex, ImplicitObjectType::Box)
	    , MMin(Other.MMin)
	    , MMax(Other.MMax)
	{
	}

	FORCEINLINE TBox(TBox<T, d>&& Other)
	    : TImplicitObject<T, d>(EImplicitObject::FiniteConvex, ImplicitObjectType::Box)
	    , MMin(MoveTemp(Other.MMin))
	    , MMax(MoveTemp(Other.MMax))
	{
	}

	FORCEINLINE TBox<T, d>& operator=(const TBox<T, d>& Other)
	{
		MMin = Other.MMin;
		MMax = Other.MMax;
		return *this;
	}

	FORCEINLINE TBox<T, d>& operator=(TBox<T, d>&& Other)
	{
		MMin = MoveTemp(Other.MMin);
		MMax = MoveTemp(Other.MMax);
		return *this;
	}
	virtual ~TBox() {}


	TArray<TVector<T, d>> ComputeLocalSamplePoints() const
	{
		return TBoxSpecializeSamplingHelper<T, d>::ComputeLocalSamplePoints(*this);
	};

	template<class TTRANSFORM>
	TBox<T, d> TransformedBox(const TTRANSFORM& SpaceTransform) const
	{
		TVector<T, d> CurrentExtents = Extents();
		int32 Idx = 0;
		const TVector<T,d> MinToNewSpace = SpaceTransform.TransformPosition(MMin);
		TBox<T, d> NewBox(MinToNewSpace, MinToNewSpace);
		NewBox.GrowToInclude(SpaceTransform.TransformPosition(MMax));

		for (int32 j = 0; j < d; ++j)
		{
			NewBox.GrowToInclude(SpaceTransform.TransformPosition(MMin + TVector<T, d>::AxisVector(j) * CurrentExtents));
			NewBox.GrowToInclude(SpaceTransform.TransformPosition(MMax - TVector<T, d>::AxisVector(j) * CurrentExtents));
		}

		return NewBox;
	}

	FORCEINLINE bool Intersects(const TBox<T, d>& Other) const
	{
		for (int32 i = 0; i < d; ++i)
		{
			if (Other.MMax[i] < MMin[i] || Other.MMin[i] > MMax[i])
				return false;
		}
		return true;
	}

	FORCEINLINE static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::Box;
	}

	const TBox<T, d>& BoundingBox() const { return *this; }

	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override
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
				{
					Normal[i] = MaxDists[i];
				}
				else if (MinDists[i] > 0)
				{
					Normal[i] = -MinDists[i];
				}
				else
				{
					Normal[i] = 0;
				}
			}
			T Phi = Normal.Size();
			if (Phi < KINDA_SMALL_NUMBER)
			{
				for (int i = 0; i < d; ++i)
				{
					if (Normal[i] > 0)
					{
						Normal[i] = 1;
					}
					else if (Normal[i] < 0)
					{
						Normal[i] = -1;
					}
				}
			}
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

	virtual Pair<TVector<T, d>, bool> FindClosestIntersectionImp(const TVector<T, d>& StartPoint, const TVector<T, d>& EndPoint, const T Thickness) const override
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

	virtual TVector<T, d> Support(const TVector<T, d>& Direction, const T Thickness) const override
	{
		if (Direction.SizeSquared() < KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
		{
			return Center();
		}
		const TVector<T, d> HalfExtents = (T)0.5 * Extents();
		const TVector<T, d> UnitDirection = Direction.GetSafeNormal();
		T MinDistance = FLT_MAX;
		for (int32 i = 0; i < d; ++i)
		{
			if (FMath::Abs(Direction[i]) < SMALL_NUMBER)
			{
				continue;
			}
			const T ProjectedDirSize = TVector<T, d>::DotProduct(UnitDirection, Direction[i] > 0 ? TVector<T, d>::AxisVector(i) : -TVector<T, d>::AxisVector(i));
			check(ProjectedDirSize >= 0);
			if (ProjectedDirSize < SMALL_NUMBER)
			{
				continue;
			}
			const T Distance = ((HalfExtents[i] + Thickness) / ProjectedDirSize);
			check(Distance >= 0);
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
			}
		}
		check(MinDistance < FLT_MAX);
		return Center() + MinDistance * UnitDirection;;
	}

	FORCEINLINE void GrowToInclude(const TVector<T, d>& V)
	{
		MMin = TVector<T, d>(FGenericPlatformMath::Min(MMin[0], V[0]), FGenericPlatformMath::Min(MMin[1], V[1]), FGenericPlatformMath::Min(MMin[2], V[2]));
		MMax = TVector<T, d>(FGenericPlatformMath::Max(MMax[0], V[0]), FGenericPlatformMath::Max(MMax[1], V[1]), FGenericPlatformMath::Max(MMax[2], V[2]));
	}

	FORCEINLINE void GrowToInclude(const TBox<T, d>& Other)
	{
		MMin = TVector<T, d>(FGenericPlatformMath::Min(MMin[0], Other.MMin[0]), FGenericPlatformMath::Min(MMin[1], Other.MMin[1]), FGenericPlatformMath::Min(MMin[2], Other.MMin[2]));
		MMax = TVector<T, d>(FGenericPlatformMath::Max(MMax[0], Other.MMax[0]), FGenericPlatformMath::Max(MMax[1], Other.MMax[1]), FGenericPlatformMath::Max(MMax[2], Other.MMax[2]));
	}

	FORCEINLINE void ShrinkToInclude(const TBox<T, d>& Other)
	{
		MMin = TVector<T, d>(FGenericPlatformMath::Max(MMin[0], Other.MMin[0]), FGenericPlatformMath::Max(MMin[1], Other.MMin[1]), FGenericPlatformMath::Max(MMin[2], Other.MMin[2]));
		MMax = TVector<T, d>(FGenericPlatformMath::Min(MMax[0], Other.MMax[0]), FGenericPlatformMath::Min(MMax[1], Other.MMax[1]), FGenericPlatformMath::Min(MMax[2], Other.MMax[2]));
	}

	FORCEINLINE void Thicken(const float Thickness)
	{
		MMin -= TVector<T, d>(Thickness);
		MMax += TVector<T, d>(Thickness);
	}

	FORCEINLINE void Thicken(const TVector<T, d> Thickness)
	{
		GrowToInclude(MMin + Thickness);
		GrowToInclude(MMax + Thickness);
	}


	FORCEINLINE TVector<T, d> Center() const
	{
		return (MMax - MMin) / (T)2 + MMin;
	}

	FORCEINLINE TVector<T, d> Extents() const
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

	FORCEINLINE const TVector<T, d>& Min() const { return MMin; }
	FORCEINLINE const TVector<T, d>& Max() const { return MMax; }

	virtual FString ToString() const
	{
		return FString::Printf(TEXT("TBox Min:%s, Max:%s"), *Min().ToString(), *Max().ToString());	
	}

private:
	TVector<T, d> MMin, MMax;
};

template <typename T>
struct TBoxSpecializeSamplingHelper<T,2>
{
	static FORCEINLINE TArray<TVector<T, 2>> ComputeLocalSamplePoints(const TBox<T, 2>& Box)
	{
		TVector<T, 2> Min = Box.Min();
		TVector<T, 2> Max = Box.Max();

		TArray<TVector<T, 2>> SamplePoints;
		SamplePoints.SetNum(8);
		//top line (min y)
		SamplePoints[0] = TVector<T, 2>{ Min.X, Min.Y };
		SamplePoints[1] = TVector<T, 2>{ 0.f, Min.Y };
		SamplePoints[2] = TVector<T, 2>{ Max.X, Min.Y };

		//mid line (y=0) (mid point removed because internal)
		SamplePoints[3] = TVector<T, 2>{ Min.X, 0.f };
		SamplePoints[4] = TVector<T, 2>{ Max.X, 0.f };

		//bottom line (max y)
		SamplePoints[5] = TVector<T, 2>{ Min.X, Max.Y };
		SamplePoints[6] = TVector<T, 2>{ 0.f, Max.Y };
		SamplePoints[7] = TVector<T, 2>{ Max.X, Max.Y };

		return SamplePoints;
	}
};

template <typename T>
struct TBoxSpecializeSamplingHelper<T,3>
{
	static FORCEINLINE TArray<TVector<T, 3>> ComputeLocalSamplePoints(const TBox<T, 3>& Box)
	{
		TVector<T, 3> Min = Box.Min();
		TVector<T, 3> Max = Box.Max();

		//todo(ocohen): should order these for best levelset cache traversal
		TArray<TVector<T, 3>>  SamplePoints;
		SamplePoints.SetNum(26);
		{
			//xy plane for Min Z
			SamplePoints[0] = TVector<T, 3>{ Min.X, Min.Y, Min.Z };
			SamplePoints[1] = TVector<T, 3>{ 0.f, Min.Y, Min.Z };
			SamplePoints[2] = TVector<T, 3>{ Max.X, Min.Y, Min.Z };

			SamplePoints[3] = TVector<T, 3>{ Min.X, 0.f, Min.Z };
			SamplePoints[4] = TVector<T, 3>{ 0.f, 0.f, Min.Z };
			SamplePoints[5] = TVector<T, 3>{ Max.X, 0.f, Min.Z };

			SamplePoints[6] = TVector<T, 3>{ Min.X, Max.Y, Min.Z };
			SamplePoints[7] = TVector<T, 3>{ 0.f, Max.Y, Min.Z };
			SamplePoints[8] = TVector<T, 3>{ Max.X, Max.Y, Min.Z };
		}

		{
			//xy plane for z = 0 (skip mid point since inside)
			SamplePoints[9] = TVector<T, 3>{ Min.X, Min.Y, 0.f };
			SamplePoints[10] = TVector<T, 3>{ 0.f, Min.Y, 0.f };
			SamplePoints[11] = TVector<T, 3>{ Max.X, Min.Y, 0.f };

			SamplePoints[12] = TVector<T, 3>{ Min.X, 0.f, 0.f };
			SamplePoints[13] = TVector<T, 3>{ Max.X, 0.f, 0.f };

			SamplePoints[14] = TVector<T, 3>{ Min.X, Max.Y, 0.f };
			SamplePoints[15] = TVector<T, 3>{ 0.f, Max.Y, 0.f };
			SamplePoints[16] = TVector<T, 3>{ Max.X, Max.Y, 0.f };
		}

		{
			//xy plane for Max Z
			SamplePoints[17] = TVector<T, 3>{ Min.X, Min.Y, Max.Z };
			SamplePoints[18] = TVector<T, 3>{ 0.f, Min.Y, Max.Z };
			SamplePoints[19] = TVector<T, 3>{ Max.X, Min.Y, Max.Z };

			SamplePoints[20] = TVector<T, 3>{ Min.X, 0.f, Max.Z };
			SamplePoints[21] = TVector<T, 3>{ 0.f, 0.f, Max.Z };
			SamplePoints[22] = TVector<T, 3>{ Max.X, 0.f, Max.Z };

			SamplePoints[23] = TVector<T, 3>{ Min.X, Max.Y, Max.Z };
			SamplePoints[24] = TVector<T, 3>{ 0.f, Max.Y, Max.Z };
			SamplePoints[25] = TVector<T, 3>{ Max.X, Max.Y, Max.Z };
		}

		return SamplePoints;
	}

};
}
