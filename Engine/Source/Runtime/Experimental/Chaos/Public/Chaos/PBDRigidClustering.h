// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Transform.h"

namespace Chaos
{
/**/
struct ClusterId
{
	ClusterId(int32 NewId = -1)
	    : Id(NewId) {}
	int32 Id;
};

/**/
template<class FPBDRigidEvolution, class FPBDCollisionConstraint, class T, int d>
class TPBDRigidClustering
{
	typedef typename FPBDCollisionConstraint::FRigidBodyContactConstraint FRigidBodyContactConstraint;
  public:

	TPBDRigidClustering(FPBDRigidEvolution& InEvolution, TPBDRigidParticles<T, d>& InParticles);
	~TPBDRigidClustering() {}

	/**/
	void AdvanceClustering(const T dt, FPBDCollisionConstraint& CollisionRule);

	/**/
	int32 CreateClusterParticle(const TArray<uint32>& Children);

	/**/
	int32 CreateClusterParticleFromClusterChildren(const TArray<uint32>& Children, uint32 Island, const TRigidTransform<T, d>& ClusterWorldTM);

	/**/
	TSet<uint32> DeactivateClusterParticle(const uint32 ClusterIndex);

	/**/
	const TPBDRigidParticles<T, d>& Particles() const { return MParticles; }
	TPBDRigidParticles<T, d>& Particles() { return MParticles; }

	const TArrayCollectionArray<ClusterId>& ClusterIds() const { return MClusterIds; }
	const TArrayCollectionArray<bool>& InternalCluster() const { return MInternalCluster; }
	const TArrayCollectionArray<TRigidTransform<T, d>>& ChildToParentMap() const{ return MChildToParent; }
	/**/
	const T Strain(const uint32 Index) const { return MStrains[Index]; }
	T& Strain(const uint32 Index) { return MStrains[Index]; }

  private:
	void UpdateMassProperties(const TArray<uint32>& Children, const uint32 NewIndex);
	void UpdateGeometry(const TArray<uint32>& Children, const uint32 NewIndex);
	void UpdateIslandParticles(const uint32 ClusterIndex);
	void UpdateConnectivityGraph(uint32 ClusterIndex);
	TSet<uint32> ModifyClusterParticle(const uint32 ClusterIndex, const TMap<uint32, T>& StrainMap);
	TMap<uint32, T> ComputeStrainFromCollision(const FPBDCollisionConstraint& CollisionRule) const;


	FPBDRigidEvolution& MEvolution;
	TPBDRigidParticles<T, d>& MParticles;

	// Cluster data
	TArrayCollectionArray<TRigidTransform<T, d>> MChildToParent;
	TMap<uint32, TArray<uint32> > MParentToChildren;
	TArrayCollectionArray<ClusterId> MClusterIds;
	TArrayCollectionArray<bool> MInternalCluster;

	// User set parameters
	TArrayCollectionArray<T> MStrains;

	struct TConnectivityEdge
	{
		TConnectivityEdge() {}
		TConnectivityEdge(uint32 InSibling, T InStrain)
		: Sibling(InSibling)
		, Strain(InStrain) {}

		TConnectivityEdge(const TConnectivityEdge& Other)
		: Sibling(Other.Sibling)
		, Strain(Other.Strain) {}

		uint32 Sibling;
		T Strain;
	};
	TArrayCollectionArray<TArray<TConnectivityEdge>> MConnectivityEdges;
};

template <typename T, int d>
TArray<TVector<T,d>> CleanCollisionParticles(const TArray<TVector<T, d>>& Vertices, float SnapDistance)
{
	const float SnapDistance2 = SnapDistance * SnapDistance;
	const float SnapScale = 1.f / SnapDistance;

	TMap<uint64, TArray<int32>> HashToCleanedVertices;
	TArray<TVector<T, d>> CleanedVertices;
	CleanedVertices.Reserve(Vertices.Num());
	for (int32 VerticesIndex = 0; VerticesIndex < Vertices.Num(); VerticesIndex++)
	{
		const TVector<T, d>& OriginalV = Vertices[VerticesIndex];
		const TVector<T, d> ScaledV = Vertices[VerticesIndex] * SnapScale;
		const uint64 HighHash = (uint32)ScaledV.X | ((uint32)ScaledV.Z & 0xFFFF0000);
		const uint64 LowHash = (uint32)ScaledV.Y | ((uint32)ScaledV.Z & 0xFFFF);
		uint64 Hash = (HighHash << 32) | LowHash;
		if (HashToCleanedVertices.Contains(Hash))
		{
			TArray<int32>& CollisionVs = HashToCleanedVertices[Hash];
			bool bFoundHit = false;
			for (int32 PotentialSameV : CollisionVs)
			{
				if ((CleanedVertices[PotentialSameV] - OriginalV).SizeSquared() < SnapDistance2)
				{
					bFoundHit = true;
					break;
				}
			}
			if (!bFoundHit)
			{
				const int32 NewIdx = CleanedVertices.Add(OriginalV);
				CollisionVs.Add(NewIdx);
			}
		}
		else
		{
			const int32 NewIdx = CleanedVertices.Add(OriginalV);
			TArray<int32> Indices;
			Indices.Add(NewIdx);
			HashToCleanedVertices.Add(Hash, MoveTemp(Indices));
		}
	}

	return CleanedVertices;
}

template <typename T, int d>
TArray<TVector<T,d>> 
CleanCollisionParticles(TTriangleMesh<T> &TriMesh, const TArrayView<const TVector<T,d>>& Vertices, float Fraction)
{
	TArray<TVector<T, d>> CollisionVertices;
	if (Fraction <= 0.0)
		return CollisionVertices;

	// Get the importance vertex ordering, from most to least.  Reorder the 
	// particles accordingly.
	TArray<int32> CoincidentVertices;
	const TArray<int32> Ordering = TriMesh.GetVertexImportanceOrdering(Vertices, &CoincidentVertices, true);

	// Particles are ordered from most important to least, with coincident 
	// vertices at the very end.
	const int32 numGoodPoints = Ordering.Num() - CoincidentVertices.Num();
	CollisionVertices.AddUninitialized(std::min(numGoodPoints, static_cast<int32>(ceil(numGoodPoints * Fraction))));
	for(int i=0; i < CollisionVertices.Num(); i++)
		CollisionVertices[i] = Vertices[Ordering[i]];

	return CollisionVertices;
}

} // namespace Chaos
