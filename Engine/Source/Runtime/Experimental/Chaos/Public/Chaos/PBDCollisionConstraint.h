// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDCollisionTypes.h"
#include "Chaos/PBDContactGraph.h"
#include "Chaos/PerParticleRule.h"

#include <memory>
#include <queue>
#include <sstream>

namespace Chaos
{
template<class T, int d>
class TPBDCollisionConstraintAccessor;

template <typename T, int d>
class TRigidTransform;

template <typename T, int d>
class TImplicitObject;

template <typename T, int d>
class TBVHParticles;

template <typename T, int d>
class TBox;

template<class T, int d>
class TPBDCollisionConstraint
{
  public:
	friend class TPBDCollisionConstraintAccessor<T, d>;
	template<class T_PARTICLES, class T2, int d2>
	friend void UpdateConstraintImp(const T_PARTICLES&, const TImplicitObject<T2, d2>&, const TRigidTransform<T2, d2>&, const TImplicitObject<T2, d2>&, const TRigidTransform<T2, d2>&, T2, TRigidBodyContactConstraint<T2, d2>&);

	typedef TRigidBodyContactConstraint<T, d> FRigidBodyContactConstraint;

	TPBDCollisionConstraint(TPBDRigidParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const int32 PushOutIterations = 1, const int32 PushOutPairIterations = 1, const T Thickness = (T)0, const T Restitution = (T)0, const T Friction = (T)0);
	virtual ~TPBDCollisionConstraint() {}

	void Reset(TPBDRigidParticles<T, d>& InParticles, const int32 PushOutIterations = 1, const int32 PushOutPairIterations = 1, const T Thickness = (T)0, const T Restitution = (T)0, const T Friction = (T)0)
	{
		MConstraints.Empty();
		MContactGraph = TPBDContactGraph<FRigidBodyContactConstraint, T, d>(InParticles);
		MNumIterations = PushOutIterations;
		MPairIterations = PushOutPairIterations;
		MThickness = Thickness;
		MRestitution = Restitution;
		MFriction = Friction;
		MAngularFriction = 0;
		bUseCCD = false;
	}

	void ComputeConstraints(const TPBDRigidParticles<T, d>& InParticles, T Dt);

	void Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island);
	void ApplyPushOut(TPBDRigidParticles<T, d>& InParticles, const T Dt, const TArray<int32>& ActiveIndices, const int32 Island);

	void CopyOutConstraints(int32 NumIslands);

	void RemoveConstraints(const TSet<uint32>& RemovedParticles);
	void UpdateConstraints(const TPBDRigidParticles<T, d>& InParticles, T Dt, const TSet<uint32>& AddedParticles, const TArray<uint32>& ActiveParticles);

	/* Pass through for the ContactGraph*/
	void UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TArray<int32>& IslandSleepCounts, TSet<int32>& ActiveIndices);
	bool SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, int32& IslandSleepCount, const int32 Island, const T LinearSleepThreshold, const T AngularSleepThreshold) const;
	void UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island);

	static bool NearestPoint(TArray<Pair<TVector<T, d>, TVector<T, d>>>& Points, TVector<T, d>& Direction, TVector<T, d>& ClosestPoint);

	const TArray<FRigidBodyContactConstraint>& GetAllConstraints() const { return MConstraints; }

  private:
	template<class T_PARTICLES>
	void UpdateConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint);
	template<class T_PARTICLES>
	static void UpdateLevelsetConstraint(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint);
	template<class T_PARTICLES>
	static void UpdateLevelsetConstraintGJK(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint);

	FRigidBodyContactConstraint ComputeConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Body1Index, int32 Body2Index, const T Thickness);

	TArray<FRigidBodyContactConstraint> MConstraints;
	TArrayCollectionArray<bool>& MCollided;
	TPBDContactGraph<FRigidBodyContactConstraint, T, d> MContactGraph;
	int32 MNumIterations;
	int32 MPairIterations;
	T MThickness;
	T MRestitution;
	T MFriction;
	T MAngularFriction;
	bool bUseCCD;
};
}
