// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/Levelset.h"
#include "Containers/Queue.h"

#include <algorithm>
#include <vector>

using namespace Chaos;

#define MAX_CLAMP(a, comp, b) (a >= comp ? b : a)
#define MIN_CLAMP(a, comp, b) (a < comp ? b : a)
#define RANGE_CLAMP(a, comp, b) ((a < 0 || comp <= a) ? b : a)

template<class T, int d>
TLevelSet<T, d>::TLevelSet(const TUniformGrid<T, d>& InGrid, const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 BandWidth)
    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::LevelSet)
    , MGrid(InGrid)
    , MPhi(MGrid)
    , MNormals(MGrid)
    , MLocalBoundingBox(MGrid.MinCorner(), MGrid.MaxCorner())
    , MBandWidth(BandWidth)
{
	check(d == 3);
	check(MGrid.Counts()[0] > 1 && MGrid.Counts()[1] > 1 && MGrid.Counts()[2] > 1);
	check(Mesh.GetSurfaceElements().Num());

	TArrayND<bool, d> BlockedFaceX(MGrid.Counts());
	TArrayND<bool, d> BlockedFaceY(MGrid.Counts());
	TArrayND<bool, d> BlockedFaceZ(MGrid.Counts());
	TArray<TVector<int32, d>> InterfaceIndices;
	ComputeDistancesNearZeroIsocontour(InParticles, Mesh, BlockedFaceX, BlockedFaceY, BlockedFaceZ, InterfaceIndices);
	T StoppingDistance = MBandWidth * MGrid.Dx().Max();
	if (StoppingDistance)
	{
		for (int32 i = 0; i < MGrid.Counts().Product(); ++i)
		{
			MPhi[i] = FGenericPlatformMath::Min(MPhi[i], StoppingDistance);
		}
	}
	CorrectSign(BlockedFaceX, BlockedFaceY, BlockedFaceZ, InterfaceIndices);
	FillWithFastMarchingMethod(StoppingDistance, InterfaceIndices);
	if (StoppingDistance)
	{
		for (int32 i = 0; i < MGrid.Counts().Product(); ++i)
		{
			if (FGenericPlatformMath::Abs(MPhi[i]) > StoppingDistance)
			{
				MPhi[i] = MPhi[i] > 0 ? StoppingDistance : -StoppingDistance;
			}
		}
	}
	ComputeNormals();
	ComputeConvexity(InterfaceIndices);
}

template<class T, int d>
TLevelSet<T, d>::TLevelSet(const TUniformGrid<T, d>& InGrid, const TImplicitObject<T, d>& InObject, const int32 BandWidth, const bool bUseObjectPhi)
    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::LevelSet)
    , MGrid(InGrid)
    , MPhi(MGrid)
    , MNormals(MGrid)
    , MLocalBoundingBox(MGrid.MinCorner(), MGrid.MaxCorner())
    , MOriginalLocalBoundingBox(InObject.BoundingBox())
    , MBandWidth(BandWidth)
{
	check(d == 3);
	check(MGrid.Counts()[0] > 1 && MGrid.Counts()[1] > 1 && MGrid.Counts()[2] > 1);
	const auto& Counts = MGrid.Counts();
	if (bUseObjectPhi)
	{
		for (int32 i = 0; i < Counts.Product(); ++i)
		{
			MPhi[i] = InObject.SignedDistance(MGrid.Center(i));
		}
		ComputeNormals();
		return;
	}
	TArrayND<T, d> ObjectPhi(MGrid);
	for (int32 i = 0; i < Counts.Product(); ++i)
	{
		ObjectPhi[i] = InObject.SignedDistance(MGrid.Center(i));
	}
	TArray<TVector<int32, d>> InterfaceIndices;
	ComputeDistancesNearZeroIsocontour(InObject, ObjectPhi, InterfaceIndices);
	T StoppingDistance = MBandWidth * MGrid.Dx().Max();
	if (StoppingDistance)
	{
		for (int32 i = 0; i < MGrid.Counts().Product(); ++i)
		{
			MPhi[i] = FGenericPlatformMath::Min(MPhi[i], StoppingDistance);
		}
	}
	// Correct Sign
	for (int32 i = 0; i < MGrid.Counts().Product(); ++i)
	{
		MPhi[i] *= FMath::Sign(ObjectPhi[i]);
	}
	FillWithFastMarchingMethod(StoppingDistance, InterfaceIndices);
	if (StoppingDistance)
	{
		for (int32 i = 0; i < MGrid.Counts().Product(); ++i)
		{
			if (FGenericPlatformMath::Abs(MPhi[i]) > StoppingDistance)
			{
				MPhi[i] = MPhi[i] > 0 ? StoppingDistance : -StoppingDistance;
			}
		}
	}
	ComputeNormals();
	ComputeConvexity(InterfaceIndices);
}

template<class T, int d>
TLevelSet<T, d>::TLevelSet(std::istream& Stream)
    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::LevelSet)
    , MGrid(Stream)
    , MPhi(Stream)
    , MLocalBoundingBox(MGrid.MinCorner(), MGrid.MaxCorner())
{
	Stream.read(reinterpret_cast<char*>(&MBandWidth), sizeof(MBandWidth));
	ComputeNormals();
}

template<class T, int d>
TLevelSet<T, d>::TLevelSet(TLevelSet<T, d>&& Other)
    : TImplicitObject<T, d>(EImplicitObject::HasBoundingBox, ImplicitObjectType::LevelSet)
    , MGrid(MoveTemp(Other.MGrid))
    , MPhi(MoveTemp(Other.MPhi))
    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
    , MBandWidth(Other.MBandWidth)
{
}

template<class T, int d>
TLevelSet<T, d>::~TLevelSet()
{
}

template<class T, int d>
void TLevelSet<T, d>::ComputeConvexity(const TArray<TVector<int32, d>>& InterfaceIndices)
{
	this->bIsConvex = true;
	int32 Sign = 1;
	bool bFirst = true;
	int ZOffset = MGrid.Counts()[2];
	int YZOffset = MGrid.Counts()[1] * ZOffset;
	const int32 NumCells = MGrid.Counts().Product();
	for (const auto& Index : InterfaceIndices)
	{
		const int32 i = (Index.X * MGrid.Counts()[1] + Index.Y) * MGrid.Counts()[2] + Index.Z;
		if (MPhi[i] > 0)
		{
			continue;
		}
		int32 LocalSign;
		T PhiX = (MPhi[MAX_CLAMP(i + YZOffset, NumCells, i)] - MPhi[MIN_CLAMP(i - YZOffset, 0, i)]) / (2 * MGrid.Dx()[0]);
		T PhiXX = (MPhi[MIN_CLAMP(i - YZOffset, 0, i)] + MPhi[MAX_CLAMP(i + YZOffset, NumCells, i)] - 2 * MPhi[i]) / (MGrid.Dx()[0] * MGrid.Dx()[0]);
		T PhiY = (MPhi[MAX_CLAMP(i + ZOffset, NumCells, i)] - MPhi[MIN_CLAMP(i - ZOffset, 0, i)]) / (2 * MGrid.Dx()[1]);
		T PhiYY = (MPhi[MIN_CLAMP(i - ZOffset, 0, i)] + MPhi[MAX_CLAMP(i + ZOffset, NumCells, i)] - 2 * MPhi[i]) / (MGrid.Dx()[1] * MGrid.Dx()[1]);
		T PhiZ = (MPhi[MAX_CLAMP(i + 1, NumCells, i)] - MPhi[MIN_CLAMP(i - 1, 0, i)]) / (2 * MGrid.Dx()[2]);
		T PhiZZ = (MPhi[MIN_CLAMP(i - 1, 0, i)] + MPhi[MAX_CLAMP(i + 1, NumCells, i)] - 2 * MPhi[i]) / (MGrid.Dx()[2] * MGrid.Dx()[2]);
		T PhiXY = (MPhi[MAX_CLAMP(i + YZOffset + ZOffset, NumCells, i)] + MPhi[MIN_CLAMP(i - YZOffset - ZOffset, 0, i)] - MPhi[RANGE_CLAMP(i - YZOffset + ZOffset, NumCells, i)] - MPhi[RANGE_CLAMP(i + YZOffset - ZOffset, NumCells, i)]) / (4 * MGrid.Dx()[0] * MGrid.Dx()[1]);
		T PhiXZ = (MPhi[MAX_CLAMP(i + YZOffset + 1, NumCells, i)] + MPhi[MIN_CLAMP(i - YZOffset - 1, 0, i)] - MPhi[RANGE_CLAMP(i - YZOffset + 1, NumCells, i)] - MPhi[RANGE_CLAMP(i + YZOffset - 1, NumCells, i)]) / (4 * MGrid.Dx()[0] * MGrid.Dx()[2]);
		T PhiYZ = (MPhi[MAX_CLAMP(i + ZOffset + 1, NumCells, i)] + MPhi[MIN_CLAMP(i - ZOffset - 1, 0, i)] - MPhi[RANGE_CLAMP(i - ZOffset + 1, NumCells, i)] - MPhi[RANGE_CLAMP(i + ZOffset - 1, NumCells, i)]) / (4 * MGrid.Dx()[1] * MGrid.Dx()[2]);

		T Denom = sqrt(PhiX * PhiX + PhiY * PhiY + PhiZ * PhiZ);
		if (Denom > SMALL_NUMBER)
		{
			T curvature = -(PhiX * PhiX * PhiYY - 2 * PhiX * PhiY * PhiXY + PhiY * PhiY * PhiXX + PhiX * PhiX * PhiZZ - 2 * PhiX * PhiZ * PhiXZ + PhiZ * PhiZ * PhiXX + PhiY * PhiY * PhiZZ - 2 * PhiY * PhiZ * PhiYZ + PhiZ * PhiZ * PhiYY) / (Denom * Denom * Denom);
			LocalSign = curvature > KINDA_SMALL_NUMBER ? 1 : (curvature < -KINDA_SMALL_NUMBER ? -1 : 0);
			if (bFirst)
			{
				bFirst = false;
				Sign = LocalSign;
			}
			else
			{
				if (LocalSign != 0 && Sign != LocalSign)
				{
					this->bIsConvex = false;
					return;
				}
			}
		}
	}
}

template <typename T, int d>
TVector<T, 2> ComputeBarycentricInPlane(const TVector<T, d>& P0, const TVector<T, d>& P1, const TVector<T, d>& P2, const TVector<T, d>& P)
{
	TVector<T, 2> Bary;
	TVector<T, d> P10 = P1 - P0;
	TVector<T, d> P20 = P2 - P0;
	TVector<T, d> PP0 = P - P0;
	T Size10 = P10.SizeSquared();
	T Size20 = P20.SizeSquared();
	T ProjSides = TVector<T, d>::DotProduct(P10, P20);
	T ProjP1 = TVector<T, d>::DotProduct(PP0, P10);
	T ProjP2 = TVector<T, d>::DotProduct(PP0, P20);
	T Denom = Size10 * Size20 - ProjSides * ProjSides;
	Bary.X = (Size20 * ProjP1 - ProjSides * ProjP2) / Denom;
	Bary.Y = (Size10 * ProjP2 - ProjSides * ProjP1) / Denom;
	return Bary;
}

template <typename T, int d>
const TVector<T,d> FindClosestPointOnLineSegment(const TVector<T, d>& P0, const TVector<T, d>& P1, const TVector<T,d>& P)
{
	const TVector<T, d> P10 = P1 - P0;
	const TVector<T, d> PP0 = P - P0;
	const T Proj = TVector<T, d>::DotProduct(P10, PP0);
	if (Proj < (T)0)	//first check we're not behind
	{
		return P0;
	}

	const T Denom2 = P10.SizeSquared();
	if (Denom2 < (T)1e-4)
	{
		return P0;
	}

	//do proper projection
	const T NormalProj = Proj / Denom2;
	if (NormalProj > (T)1)	//too far forward
	{
		return P1;
	}

	return P0 + NormalProj * P10;	//somewhere on the line
}

template <typename T, int d>
TVector<T, d> FindClosestPointOnTriangle(const TPlane<T,d>& TrianglePlane, const TVector<T, d>& P0, const TVector<T, d>& P1, const TVector<T, d>& P2, const TVector<T, d>& P)
{
	const T Epsilon = 1e-4;
	const TVector<T, d> PointOnPlane = TrianglePlane.FindClosestPoint(P);
	const TVector<T, 2> Bary = ComputeBarycentricInPlane(P0, P1, P2, PointOnPlane);

	if (Bary[0] >= -Epsilon && Bary[0] <= 1 + Epsilon && Bary[1] >= -Epsilon && Bary[1] <= 1 + Epsilon && (Bary[0] + Bary[1]) <= (1 + Epsilon))
	{
		return PointOnPlane;
	}

	const TVector<T, d> P10Closest = FindClosestPointOnLineSegment(P0, P1, P);
	const TVector<T, d> P20Closest = FindClosestPointOnLineSegment(P0, P2, P);
	const TVector<T, d> P21Closest = FindClosestPointOnLineSegment(P1, P2, P);

	const T P10Dist2 = (P - P10Closest).SizeSquared();
	const T P20Dist2 = (P - P20Closest).SizeSquared();
	const T P21Dist2 = (P - P21Closest).SizeSquared();

	if (P10Dist2 < P20Dist2)
	{
		if (P10Dist2 < P21Dist2)
		{
			return P10Closest;
		}
		else
		{
			return P21Closest;
		}
	}
	else
	{
		if (P20Dist2 < P21Dist2)
		{
			return P20Closest;
		}
		else
		{
			return P21Closest;
		}
	}
}

template<class T, int d>
void TLevelSet<T, d>::ComputeDistancesNearZeroIsocontour(const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, TArrayND<bool, d>& BlockedFaceX, TArrayND<bool, d>& BlockedFaceY, TArrayND<bool, d>& BlockedFaceZ, TArray<TVector<int32, d>>& InterfaceIndices)
{
	MPhi.Fill(FLT_MAX);
	const auto Normals = Mesh.GetFaceNormals(InParticles);
	BlockedFaceX.Fill(false);
	BlockedFaceY.Fill(false);
	BlockedFaceZ.Fill(false);
	const TArray<TVector<int32,3>>& Elements = Mesh.GetSurfaceElements();
	if (Elements.Num() > 0)
	{
		MOriginalLocalBoundingBox = TBox<T, d>(InParticles.X(Elements[0][0]), InParticles.X(Elements[0][0]));
	}
	else
	{
		//just use bounds of grid. This should not happen
		//check(false);
		MOriginalLocalBoundingBox = MLocalBoundingBox;
	}
	for (int32 Index = 0; Index < Elements.Num(); ++Index)
	{
		const auto& Element = Elements[Index];
		TPlane<T, d> TrianglePlane(InParticles.X(Element[0]), Normals[Index]);
		TBox<T, d> TriangleBounds(InParticles.X(Element[0]), InParticles.X(Element[0]));
		TriangleBounds.GrowToInclude(InParticles.X(Element[1]));
		TriangleBounds.GrowToInclude(InParticles.X(Element[2]));
		MOriginalLocalBoundingBox.GrowToInclude(TriangleBounds); //also save the original bounding box

		TVector<int32, d> StartIndex = MGrid.ClampIndex(MGrid.Cell(TriangleBounds.Min() - TVector<T, d>((0.5+KINDA_SMALL_NUMBER) * MGrid.Dx())));
		TVector<int32, d> EndIndex = MGrid.ClampIndex(MGrid.Cell(TriangleBounds.Max() + TVector<T, d>((0.5 + KINDA_SMALL_NUMBER) * MGrid.Dx())));
		for (int32 i = StartIndex[0]; i <= EndIndex[0]; ++i)
		{
			for (int32 j = StartIndex[1]; j <= EndIndex[1]; ++j)
			{
				for (int32 k = StartIndex[2]; k <= EndIndex[2]; ++k)
				{
					const TVector<int32, d> CellIndex(i, j, k);
					const TVector<T,d> Center = MGrid.Location(CellIndex);
					const TVector<T, d> Point = FindClosestPointOnTriangle(TrianglePlane, InParticles.X(Element[0]), InParticles.X(Element[1]), InParticles.X(Element[2]), Center);

					T NewPhi = (Point - Center).Size();
					if (NewPhi < MPhi(CellIndex))
					{
						MPhi(CellIndex) = NewPhi;
						InterfaceIndices.AddUnique(CellIndex);
					}
				}
			}
		}
		for (int32 i = StartIndex[0] + 1; i <= EndIndex[0]; ++i)
		{
			for (int32 j = StartIndex[1] + 1; j <= EndIndex[1]; ++j)
			{
				for (int32 k = StartIndex[2] + 1; k <= EndIndex[2]; ++k)
				{
					const TVector<int32, d> CellIndex(i, j, k);
					if (!BlockedFaceX(CellIndex) && IsIntersectingWithTriangle(InParticles, Element, TrianglePlane, CellIndex, TVector<int32, d>(i - 1, j, k)))
					{
						BlockedFaceX(CellIndex) = true;
					}
					if (!BlockedFaceY(CellIndex) && IsIntersectingWithTriangle(InParticles, Element, TrianglePlane, CellIndex, TVector<int32, d>(i, j - 1, k)))
					{
						BlockedFaceY(CellIndex) = true;
					}
					if (!BlockedFaceZ(CellIndex) && IsIntersectingWithTriangle(InParticles, Element, TrianglePlane, CellIndex, TVector<int32, d>(i, j, k - 1)))
					{
						BlockedFaceZ(CellIndex) = true;
					}
				}
			}
		}
	}
}

template<class T, int d>
void TLevelSet<T, d>::ComputeDistancesNearZeroIsocontour(const TImplicitObject<T, d>& Object, const TArrayND<T, d>& ObjectPhi, TArray<TVector<int32, d>>& InterfaceIndices)
{
	MPhi.Fill(FLT_MAX);
	const auto& Counts = MGrid.Counts();
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			for (int32 k = 0; k < Counts[2]; ++k)
			{
				bool bBoundaryCell = false;
				const TVector<int32, d> CellIndex(i, j, k);
				const TVector<int32, d> CellIndexXM1(i - 1, j, k);
				const TVector<int32, d> CellIndexXP1(i + 1, j, k);
				const TVector<int32, d> CellIndexYM1(i, j - 1, k);
				const TVector<int32, d> CellIndexYP1(i, j + 1, k);
				const TVector<int32, d> CellIndexZM1(i, j, k - 1);
				const TVector<int32, d> CellIndexZP1(i, j, k + 1);
				if (i > 0 && FMath::Sign(ObjectPhi(CellIndex)) != FMath::Sign(ObjectPhi(CellIndexXM1)))
				{
					bBoundaryCell = true;
				}
				if (i < (Counts[0] - 1) && FMath::Sign(ObjectPhi(CellIndex)) != FMath::Sign(ObjectPhi(CellIndexXP1)))
				{
					bBoundaryCell = true;
				}
				if (j > 0 && FMath::Sign(ObjectPhi(CellIndex)) != FMath::Sign(ObjectPhi(CellIndexYM1)))
				{
					bBoundaryCell = true;
				}
				if (j < (Counts[1] - 1) && FMath::Sign(ObjectPhi(CellIndex)) != FMath::Sign(ObjectPhi(CellIndexYP1)))
				{
					bBoundaryCell = true;
				}
				if (k > 0 && FMath::Sign(ObjectPhi(CellIndex)) != FMath::Sign(ObjectPhi(CellIndexZM1)))
				{
					bBoundaryCell = true;
				}
				if (k < (Counts[2] - 1) && FMath::Sign(ObjectPhi(CellIndex)) != FMath::Sign(ObjectPhi(CellIndexZP1)))
				{
					bBoundaryCell = true;
				}
				if (bBoundaryCell)
				{
					MPhi(CellIndex) = FMath::Abs(ObjectPhi(CellIndex));
					InterfaceIndices.Add(CellIndex);
				}
			}
		}
	}
}

template<class T, int d>
void TLevelSet<T, d>::CorrectSign(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArray<TVector<int32,d>>& InterfaceIndices)
{
	int32 NextColor = -1;
	TArrayND<int32, d> Color(MGrid);
	Color.Fill(-1);
	const auto& Counts = MGrid.Counts();
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			for (int32 k = 0; k < Counts[2]; ++k)
			{
				//if we have any isolated holes or single cells near the border mark them with a color
				const TVector<int32, d> CellIndex(i, j, k);
				if ((i == 0 || BlockedFaceX(CellIndex)) && (i == (Counts[0] - 1) || BlockedFaceX(TVector<int32, d>(i + 1, j, k))) &&
				    (j == 0 || BlockedFaceY(CellIndex)) && (j == (Counts[1] - 1) || BlockedFaceY(TVector<int32, d>(i, j + 1, k))) &&
				    (k == 0 || BlockedFaceZ(CellIndex)) && (k == (Counts[2] - 1) || BlockedFaceZ(TVector<int32, d>(i, j, k + 1))))
				{
					Color(CellIndex) = ++NextColor;
				}
			}
		}
	}
	FloodFill(BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color, NextColor);
	TSet<int32> BoundaryColors;
	TArray<int32> ColorIsInside;
	ColorIsInside.SetNum(NextColor + 1);
	for (int32 i = 0; i <= NextColor; ++i)
	{
		ColorIsInside[i] = -1;
	}
	for (int32 j = 0; j < Counts[1]; ++j)
	{
		for (int32 k = 0; k < Counts[2]; ++k)
		{
			int32 LColor = Color(TVector<int32, d>(0, j, k));
			int32 RColor = Color(TVector<int32, d>(Counts[0] - 1, j, k));
			ColorIsInside[LColor] = 0;
			ColorIsInside[RColor] = 0;
			if (!BoundaryColors.Contains(LColor))
			{
				BoundaryColors.Add(LColor);
			}
			if (!BoundaryColors.Contains(RColor))
			{
				BoundaryColors.Add(RColor);
			}
		}
	}
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 k = 0; k < Counts[2]; ++k)
		{
			int32 LColor = Color(TVector<int32, d>(i, 0, k));
			int32 RColor = Color(TVector<int32, d>(i, Counts[1] - 1, k));
			ColorIsInside[LColor] = 0;
			ColorIsInside[RColor] = 0;
			if (!BoundaryColors.Contains(LColor))
			{
				BoundaryColors.Add(LColor);
			}
			if (!BoundaryColors.Contains(RColor))
			{
				BoundaryColors.Add(RColor);
			}
		}
	}
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			int32 LColor = Color(TVector<int32, d>(i, j, 0));
			int32 RColor = Color(TVector<int32, d>(i, j, Counts[2] - 1));
			ColorIsInside[LColor] = 0;
			ColorIsInside[RColor] = 0;
			if (!BoundaryColors.Contains(LColor))
			{
				BoundaryColors.Add(LColor);
			}
			if (!BoundaryColors.Contains(RColor))
			{
				BoundaryColors.Add(RColor);
			}
		}
	}
#if 0
	TMap<int32, TSet<int32>> ColorAdjacency;
	for (int32 i = 0; i <= NextColor; ++i)
	{
		ColorAdjacency.Add(i, TSet<int32>());
	}
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			for (int32 k = 0; k < Counts[2]; ++k)
			{
				const auto CellIndex = TVector<int32, d>(i, j, k);
				int32 SourceColor = Color(CellIndex);
				const auto CellIndexXP1 = CellIndex + TVector<int32, d>::AxisVector(0);
				const auto CellIndexXM1 = CellIndex - TVector<int32, d>::AxisVector(0);
				const auto CellIndexYP1 = CellIndex + TVector<int32, d>::AxisVector(1);
				const auto CellIndexYM1 = CellIndex - TVector<int32, d>::AxisVector(1);
				const auto CellIndexZP1 = CellIndex + TVector<int32, d>::AxisVector(2);
				const auto CellIndexZM1 = CellIndex - TVector<int32, d>::AxisVector(2);
				const int32 ColorXP1 = CellIndexXP1[0] < MGrid.Counts()[0] ? Color(CellIndexXP1) : -1;
				const int32 ColorXM1 = CellIndexXM1[0] >= 0 ? Color(CellIndexXM1) : -1;
				const int32 ColorYP1 = CellIndexYP1[1] < MGrid.Counts()[1] ? Color(CellIndexYP1) : -1;
				const int32 ColorYM1 = CellIndexYM1[1] >= 0 ? Color(CellIndexYM1) : -1;
				const int32 ColorZP1 = CellIndexZP1[2] < MGrid.Counts()[2] ? Color(CellIndexZP1) : -1;
				const int32 ColorZM1 = CellIndexZM1[2] >= 0 ? Color(CellIndexZM1) : -1;
				if (CellIndexZP1[2] < MGrid.Counts()[2] && SourceColor != ColorZP1 && !ColorAdjacency[SourceColor].Contains(ColorZP1))
				{
					ColorAdjacency[SourceColor].Add(ColorZP1);
				}
				if (CellIndexZM1[2] >= 0 && SourceColor != ColorZM1 && !ColorAdjacency[SourceColor].Contains(ColorZM1))
				{
					ColorAdjacency[SourceColor].Add(ColorZM1);
				}
				if (CellIndexYP1[1] < MGrid.Counts()[1] && SourceColor != ColorYP1 && !ColorAdjacency[SourceColor].Contains(ColorYP1))
				{
					ColorAdjacency[SourceColor].Add(ColorYP1);
				}
				if (CellIndexYM1[1] >= 0 && SourceColor != ColorYM1 && !ColorAdjacency[SourceColor].Contains(ColorYM1))
				{
					ColorAdjacency[SourceColor].Add(ColorYM1);
				}
				if (CellIndexXP1[0] < MGrid.Counts()[0] && SourceColor != ColorXP1 && !ColorAdjacency[SourceColor].Contains(ColorXP1))
				{
					ColorAdjacency[SourceColor].Add(ColorXP1);
				}
				if (CellIndexXM1[0] >= 0 && SourceColor != ColorXM1 && !ColorAdjacency[SourceColor].Contains(ColorXM1))
				{
					ColorAdjacency[SourceColor].Add(ColorXM1);
				}
			}
		}
	}
	TQueue<int32> ProcessedColors;
	for (const int32 BoundaryColor : BoundaryColors)
	{
		ProcessedColors.Enqueue(BoundaryColor);
	}
	while (!ProcessedColors.IsEmpty())
	{
		int32 CurrentColor;
		ProcessedColors.Dequeue(CurrentColor);
		for (const int32 AdjacentColor : ColorAdjacency[CurrentColor])
		{
			if (ColorIsInside[AdjacentColor] < 0)
			{
				ColorIsInside[AdjacentColor] = !ColorIsInside[CurrentColor];
				ProcessedColors.Enqueue(AdjacentColor);
			}
			else
			{
				check(ColorIsInside[AdjacentColor] == !ColorIsInside[CurrentColor]);
			}
		}
	}
#endif
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			for (int32 k = 0; k < Counts[2]; ++k)
			{
				const TVector<int32, d> CellIndex(i, j, k);
				if (ColorIsInside[Color(CellIndex)])
				{
					MPhi(CellIndex) *= -1;
				}
			}
		}
	}

	//remove internal cells from interface list
	for (int32 i = InterfaceIndices.Num() - 1; i >= 0; --i)
	{
		const TVector<int32, 3>& CellIndex = InterfaceIndices[i];
		if (!ColorIsInside[Color(CellIndex)])
		{
			continue;	//already an outside color 
		}

		bool bInside = true;
		for (int32 Axis = 0; Axis < d; ++Axis)
		{
			//if any neighbors are outside this is a real interface cell
			const TVector<int32, 3> IndexP1 = CellIndex + TVector<int32, d>::AxisVector(Axis);

			if (IndexP1[Axis] >= MGrid.Counts()[Axis] || !ColorIsInside[Color(IndexP1)])
			{
				bInside = false;
				break;
			}

			const TVector<int32, 3> IndexM1 = CellIndex - TVector<int32, d>::AxisVector(Axis);
			if (IndexM1[Axis] < 0 || !ColorIsInside[Color(IndexM1)])
			{
				bInside = false;
				break;
			}
		}

		if (bInside)
		{
			//fully internal cell so remove it from interface list
			MPhi(CellIndex) = -FLT_MAX;
			InterfaceIndices.RemoveAt(i);
		}
	}
}

template<class T, int d>
bool Compare(const Pair<T*, TVector<int32, d>>& Other1, const Pair<T*, TVector<int32, d>>& Other2)
{
	return FMath::Abs(*Other1.First) < FMath::Abs(*Other2.First);
}

template<class T, int d>
void TLevelSet<T, d>::FillWithFastMarchingMethod(const T StoppingDistance, const TArray<TVector<int32, d>>& InterfaceIndices)
{
	TArrayND<bool, d> Done(MGrid), InHeap(MGrid);
	Done.Fill(false);
	InHeap.Fill(false);
	TArray<Pair<T*, TVector<int32, d>>> Heap;
	// TODO(mlentine): Update phi for these cells
	for (const auto& CellIndex : InterfaceIndices)
	{
		check(!Done(CellIndex) && !InHeap(CellIndex));
		Done(CellIndex) = true;
		Heap.Add(MakePair(&MPhi(CellIndex), CellIndex));
		InHeap(CellIndex) = true;
	}
	Heap.Heapify(Compare<T, d>);
	while (Heap.Num())
	{
		Pair<T*, TVector<int32, d>> Smallest;
		Heap.HeapPop(Smallest, Compare<T, d>);
		check(InHeap(Smallest.Second));
		if (StoppingDistance && FGenericPlatformMath::Abs(*Smallest.First) > StoppingDistance)
		{
			break;
		}
		Done(Smallest.Second) = true;
		InHeap(Smallest.Second) = false;
		for (int32 Axis = 0; Axis < d; ++Axis)
		{
			const auto IP1 = Smallest.Second + TVector<int32, d>::AxisVector(Axis);
			const auto IM1 = Smallest.Second - TVector<int32, d>::AxisVector(Axis);
			if (IM1[Axis] >= 0 && !Done(IM1))
			{
				MPhi(IM1) = ComputePhi(Done, IM1);
				if (!InHeap(IM1))
				{
					Heap.Add(MakePair(&MPhi(IM1), IM1));
					InHeap(IM1) = true;
				}
			}
			if (IP1[Axis] < MGrid.Counts()[Axis] && !Done(IP1))
			{
				MPhi(IP1) = ComputePhi(Done, IP1);
				if (!InHeap(IP1))
				{
					Heap.Add(MakePair(&MPhi(IP1), IP1));
					InHeap(IP1) = true;
				}
			}
		}
		Heap.Heapify(Compare<T, d>);
	}
}

template<class T>
T SolveQuadraticEquation(const T Phi, const T PhiX, const T PhiY, const T Dx, const T Dy)
{
	check(FMath::Sign(PhiX) == FMath::Sign(PhiY) || FMath::Sign(PhiX) == 0 || FMath::Sign(PhiY) == 0);
	T Sign = Phi > 0 ? 1 : -1;
	if (FMath::Abs(PhiX) >= (FMath::Abs(PhiY) + Dy))
	{
		return PhiY + Sign * Dy;
	}
	if (FMath::Abs(PhiY) >= (FMath::Abs(PhiX) + Dx))
	{
		return PhiX + Sign * Dx;
	}
	T Dx2 = Dx * Dx;
	T Dy2 = Dy * Dy;
	T Diff = PhiX - PhiY;
	T Diff2 = Diff * Diff;
	return (Dy2 * PhiX + Dx2 * PhiY + Sign * Dx * Dy * FMath::Sqrt(Dx2 + Dy2 - Diff2)) / (Dx2 + Dy2);
}

template<class T, int d>
T TLevelSet<T, d>::ComputePhi(const TArrayND<bool, d>& Done, const TVector<int32, d>& CellIndex)
{
	int32 NumberOfAxes = 0;
	TVector<T, d> NeighborPhi;
	TVector<T, d> Dx;
	for (int32 Axis = 0; Axis < d; ++Axis)
	{
		const auto IP1 = CellIndex + TVector<int32, d>::AxisVector(Axis);
		const auto IM1 = CellIndex - TVector<int32, d>::AxisVector(Axis);
		if (IM1[Axis] < 0 || !Done(IM1)) // IM1 not valid
		{
			if (IP1[Axis] < MGrid.Counts()[Axis] && Done(IP1)) // IP1 is valid
			{
				Dx[NumberOfAxes] = MGrid.Dx()[Axis];
				NeighborPhi[NumberOfAxes++] = MPhi(IP1);
			}
		}
		else if (IP1[Axis] >= MGrid.Counts()[Axis] || !Done(IP1))
		{
			Dx[NumberOfAxes] = MGrid.Dx()[Axis];
			NeighborPhi[NumberOfAxes++] = MPhi(IM1);
		}
		else
		{
			Dx[NumberOfAxes] = MGrid.Dx()[Axis];
			if (FMath::Abs(MPhi(IP1)) < FMath::Abs(MPhi(IM1)))
			{
				NeighborPhi[NumberOfAxes++] = MPhi(IP1);
			}
			else
			{
				NeighborPhi[NumberOfAxes++] = MPhi(IM1);
			}
		}
	}
	if (NumberOfAxes == 1)
	{
		T Sign = MPhi(CellIndex) > 0 ? 1 : -1;
		T NewPhi = FGenericPlatformMath::Abs(NeighborPhi[0]) + Dx[0];
		check(NewPhi <= FGenericPlatformMath::Abs(MPhi(CellIndex)));
		return Sign * NewPhi;
	}
	T QuadraticXY = SolveQuadraticEquation(MPhi(CellIndex), NeighborPhi[0], NeighborPhi[1], Dx[0], Dx[1]);
	if (NumberOfAxes == 2 || FMath::Abs(NeighborPhi[2]) > FMath::Abs(QuadraticXY))
	{
		return QuadraticXY;
	}
	T QuadraticXZ = SolveQuadraticEquation(MPhi(CellIndex), NeighborPhi[0], NeighborPhi[2], Dx[0], Dx[2]);
	if (FMath::Abs(NeighborPhi[1]) > FMath::Abs(QuadraticXZ))
	{
		return QuadraticXZ;
	}
	T QuadraticYZ = SolveQuadraticEquation(MPhi(CellIndex), NeighborPhi[1], NeighborPhi[2], Dx[1], Dx[2]);
	if (FMath::Abs(NeighborPhi[0]) > FMath::Abs(QuadraticYZ))
	{
		return QuadraticYZ;
	}
	// Cubic
	T Sign = MPhi(CellIndex) > 0 ? 1 : -1;
	T Dx2 = Dx[0] * Dx[0];
	T Dy2 = Dx[1] * Dx[1];
	T Dz2 = Dx[2] * Dx[2];
	T Dx2Dy2 = Dx2 * Dy2;
	T Dx2Dz2 = Dx2 * Dz2;
	T Dy2Dz2 = Dy2 * Dz2;
	T XmY = NeighborPhi[0] - NeighborPhi[1];
	T XmZ = NeighborPhi[0] - NeighborPhi[2];
	T YmZ = NeighborPhi[1] - NeighborPhi[2];
	T XmY2 = XmY * XmY;
	T XmZ2 = XmZ * XmZ;
	T YmZ2 = YmZ * YmZ;
	return (Dy2Dz2 * NeighborPhi[0] + Dx2Dz2 * NeighborPhi[1] + Dx2Dy2 * NeighborPhi[2] + Sign * Dx.Product() * FMath::Sqrt(Dx2Dy2 + Dx2Dz2 + Dy2Dz2 - Dx2 * YmZ2 - Dy2 * XmZ2 - Dz2 * XmY2)) / (Dx2Dy2 + Dx2Dz2 + Dy2Dz2);
}

template<class T, int d>
void TLevelSet<T, d>::FloodFill(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color, int32& NextColor)
{
	const auto& Counts = MGrid.Counts();
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			for (int32 k = 0; k < Counts[2]; ++k)
			{
				const TVector<int32, d> CellIndex(i, j, k);
				if (Color(CellIndex) == -1)
				{
					FloodFillFromCell(CellIndex, ++NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
					check(Color(CellIndex) != -1);
				}
			}
		}
	}
}

template<class T, int d>
void TLevelSet<T, d>::FloodFillFromCell(const TVector<int32, d> RootCellIndex, const int32 NextColor, const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color)
{
	TArray<TVector<int32, d>> Queue;
	Queue.Add(RootCellIndex);
	while (Queue.Num())
	{
		TVector<int32, d> CellIndex = Queue.Pop();
		if (Color(CellIndex) == NextColor)
		{
			continue;
		}

		ensure(Color(CellIndex) == -1);
		Color(CellIndex) = NextColor;
		const auto CellIndexXP1 = CellIndex + TVector<int32, d>::AxisVector(0);
		const auto CellIndexXM1 = CellIndex - TVector<int32, d>::AxisVector(0);
		const auto CellIndexYP1 = CellIndex + TVector<int32, d>::AxisVector(1);
		const auto CellIndexYM1 = CellIndex - TVector<int32, d>::AxisVector(1);
		const auto CellIndexZP1 = CellIndex + TVector<int32, d>::AxisVector(2);
		const auto CellIndexZM1 = CellIndex - TVector<int32, d>::AxisVector(2);
		if (CellIndexZP1[2] < MGrid.Counts()[2] && !BlockedFaceZ(CellIndexZP1)  && Color(CellIndexZP1) != NextColor)
		{
			Queue.Add(CellIndexZP1);
		}
		if (!BlockedFaceZ(CellIndex) && CellIndexZM1[2] >= 0 && Color(CellIndexZM1) != NextColor)
		{
			Queue.Add(CellIndexZM1);
		}
		if (CellIndexYP1[1] < MGrid.Counts()[1] && !BlockedFaceY(CellIndexYP1) &&  Color(CellIndexYP1) != NextColor)
		{
			Queue.Add(CellIndexYP1);
		}
		if (!BlockedFaceY(CellIndex) && CellIndexYM1[1] >= 0 && Color(CellIndexYM1) != NextColor)
		{
			Queue.Add(CellIndexYM1);
		}
		if (CellIndexXP1[0] < MGrid.Counts()[0] && !BlockedFaceX(CellIndexXP1) && Color(CellIndexXP1) != NextColor)
		{
			Queue.Add(CellIndexXP1);
		}

		if (!BlockedFaceX(CellIndex) && CellIndexXM1[0] >= 0 && Color(CellIndexXM1) != NextColor)
		{
			Queue.Add(CellIndexXM1);
		}
	}
}

template<class T, int d>
bool TLevelSet<T, d>::IsIntersectingWithTriangle(const TParticles<T, d>& Particles, const TVector<int32, 3>& Element, const TPlane<T, d>& TrianglePlane, const TVector<int32, d>& CellIndex, const TVector<int32, d>& PrevCellIndex)
{
	auto Intersection = TrianglePlane.FindClosestIntersection(MGrid.Location(CellIndex), MGrid.Location(PrevCellIndex), 0);
	if (Intersection.Second)
	{
		const T Epsilon = 1e-1;	//todo(ocohen): fattening triangle up is relative to triangle size. Do we care about very large triangles?
		const TVector<T, 2> Bary = ComputeBarycentricInPlane(Particles.X(Element[0]), Particles.X(Element[1]), Particles.X(Element[2]), Intersection.First);
		
		if (Bary.X >= -Epsilon && Bary.Y >= -Epsilon && (Bary.Y + Bary.X) <= 1+Epsilon)
		{
			return true;
		}
	}
	return false;
}

template<class T, int d>
void TLevelSet<T, d>::ComputeNormals()
{
	const auto& Counts = MGrid.Counts();
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			for (int32 k = 0; k < Counts[2]; ++k)
			{
				const TVector<int32, d> CellIndex(i, j, k);
				const auto Dx = MGrid.Dx();
				TVector<T, d> X = MGrid.Location(CellIndex);
				MNormals(CellIndex) = TVector<T, d>(
				    (SignedDistance(X + TVector<T, d>::AxisVector(0) * Dx[0]) - SignedDistance(X - TVector<T, d>::AxisVector(0) * Dx[0])) / (2 * Dx[0]),
				    (SignedDistance(X + TVector<T, d>::AxisVector(1) * Dx[1]) - SignedDistance(X - TVector<T, d>::AxisVector(1) * Dx[1])) / (2 * Dx[1]),
				    (SignedDistance(X + TVector<T, d>::AxisVector(2) * Dx[2]) - SignedDistance(X - TVector<T, d>::AxisVector(2) * Dx[2])) / (2 * Dx[2]));
			}
		}
	}
}

template<class T, int d>
void TLevelSet<T, d>::Write(std::ostream& Stream) const
{
	MGrid.Write(Stream);
	MPhi.Write(Stream);
	Stream.write(reinterpret_cast<const char*>(&MBandWidth), sizeof(MBandWidth));
}

template<class T, int d>
T TLevelSet<T, d>::SignedDistance(const TVector<T, d>& x) const
{
	TVector<T, d> Location = MGrid.ClampMinusHalf(x);
	T SizeSquared = (Location - x).SizeSquared();
	T Phi = MGrid.LinearlyInterpolate(MPhi, Location);
	return SizeSquared ? (sqrt(SizeSquared) + Phi) : Phi;
}

template<class T, int d>
T TLevelSet<T, d>::PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const
{
	TVector<T, d> Location = MGrid.ClampMinusHalf(x);
	T SizeSquared = (Location - x).SizeSquared();
	Normal = SizeSquared ? MLocalBoundingBox.Normal(x) : MGrid.LinearlyInterpolate(MNormals, Location);
	T Phi = MGrid.LinearlyInterpolate(MPhi, Location);
	return SizeSquared ? (sqrt(SizeSquared) + Phi) : Phi;
}

template class Chaos::TLevelSet<float, 3>;
