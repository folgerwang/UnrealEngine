// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/KinematicGeometryParticles.h"
#include "Apeiron/PBDParticles.h"

#include <unordered_set>

namespace Apeiron
{
template<class T, int d>
class APEIRON_API TPBDEvolution
{
  public:
	// TODO(mlentine): Init particles from some type of input
	TPBDEvolution(TPBDParticles<T, d>&& InParticles, TKinematicGeometryParticles<T, d>&& InGeometryParticles, TArray<TVector<int32, 3>>&& CollisionTriangles, int32 NumIterations = 1, T CollisionThickness = 0, T SelfCollisionsThickness = 0, T CoefficientOfFriction = 0, T Damping = 0.01);
	~TPBDEvolution() {}

	void AdvanceOneTimeStep(const T dt);

	void SetKinematicUpdateFunction(TFunction<void(TPBDParticles<T, d>&, const T, const T, const int32)> KinematicUpdate) { MKinematicUpdate = KinematicUpdate; }
	void SetCollisionKinematicUpdateFunction(TFunction<void(TKinematicGeometryParticles<T, d>&, const T, const T, const int32)> KinematicUpdate) { MCollisionKinematicUpdate = KinematicUpdate; }
	void SetParticleUpdateFunction(TFunction<void(TPBDParticles<T, d>&, const T)> ParticleUpdate) { MParticleUpdate = ParticleUpdate; }
	void AddPBDConstraintFunction(TFunction<void(TPBDParticles<T, d>&, const T)> ConstraintFunction) { MConstraintRules.Add(ConstraintFunction); }
	void AddForceFunction(TFunction<void(TPBDParticles<T, d>&, const T, const int32)> ForceFunction) { MForceRules.Add(ForceFunction); }

	const TPBDParticles<T, d>& Particles() const { return MParticles; }
	TPBDParticles<T, d>& Particles() { return MParticles; }

	const TGeometryParticles<T, d>& CollisionParticles() const { return MCollisionParticles; }
	TGeometryParticles<T, d>& CollisionParticles() { return MCollisionParticles; }
	const bool Collided(int32 index) { return MCollided[index]; }

	TArray<TVector<int32, 3>>& CollisionTriangles() { return MCollisionTriangles; }
	TSet<TVector<int32, 2>>& DisabledCollisionElements() { return MDisabledCollisionElements; }

  private:
	TPBDParticles<T, d> MParticles;
	TKinematicGeometryParticles<T, d> MCollisionParticles;
	TArray<TVector<int32, 3>> MCollisionTriangles;
	TSet<TVector<int32, 2>> MDisabledCollisionElements;
	TArrayCollectionArray<bool> MCollided;
	int32 MNumIterations;
	T MCollisionThickness;
	T MSelfCollisionThickness;
	T MCoefficientOfFriction;
	T MDamping;
	T MTime;

	TArray<TFunction<void(TPBDParticles<T, d>&, const T, const int32)>> MForceRules;
	TArray<TFunction<void(TPBDParticles<T, d>&, const T)>> MConstraintRules;
	TFunction<void(TPBDParticles<T, d>&, const T)> MParticleUpdate;
	TFunction<void(TPBDParticles<T, d>&, const T, const T, const int32)> MKinematicUpdate;
	TFunction<void(TKinematicGeometryParticles<T, d>&, const T, const T, const int32)> MCollisionKinematicUpdate;
};
}
