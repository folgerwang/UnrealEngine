// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDCollisionTypes.h"
#include "Chaos/PBDContactGraph.h"
#include "Chaos/PerParticleRule.h"

#include <memory>
#include <queue>
#include <sstream>

namespace ChaosTest
{
	template<class T> void CollisionPGS();
	template<class T> void CollisionPGS2();
}

namespace Chaos
{
template<class T, int d>
class TPBDCollisionConstraintPGS
{
  public:
	friend void ChaosTest::CollisionPGS<T>();
	friend void ChaosTest::CollisionPGS2<T>();

	typedef TRigidBodyContactConstraintPGS<T, d> FRigidBodyContactConstraint;

	TPBDCollisionConstraintPGS(TPBDRigidParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const int32 PushOutIterations = 1, const int32 PushOutPairIterations = 1, const T Thickness = (T)0, const T Restitution = (T)0, const T Friction = (T)0);
	virtual ~TPBDCollisionConstraintPGS() {}

	void ComputeConstraints(const TPBDRigidParticles<T, d>& InParticles, const T Dt);

	void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island);
	void ApplyPushOut(TPBDRigidParticles<T, d>& InParticles, const T Dt, const TArray<int32>& ActiveIndices, const int32 Island);
	template<class T_PARTICLES> void Solve(T_PARTICLES& InParticles, const T Dt, const int32 Island);

	void CopyOutConstraints(int32 Island);

	void RemoveConstraints(const TSet<uint32>& RemovedParticles);
	void UpdateConstraints(const TPBDRigidParticles<T, d>& InParticles, T Dt, const TSet<uint32>& AddedParticles, const TArray<uint32>& ActiveParticles);

	/* Pass through for the ContactGraph*/
	void UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TArray<int32>& IslandSleepCounts, TSet<int32>& ActiveIndices);
	bool SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, int32& IslandSleepCount, const int32 Island, const T LinearSleepThreshold, const T AngularSleepThreshold) const;
	void UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island);

	static bool NearestPoint(TArray<Pair<TVector<T, d>, TVector<T, d>>>& Points, TVector<T, d>& Direction);

	const TArray<FRigidBodyContactConstraint>& GetAllConstraints() const { return MConstraints; }

  private:
	void PrintParticles(const TPBDRigidParticles<T, d>& InParticles, const int32 Island);
	void PrintConstraints(const TPBDRigidParticles<T, d>& InParticles, const int32 Island);

	template<class T_PARTICLES>
	void UpdateConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateLevelsetConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateLevelsetConstraintGJK(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateBoxConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateBoxPlaneConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateSphereConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateSpherePlaneConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	void UpdateSphereBoxConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);

	FRigidBodyContactConstraint ComputeLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex, const T Thickness);
	FRigidBodyContactConstraint ComputeLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex, const T Thickness);
	FRigidBodyContactConstraint ComputeBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Box1Index, int32 Box2Index, const T Thickness);
	FRigidBodyContactConstraint ComputeBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 BoxIndex, int32 PlaneIndex, const T Thickness);
	FRigidBodyContactConstraint ComputeSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Sphere1Index, int32 Sphere2Index, const T Thickness);
	FRigidBodyContactConstraint ComputeSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 PlaneIndex, const T Thickness);
	FRigidBodyContactConstraint ComputeSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 BoxIndex, const T Thickness);
	FRigidBodyContactConstraint ComputeConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Body1Index, int32 Body2Index, const T Thickness);

	TArray<FRigidBodyContactConstraint> MConstraints;
	TArrayCollectionArray<bool>& MCollided;
	TPBDContactGraph<FRigidBodyContactConstraint, T, d> MContactGraph;
	const int32 MNumIterations;
	const int32 MPairIterations;
	const T MThickness;
	const T MRestitution;
	const T MFriction;
	const T Tolerance;
	const T MaxIterations;
	const bool bUseCCD;
};
}
