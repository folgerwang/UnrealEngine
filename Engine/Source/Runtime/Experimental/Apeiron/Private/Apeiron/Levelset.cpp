// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/Levelset.h"

#include <algorithm>
#include <vector>

using namespace Apeiron;


#define MAX_CLAMP(a,comp,b) (a>=comp?b:a)
#define MIN_CLAMP(a,comp,b) (a<comp?b:a)
#define RANGE_CLAMP(a,comp,b) ((a<0 || comp<=a)?b:a)

template<class T, int d>
TLevelSet<T, d>::TLevelSet(const TUniformGrid<T, d>& InGrid, const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 BandWidth)
    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; }, nullptr, nullptr)
    , MGrid(InGrid)
    , MPhi(MGrid)
    , MNormals(MGrid)
    , MLocalBoundingBox(MGrid.MinCorner(), MGrid.MaxCorner())
    , MBandWidth(BandWidth)
{
	check(d == 3);
	TArrayND<bool, d> BlockedFaceX(MGrid.Counts() + TVector<int32, d>::AxisVector(0));
	TArrayND<bool, d> BlockedFaceY(MGrid.Counts() + TVector<int32, d>::AxisVector(1));
	TArrayND<bool, d> BlockedFaceZ(MGrid.Counts() + TVector<int32, d>::AxisVector(2));
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
	CorrectSign(BlockedFaceX, BlockedFaceY, BlockedFaceZ);
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
	this->bIsConvex = true;
	int32 Sign = 1;
	bool bFirst = true;
	int ZOffset = MGrid.Counts()[2];
	int YZOffset = MGrid.Counts()[1] * ZOffset;
	int32 NumCells = MGrid.Counts().Product();
	for (int32 i = 0; i < NumCells; ++i)
	{
		if (FMath::Abs(MPhi[i]) > MGrid.Dx().Max())
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
		T PhiXY = (MPhi[MAX_CLAMP(i + YZOffset + ZOffset, NumCells, i)]
			+ MPhi[MIN_CLAMP(i - YZOffset - ZOffset, 0, i)]
			- MPhi[RANGE_CLAMP(i - YZOffset + ZOffset, NumCells, i)]
			- MPhi[RANGE_CLAMP(i + YZOffset - ZOffset, NumCells, i)])
			/ (4 * MGrid.Dx()[0] * MGrid.Dx()[1]);
		T PhiXZ = (MPhi[MAX_CLAMP(i + YZOffset + 1, NumCells, i)]
			+ MPhi[MIN_CLAMP(i - YZOffset - 1, 0, i)]
			- MPhi[RANGE_CLAMP(i - YZOffset + 1, NumCells, i)]
			- MPhi[RANGE_CLAMP(i + YZOffset - 1, NumCells, i)])
			/ (4 * MGrid.Dx()[0] * MGrid.Dx()[2]);
		T PhiYZ = (MPhi[MAX_CLAMP(i + ZOffset + 1, NumCells, i)]
			+ MPhi[MIN_CLAMP(i - ZOffset - 1, 0, i)]
			- MPhi[RANGE_CLAMP(i - ZOffset + 1, NumCells, i)]
			- MPhi[RANGE_CLAMP(i + ZOffset - 1, NumCells, i)])
			/ (4 * MGrid.Dx()[1] * MGrid.Dx()[2]);

		T Denom = sqrt(PhiX * PhiX + PhiY * PhiY + PhiZ * PhiZ);
		if (Denom > SMALL_NUMBER)
		{
			T curvature = -(PhiX * PhiX * PhiYY - 2 * PhiX * PhiY * PhiXY + PhiY * PhiY * PhiXX + PhiX * PhiX * PhiZZ - 2 * PhiX * PhiZ * PhiXZ + PhiZ * PhiZ * PhiXX + PhiY * PhiY * PhiZZ - 2 * PhiY * PhiZ * PhiYZ + PhiZ * PhiZ * PhiYY) / (Denom * Denom * Denom);
			LocalSign = curvature > 0 ? 1 : -1;
			if (bFirst)
			{
				bFirst = false;
				Sign = LocalSign;
			}
			else
			{
				if (Sign != LocalSign)
				{
					this->bIsConvex = false;
					return;
				}
			}
		}
	}
}

template<class T, int d>
TLevelSet<T, d>::TLevelSet(std::istream& Stream)
    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; }, nullptr, nullptr)
    , MGrid(Stream)
    , MPhi(Stream)
    , MLocalBoundingBox(MGrid.MinCorner(), MGrid.MaxCorner())
{
	Stream.read(reinterpret_cast<char*>(&MBandWidth), sizeof(MBandWidth));
	ComputeNormals();
}

template<class T, int d>
TLevelSet<T, d>::TLevelSet(TLevelSet<T, d>&& Other)
    : TImplicitObject<T, d>([this](const TVector<T, d>& x, TVector<T, d>& Normal) { return PhiWithNormal(x, Normal); },
          [this]() -> const TBox<T, d>& { return MLocalBoundingBox; }, nullptr, nullptr)
    , MGrid(MoveTemp(Other.MGrid))
    , MPhi(MoveTemp(Other.MPhi))
    , MLocalBoundingBox(MoveTemp(Other.MLocalBoundingBox))
    , MBandWidth(Other.MBandWidth)
{
	Other.MPhiWithNormal = nullptr;
	Other.MBoundingBox = nullptr;
}

template<class T, int d>
TLevelSet<T, d>::~TLevelSet()
{
}

template<class T, int d>
void TLevelSet<T, d>::ComputeDistancesNearZeroIsocontour(const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, TArrayND<bool, d>& BlockedFaceX, TArrayND<bool, d>& BlockedFaceY, TArrayND<bool, d>& BlockedFaceZ, TArray<TVector<int32, d>>& InterfaceIndices)
{
	MPhi.Fill(FLT_MAX);
	const auto Normals = Mesh.GetFaceNormals(InParticles);
	BlockedFaceX.Fill(false);
	BlockedFaceY.Fill(false);
	BlockedFaceZ.Fill(false);
	const auto& Elements = Mesh.GetSurfaceElements();
	for (int32 Index = 0; Index < Elements.Num(); ++Index)
	{
		const auto& Element = Elements[Index];
		TPlane<T, d> TrianglePlane(InParticles.X(Element[0]), Normals[Index]);
		TBox<T, d> TriangleBounds(InParticles.X(Element[0]), InParticles.X(Element[0]));
		TriangleBounds.GrowToInclude(InParticles.X(Element[1]));
		TriangleBounds.GrowToInclude(InParticles.X(Element[2]));
		TVector<int32, d> StartIndex = MGrid.ClampIndex(MGrid.Cell(TriangleBounds.Min() - TVector<T, d>(0.5 * MGrid.Dx())));
		TVector<int32, d> EndIndex = MGrid.ClampIndex(MGrid.Cell(TriangleBounds.Max() + TVector<T, d>(0.5 * MGrid.Dx())));
		for (int32 i = StartIndex[0]; i <= EndIndex[0]; ++i)
		{
			for (int32 j = StartIndex[1]; j <= EndIndex[1]; ++j)
			{
				for (int32 k = StartIndex[2]; k <= EndIndex[2]; ++k)
				{
					const TVector<int32, d> CellIndex(i, j, k);
					const auto Center = MGrid.Location(CellIndex);
					const auto Point = TrianglePlane.FindClosestPoint(Center);
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
void TLevelSet<T, d>::CorrectSign(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ)
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
	TArray<bool> ColorIsInside;
	ColorIsInside.SetNum(NextColor + 1);
	for (int32 i = 0; i <= NextColor; ++i)
	{
		ColorIsInside[i] = true;
	}
	for (int32 j = 0; j < Counts[1]; ++j)
	{
		for (int32 k = 0; k < Counts[2]; ++k)
		{
			ColorIsInside[Color(TVector<int32, d>(0, j, k))] = false;
			ColorIsInside[Color(TVector<int32, d>(Counts[0] - 1, j, k))] = false;
		}
	}
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 k = 0; k < Counts[2]; ++k)
		{
			ColorIsInside[Color(TVector<int32, d>(i, 0, k))] = false;
			ColorIsInside[Color(TVector<int32, d>(i, Counts[1] - 1, k))] = false;
		}
	}
	for (int32 i = 0; i < Counts[0]; ++i)
	{
		for (int32 j = 0; j < Counts[1]; ++j)
		{
			ColorIsInside[Color(TVector<int32, d>(i, j, 0))] = false;
			ColorIsInside[Color(TVector<int32, d>(i, j, Counts[2] - 1))] = false;
		}
	}
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
}

template<class T, int d>
bool Compare(const Pair<T*, TVector<int32, d>>& Other1, const Pair<T*, TVector<int32, d>>& Other2)
{
	return *Other1.First > *Other2.First;
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
			if (IM1[Axis] >= 0)
			{
				MPhi(IM1) = ComputePhi(Smallest.Second, IM1, Axis);
				if (!Done(IM1) && !InHeap(IM1))
				{
					Heap.Add(MakePair(&MPhi(IM1), IM1));
					InHeap(IM1) = true;
				}
			}
			if (IP1[Axis] < MGrid.Counts()[Axis])
			{
				MPhi(IP1) = ComputePhi(Smallest.Second, IP1, Axis);
				if (!Done(IP1) && !InHeap(IP1))
				{
					Heap.Add(MakePair(&MPhi(IP1), IP1));
					InHeap(IP1) = true;
				}
			}
		}
		Heap.Heapify(Compare<T, d>);
	}
}

template<class T, int d>
T TLevelSet<T, d>::ComputePhi(const TVector<int32, d>& PrevCellIndex, const TVector<int32, d>& CellIndex, const int32 Axis)
{
	// TODO(mlentine): Make this more accurate as we currently ignore the diagonal case
	T Sign = MPhi(CellIndex) > 0 ? 1 : -1;
	T NewPhi = FGenericPlatformMath::Abs(MPhi(PrevCellIndex)) + MGrid.Dx()[Axis];
	return NewPhi < FGenericPlatformMath::Abs(MPhi(CellIndex)) ? Sign * NewPhi : MPhi(CellIndex);
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
void TLevelSet<T, d>::FloodFillFromCell(const TVector<int32, d> CellIndex, const int32 NextColor, const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color)
{
	if (Color(CellIndex) == NextColor)
	{
		return;
	}
	check(Color(CellIndex) == -1);
	Color(CellIndex) = NextColor;
	const auto CellIndexXP1 = CellIndex + TVector<int32, d>::AxisVector(0);
	const auto CellIndexXM1 = CellIndex - TVector<int32, d>::AxisVector(0);
	const auto CellIndexYP1 = CellIndex + TVector<int32, d>::AxisVector(1);
	const auto CellIndexYM1 = CellIndex - TVector<int32, d>::AxisVector(1);
	const auto CellIndexZP1 = CellIndex + TVector<int32, d>::AxisVector(2);
	const auto CellIndexZM1 = CellIndex - TVector<int32, d>::AxisVector(2);
	if (!BlockedFaceX(CellIndex) && CellIndexXM1[0] >= 0)
	{
		FloodFillFromCell(CellIndexXM1, NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
	}
	if (!BlockedFaceX(CellIndexXP1) && CellIndexXP1[0] < MGrid.Counts()[0])
	{
		FloodFillFromCell(CellIndexXP1, NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
	}
	if (!BlockedFaceY(CellIndex) && CellIndexYM1[1] >= 0)
	{
		FloodFillFromCell(CellIndexYM1, NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
	}
	if (!BlockedFaceY(CellIndexYP1) && CellIndexYP1[1] < MGrid.Counts()[1])
	{
		FloodFillFromCell(CellIndexYP1, NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
	}
	if (!BlockedFaceZ(CellIndex) && CellIndexZM1[2] >= 0)
	{
		FloodFillFromCell(CellIndexZM1, NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
	}
	if (!BlockedFaceZ(CellIndexZP1) && CellIndexZP1[2] < MGrid.Counts()[2])
	{
		FloodFillFromCell(CellIndexZP1, NextColor, BlockedFaceX, BlockedFaceY, BlockedFaceZ, Color);
	}
}

template<class T, int d>
bool TLevelSet<T, d>::IsIntersectingWithTriangle(const TParticles<T, d>& Particles, const TVector<int32, 3>& Element, const TPlane<T, d>& TrianglePlane, const TVector<int32, d>& CellIndex, const TVector<int32, d>& PrevCellIndex)
{
	auto Intersection = TrianglePlane.FindClosestIntersection(MGrid.Location(CellIndex), MGrid.Location(PrevCellIndex), 0);
	if (Intersection.Second)
	{
		TVector<T, 2> Bary;
		TVector<T, d> P10 = Particles.X(Element[1]) - Particles.X(Element[0]);
		TVector<T, d> P20 = Particles.X(Element[2]) - Particles.X(Element[0]);
		TVector<T, d> PP0 = Intersection.First - Particles.X(Element[0]);
		T Size10 = P10.SizeSquared();
		T Size20 = P20.SizeSquared();
		T ProjSides = TVector<T, d>::DotProduct(P10, P20);
		T ProjP1 = TVector<T, d>::DotProduct(PP0, P10);
		T ProjP2 = TVector<T, d>::DotProduct(PP0, P20);
		T Denom = Size10 * Size20 - ProjSides * ProjSides;
		Bary.X = (Size20 * ProjP1 - ProjSides * ProjP2) / Denom;
		Bary.Y = (Size10 * ProjP2 - ProjSides * ProjP1) / Denom;
		if (Bary.X >= 0 && Bary.Y >= 0 && (Bary.Y + Bary.X) <= 1)
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

template class Apeiron::TLevelSet<float, 3>;
