// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PBDCollisionConstraint.h"
#include "Apeiron/PBDRigidParticles.h"
#include "Apeiron/Transform.h"

namespace Apeiron
{
template<class T, int d>
class TPBDRigidsEvolution;

/**/
struct ClusterId
{
	ClusterId(int32 NewId = -1)
	    : Id(NewId) {}
	int32 Id;
};

/**/
template<class T, int d>
class TPBDRigidClustering
{
  public:

	TPBDRigidClustering(TPBDRigidsEvolution<T, d>& InEvolution, TPBDRigidParticles<T, d>& InParticles);
	~TPBDRigidClustering() {}

	void UpdatePosition(TPBDRigidParticles<T, d>& MParticlesInput, const T Dt);

	/**/
	void AdvanceClustering(const T dt, TPBDCollisionConstraint<T, d>& CollisionRule);

	/**/
	int32 CreateClusterParticle(const TArray<uint32>& Children);

	/**/
	const TPBDRigidParticles<T, d>& Particles() const { return MParticles; }
	TPBDRigidParticles<T, d>& Particles() { return MParticles; }

	/**/
	const T Strain(const uint32 Index) const { return MStrains[Index]; }
	T& Strain(const uint32 Index) { return MStrains[Index]; }

  private:
	void UpdatePositionRecursive(TPBDRigidParticles<T, d>& MParticlesInput, TArray<bool> & Processed, const uint32 & Index);
	void UpdateMassProperties(const TArray<uint32>& Children, const uint32 NewIndex);
	void UpdateIslandParticles(const uint32 ClusterIndex);
	void UpdateChildAttributes(const uint32 ClusterIndex, const TSet<uint32> & Children);
	TSet<uint32> DeactivateClusterParticle(const uint32 ClusterIndex);

	TPBDRigidsEvolution<T, d>& MEvolution;
	TPBDRigidParticles<T, d>& MParticles;

	// Cluster data
	TMap<uint32, TRigidTransform<T, d>> MChildToParent;
	TMap<uint32, TArray<uint32> > MParentToChildren;
	TArrayCollectionArray<ClusterId> MClusterIds;

	// User set parameters
	TArrayCollectionArray<T> MStrains;
};
}
