// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/PBDRigidsEvolution.h"

#include "Apeiron/Box.h"
#include "Apeiron/Defines.h"
#include "Apeiron/ImplicitObjectTransformed.h"
#include "Apeiron/ImplicitObjectUnion.h"
#include "Apeiron/PBDCollisionConstraint.h"
#include "Apeiron/PBDCollisionSpringConstraints.h"
#include "Apeiron/ParallelFor.h"
#include "Apeiron/PerParticleDampVelocity.h"
#include "Apeiron/PerParticleEulerStepVelocity.h"
#include "Apeiron/PerParticleGravity.h"
#include "Apeiron/PerParticleInitForce.h"
#include "Apeiron/PerParticlePBDEulerStep.h"
#include "Apeiron/PerParticlePBDGroundConstraint.h"
#include "Apeiron/PerParticlePBDUpdateFromDeltaPosition.h"

#include "ProfilingDebugging/ScopedTimers.h"

#define LOCTEXT_NAMESPACE "Apeiron"

DEFINE_LOG_CATEGORY(LogApeiron);

using namespace Apeiron;

#define GLOBAL_DESTRUCTION 1

template<class T, int d>
TPBDRigidsEvolution<T, d>::TPBDRigidsEvolution(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations)
    : MParticles(MoveTemp(InParticles))
    , MClustering(*this, MParticles)
    , MNumIterations(NumIterations)
    , MTime(0)
    , MWaitEvent(FPlatformProcess::GetSynchEventFromPool())
    , MDebugMode(false)
	, MFriction(0.5)
	, MRestitution(0.1)
{
	MParticles.AddArray(&MCollided);
	SetParticleUpdateFunction([PBDUpdateRule = TPerParticlePBDUpdateFromDeltaPosition<float, 3>(), this](TPBDRigidParticles<T, d>& MParticlesInput, const T Dt) {
		ParallelFor(MActiveIndicesArray.Num(), [&](int32 ActiveIndex) {
			int32 Index = MActiveIndicesArray[ActiveIndex];
			PBDUpdateRule.Apply(MParticlesInput, Dt, Index);
		});
		MClustering.UpdatePosition(MParticlesInput, Dt);
	});
	InitializeFromParticleData();
}

template<class T, int d>
void TPBDRigidsEvolution<T, d>::AdvanceOneTimeStep(const T Dt)
{
	UE_LOG(LogApeiron, Verbose, TEXT("START FRAME with Dt %f"), Dt);
	double FrameTime = 0, Time = 0;
	MActiveIndicesArray = MActiveIndices.Array();
	FDurationTimer Timer(Time), FrameTimer(FrameTime);
	TPerParticleInitForce<T, d> InitForceRule;
	TPerParticleEulerStepVelocity<T, d> EulerStepVelocityRule;
	TPerParticleDampVelocity<T, d> DampVelocityRule;
	TPerParticlePBDEulerStep<T, d> EulerStepRule;
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Init Time is %f"), Time);

	Time = 0;
	Timer.Start();
	DampVelocityRule.UpdatePositionBasedState(MParticles, MActiveIndicesArray);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Update PBS Time is %f"), Time);

	Time = 0;
	Timer.Start();
	ParallelFor(MActiveIndicesArray.Num(), [&](int32 ActiveIndex) {
		int32 Index = MActiveIndicesArray[ActiveIndex];
		check(!MParticles.Disabled(Index) && !MParticles.Sleeping(Index));
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
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Per ParticleUpdate Time is %f"), Time);
	AddSubstep();

	Time = 0;
	Timer.Start();
	for (int i = 0; i < MNumIterations; ++i)
	{
		for (auto ConstraintRule : MConstraintRules)
		{
			ConstraintRule(MParticles, Dt);
		}
	}
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Constraint Update Time is %f"), Time);
	AddSubstep();

	Time = 0;
	Timer.Start();
	check(MParticleUpdate);
	MParticleUpdate(MParticles, Dt);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Particle Update Time is %f"), Time);
	AddSubstep();

	Time = 0;
	Timer.Start();
	TPBDCollisionConstraint<T, d> CollisionRule(MParticles, MCollided, 2, 5, (T)0, MRestitution, MFriction);
	CollisionRule.ComputeConstraints(MParticles);
	CollisionRule.UpdateIslandsFromConstraints(MParticles, MIslandParticles, MActiveIndices);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Find Collision Pairs Time is %f"), Time);
	Time = 0;
	Timer.Start();
	ParallelFor(MIslandParticles.Num(), [&](int32 Island) {
		TArray<int32> ActiveIndices = MIslandParticles[Island].Array();
		// Per level and per color
		CollisionRule.UpdateAccelerationStructures(MParticles, ActiveIndices, Island);
		// Resolve collisions
		for (int i = 0; i < MNumIterations; ++i)
		{
			CollisionRule.Apply(MParticles, Dt, Island);
		}
		CollisionRule.ApplyPushOut(MParticles, Island);
		// Turn off if not moving
		CollisionRule.SleepInactive(MParticles, ActiveIndices, MActiveIndices, Island);
	});
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Collision Update Time is %f"), Time);

	AddSubstep();
	MClustering.AdvanceClustering(Dt, CollisionRule);
	AddSubstep();

	MTime += Dt;

	FrameTimer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Time Step Update Time is %f"), FrameTime);
}

template class Apeiron::TPBDRigidsEvolution<float, 3>;

#undef LOCTEXT_NAMESPACE
