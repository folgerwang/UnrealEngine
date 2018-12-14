// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDEvolution.h"

#include "Chaos/Framework/Parallel.h"
#include "Chaos/PBDCollisionSphereConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PerParticleDampVelocity.h"
#include "Chaos/PerParticleEulerStepVelocity.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PerParticleInitForce.h"
#include "Chaos/PerParticlePBDCollisionConstraint.h"
#include "Chaos/PerParticlePBDEulerStep.h"
#include "Chaos/PerParticlePBDGroundConstraint.h"
#include "Chaos/PerParticlePBDUpdateFromDeltaPosition.h"

using namespace Chaos;

template<class T, int d>
TPBDEvolution<T, d>::TPBDEvolution(TPBDParticles<T, d>&& InParticles, TKinematicGeometryParticles<T, d>&& InGeometryParticles, TArray<TVector<int32, 3>>&& CollisionTriangles,
    int32 NumIterations, T CollisionThickness, T SelfCollisionThickness, T CoefficientOfFriction, T Damping)
    : MParticles(MoveTemp(InParticles)), MCollisionParticles(MoveTemp(InGeometryParticles)), MCollisionTriangles(MoveTemp(CollisionTriangles)), MNumIterations(NumIterations), MCollisionThickness(CollisionThickness), MSelfCollisionThickness(SelfCollisionThickness), MCoefficientOfFriction(CoefficientOfFriction), MDamping(Damping), MTime(0)
{
	MCollisionParticles.AddArray(&MCollided);
	SetParticleUpdateFunction([PBDUpdateRule = TPerParticlePBDUpdateFromDeltaPosition<float, 3>()](TPBDParticles<T, d>& MParticlesinput, const T Dt) {
		PhysicsParallelFor(MParticlesinput.Size(), [&](int32 Index) {
			PBDUpdateRule.Apply(MParticlesinput, Dt, Index);
		});
	});
}

template<class T, int d>
void TPBDEvolution<T, d>::AdvanceOneTimeStep(const T Dt)
{
	TPerParticleInitForce<T, d> InitForceRule;
	TPerParticleEulerStepVelocity<T, d> EulerStepVelocityRule;
	TPerParticleDampVelocity<T, d> DampVelocityRule(MDamping);
	TPerParticlePBDEulerStep<T, d> EulerStepRule;
	TPerParticlePBDCollisionConstraint<T, d> CollisionRule(MCollisionParticles, MCollided, MCollisionThickness, MCoefficientOfFriction);

	DampVelocityRule.UpdatePositionBasedState(MParticles);

	PhysicsParallelFor(MCollisionParticles.Size(), [&](int32 Index) {
		MCollided[Index] = false;
	});
	PhysicsParallelFor(MParticles.Size(), [&](int32 Index) {
		InitForceRule.Apply(MParticles, Dt, Index);
		for (auto ForceRule : MForceRules)
		{
			ForceRule(MParticles, Dt, Index);
		}
		if (MKinematicUpdate)
		{
			MKinematicUpdate(MParticles, Dt, MTime + Dt, Index);
		}
		EulerStepVelocityRule.Apply(MParticles, Dt, Index);
		DampVelocityRule.Apply(MParticles, Dt, Index);
		EulerStepRule.Apply(MParticles, Dt, Index);
	});
	if (MCollisionKinematicUpdate)
	{
		PhysicsParallelFor(MCollisionParticles.Size(), [&](int32 Index) {
			MCollisionKinematicUpdate(MCollisionParticles, Dt, MTime + Dt, Index);
		});
	}
#if !COMPILE_WITHOUT_UNREAL_SUPPORT
	TPBDCollisionSpringConstraints<T, d> SelfCollisionRule(MParticles, MCollisionTriangles, MDisabledCollisionElements, Dt, MSelfCollisionThickness, 1.5f);
#endif
	for (int i = 0; i < MNumIterations; ++i)
	{
		for (auto ConstraintRule : MConstraintRules)
		{
			ConstraintRule(MParticles, Dt);
		}
#if !COMPILE_WITHOUT_UNREAL_SUPPORT
		SelfCollisionRule.Apply(MParticles, Dt);
#endif
		CollisionRule.ApplyPerParticle(MParticles, Dt);
	}
	check(MParticleUpdate);
	MParticleUpdate(MParticles, Dt);
	if (MCoefficientOfFriction > 0)
	{
		PhysicsParallelFor(MParticles.Size(), [&](int32 Index) {
			CollisionRule.ApplyFriction(MParticles, Dt, Index);
		});
	}
	MTime += Dt;
}

template class Chaos::TPBDEvolution<float, 3>;
