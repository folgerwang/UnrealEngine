// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDRigidsEvolution.h"

#include "Chaos/Box.h"
#include "Chaos/Defines.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/PBDCollisionConstraint.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/PBDRigidsEvolutionPGS.h"
#include "Chaos/PerParticleEtherDrag.h"
#include "Chaos/PerParticleEulerStepVelocity.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PerParticleInitForce.h"
#include "Chaos/PerParticlePBDEulerStep.h"
#include "Chaos/PerParticlePBDGroundConstraint.h"
#include "Chaos/PerParticlePBDUpdateFromDeltaPosition.h"
#include "ChaosStats.h"
#include "ChaosLog.h"

#include "ProfilingDebugging/ScopedTimers.h"
#include "Chaos/DebugDrawQueue.h"

#define LOCTEXT_NAMESPACE "Chaos"

DEFINE_LOG_CATEGORY(LogChaos);

using namespace Chaos;

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
TPBDRigidsEvolutionBase<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::TPBDRigidsEvolutionBase(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations)
    : MParticles(MoveTemp(InParticles))
    , MClustering(static_cast<FPBDRigidsEvolution&>(*this), MParticles)
	, MTime(0)
	, MCollisionRule(InParticles, MCollided)
    , MWaitEvent(FPlatformProcess::GetSynchEventFromPool())
    , MDebugMode(false)
	, MFriction(0.5)
	, MRestitution(0.1)
	, SleepLinearThreshold(1.f)
	, SleepAngularThreshold(1.f)
	, MNumIterations(NumIterations)
	, MPushOutIterations(5)
	, MPushOutPairIterations(2)
{
	MParticles.AddArray(&MCollided);
	InitializeFromParticleData();
}

template class Chaos::TPBDRigidsEvolutionBase<TPBDRigidsEvolutionGBF<float, 3>, TPBDCollisionConstraint<float, 3>, float, 3>;
template class Chaos::TPBDRigidsEvolutionBase<TPBDRigidsEvolutionPGS<float, 3>, TPBDCollisionConstraintPGS<float, 3>, float, 3>;


#undef LOCTEXT_NAMESPACE