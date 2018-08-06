// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ArrayND.h"
#include "Apeiron/Box.h"
#include "Apeiron/ImplicitObject.h"
#include "Apeiron/Particles.h"
#include "Apeiron/Plane.h"
#include "Apeiron/TriangleMesh.h"
#include "Apeiron/UniformGrid.h"

namespace Apeiron
{
template<class T, int d>
class APEIRON_API TLevelSet : public TImplicitObject<T, d>
{
  public:
	using TImplicitObject<T, d>::SignedDistance;

	TLevelSet(const TUniformGrid<T, d>& InGrid, const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 BandWidth = 0);
	TLevelSet(std::istream& Stream);
	TLevelSet(const TLevelSet<T, d>& Other) = delete;
	TLevelSet(TLevelSet<T, d>&& Other);
	~TLevelSet();

	void Write(std::ostream& Stream) const;
	T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const;
	T SignedDistance(const TVector<T, d>& x) const;

  private:
	void ComputeDistancesNearZeroIsocontour(const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, TArrayND<bool, d>& BlockedFaceX, TArrayND<bool, d>& BlockedFaceY, TArrayND<bool, d>& BlockedFaceZ, TArray<TVector<int32, d>>& InterfaceIndices);
	void CorrectSign(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ);
	T ComputePhi(const TVector<int32, d>& PrevCellIndex, const TVector<int32, d>& CellIndex, const int32 Axis);
	void FillWithFastMarchingMethod(const T StoppingDistance, const TArray<TVector<int32, d>>& InterfaceIndices);
	void FloodFill(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color, int32& NextColor);
	void FloodFillFromCell(const TVector<int32, d> CellIndex, const int32 NextColor, const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color);
	bool IsIntersectingWithTriangle(const TParticles<T, d>& Particles, const TVector<int32, 3>& Elements, const TPlane<T, d>& TrianglePlane, const TVector<int32, d>& CellIndex, const TVector<int32, d>& PrevCellIndex);
	void ComputeNormals();

	TUniformGrid<T, d> MGrid;
	TArrayND<T, d> MPhi;
	TArrayND<TVector<T, d>, d> MNormals;
	TBox<T, d> MLocalBoundingBox;
	int32 MBandWidth;
};
}
