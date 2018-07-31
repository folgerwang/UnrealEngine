// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PBDCollisionTypes.h"
#include "Apeiron/PBDContactGraph.h"
#include "Apeiron/PerParticleRule.h"

#include <memory>
#include <queue>
#include <sstream>

namespace Apeiron
{
template<class T, int d>
class TPBDCollisionConstraintAccessor;

template<class T, int d>
class TPBDCollisionConstraint
{
  public:
	friend class TPBDCollisionConstraintAccessor<T, d>;

	typedef TRigidBodyContactConstraint<T, d> FRigidBodyContactConstraint;

	TPBDCollisionConstraint(TPBDRigidParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const int32 PushOutIterations = 1, const int32 PushOutPairIterations = 1, const T Thickness = (T)0, const T Restitution = (T)0, const T Friction = (T)0);
	virtual ~TPBDCollisionConstraint() {}

	void ComputeConstraints(const TPBDRigidParticles<T, d>& InParticles);

	void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island) const;
	void ApplyPushOut(TPBDRigidParticles<T, d>& InParticles, const int32 Island);

	void RemoveConstraints(const TSet<uint32>& RemovedParticles);
	void UpdateConstraints(const TPBDRigidParticles<T, d>& InParticles, const TSet<uint32>& AddedParticles, const TArray<uint32>& ActiveParticles);

	/* Pass through for the ContactGraph*/
	void UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TSet<int32>& ActiveIndices);
	void SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, TSet<int32>& GlobalActiveIndices, const int32 Island) const;
	void UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island);

  private:
	void UpdateConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);
	void UpdateSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint);

	FRigidBodyContactConstraint ComputeLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex);
	FRigidBodyContactConstraint ComputeLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex);
	FRigidBodyContactConstraint ComputeBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Box1Index, int32 Box2Index);
	FRigidBodyContactConstraint ComputeBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 BoxIndex, int32 PlaneIndex);
	FRigidBodyContactConstraint ComputeSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Sphere1Index, int32 Sphere2Index);
	FRigidBodyContactConstraint ComputeSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 PlaneIndex);
	FRigidBodyContactConstraint ComputeSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 BoxIndex);
	FRigidBodyContactConstraint ComputeConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Body1Index, int32 Body2Index);

	TArray<FRigidBodyContactConstraint> MConstraints;
	TArrayCollectionArray<bool>& MCollided;
	TPBDContactGraph<T, d> MContactGraph;
	const int32 MNumIterations;
	const int32 MPairIterations;
	const T MThickness;
	const T MRestitution;
	const T MFriction;
};
}
