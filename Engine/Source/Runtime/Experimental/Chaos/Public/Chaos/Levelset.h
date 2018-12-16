// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayND.h"
#include "Chaos/Box.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Particles.h"
#include "Chaos/Plane.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/UniformGrid.h"

namespace Chaos
{
template<class T, int d>
class CHAOS_API TLevelSet final : public TImplicitObject<T, d>
{
  public:
	using TImplicitObject<T, d>::SignedDistance;

	TLevelSet(const TUniformGrid<T, d>& InGrid, const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, const int32 BandWidth = 0);
	TLevelSet(const TUniformGrid<T, d>& InGrid, const TImplicitObject<T, d>& InObject, const int32 BandWidth = 0, const bool bUseObjectPhi = false);
	TLevelSet(std::istream& Stream);
	TLevelSet(const TLevelSet<T, d>& Other) = delete;
	TLevelSet(TLevelSet<T, d>&& Other);
	virtual ~TLevelSet();

	void Write(std::ostream& Stream) const;
	virtual T PhiWithNormal(const TVector<T, d>& x, TVector<T, d>& Normal) const override;
	T SignedDistance(const TVector<T, d>& x) const;

	virtual const TBox<T, d>& BoundingBox() const override { return MOriginalLocalBoundingBox; }

	// Returns a const ref to the underlying phi grid
	const TArrayND<T, d>& GetPhiArray() const { return MPhi; }

	// Returns a const ref to the underlying grid of normals
	const TArrayND<TVector<T, d>, d>& GetNormalsArray() const { return MNormals; }

	// Returns a const ref to the underlying grid structure
	const TUniformGrid<T, d>& GetGrid() const { return MGrid; }

	FORCEINLINE static ImplicitObjectType GetType()
	{
		return ImplicitObjectType::LevelSet;
	}

  private:
	void ComputeDistancesNearZeroIsocontour(const TParticles<T, d>& InParticles, const TTriangleMesh<T>& Mesh, TArrayND<bool, d>& BlockedFaceX, TArrayND<bool, d>& BlockedFaceY, TArrayND<bool, d>& BlockedFaceZ, TArray<TVector<int32, d>>& InterfaceIndices);
	void ComputeDistancesNearZeroIsocontour(const TImplicitObject<T, d>& Object, const TArrayND<T, d>& ObjectPhi, TArray<TVector<int32, d>>& InterfaceIndices);
	void CorrectSign(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArray<TVector<int32, d>>& InterfaceIndices);
	T ComputePhi(const TArrayND<bool, d>& Done, const TVector<int32, d>& CellIndex);
	void FillWithFastMarchingMethod(const T StoppingDistance, const TArray<TVector<int32, d>>& InterfaceIndices);
	void FloodFill(const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color, int32& NextColor);
	void FloodFillFromCell(const TVector<int32, d> CellIndex, const int32 NextColor, const TArrayND<bool, d>& BlockedFaceX, const TArrayND<bool, d>& BlockedFaceY, const TArrayND<bool, d>& BlockedFaceZ, TArrayND<int32, d>& Color);
	bool IsIntersectingWithTriangle(const TParticles<T, d>& Particles, const TVector<int32, 3>& Elements, const TPlane<T, d>& TrianglePlane, const TVector<int32, d>& CellIndex, const TVector<int32, d>& PrevCellIndex);
	void ComputeNormals();
	void ComputeConvexity(const TArray<TVector<int32, d>>& InterfaceIndices);

	TUniformGrid<T, d> MGrid;
	TArrayND<T, d> MPhi;
	TArrayND<TVector<T, d>, d> MNormals;
	TBox<T, d> MLocalBoundingBox;
	TBox<T, d> MOriginalLocalBoundingBox;
	int32 MBandWidth;
};
}
