// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDRigidsEvolutionGBF.h"

#include "Chaos/Box.h"
#include "Chaos/Defines.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/PBDCollisionConstraint.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PerParticleEtherDrag.h"
#include "Chaos/PerParticleEulerStepVelocity.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PerParticleInitForce.h"
#include "Chaos/PerParticlePBDEulerStep.h"
#include "Chaos/PerParticlePBDGroundConstraint.h"
#include "Chaos/PerParticlePBDUpdateFromDeltaPosition.h"
#include "ChaosStats.h"

#include "ProfilingDebugging/ScopedTimers.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/Levelset.h"

#define LOCTEXT_NAMESPACE "Chaos"

int32 DisableSim = 0;
FAutoConsoleVariableRef CVarDisableSim(TEXT("p.DisableSim"), DisableSim, TEXT("Disable Sim"));

using namespace Chaos;

template<class T, int d>
TPBDRigidsEvolutionGBF<T, d>::TPBDRigidsEvolutionGBF(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations)
    : Base(MoveTemp(InParticles), NumIterations)
{
	SetParticleUpdateVelocityFunction([PBDUpdateRule = TPerParticlePBDUpdateFromDeltaPosition<float, 3>(), this](TPBDRigidParticles<T, d>& MParticlesInput, const T Dt, const TArray<int32>& ActiveIndices) {
		PhysicsParallelFor(ActiveIndices.Num(), [&](int32 ActiveIndex) {
			int32 Index = ActiveIndices[ActiveIndex];
			PBDUpdateRule.Apply(MParticlesInput, Dt, Index);
		});
	});

	SetParticleUpdatePositionFunction([this](TPBDRigidParticles<T, d>& ParticlesInput, const T Dt)
	{
		PhysicsParallelFor(MActiveIndicesArray.Num(), [&](int32 ActiveIndex)
		{
			int32 Index = MActiveIndicesArray[ActiveIndex];
			ParticlesInput.X(Index) = ParticlesInput.P(Index);
			ParticlesInput.R(Index) = ParticlesInput.Q(Index);
		});
	});
}

template <typename T, int d>
void TPBDRigidsEvolutionGBF<T, d>::Integrate(const TArray<int32>& ActiveIndices, T Dt)
{
	double Time = 0.0;
	FDurationTimer Timer(Time);
	TPerParticleInitForce<T, d> InitForceRule;
	TPerParticleEulerStepVelocity<T, d> EulerStepVelocityRule;
	TPerParticleEtherDrag<T, d> EtherDragRule(0.0, 0.0);
	TPerParticlePBDEulerStep<T, d> EulerStepRule;
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("Init Time is %f"), Time);

	Time = 0;
	Timer.Start();
	PhysicsParallelFor(ActiveIndices.Num(), [&](int32 ActiveIndex) {
		int32 Index = ActiveIndices[ActiveIndex];
		check(!MParticles.Disabled(Index) && !MParticles.Sleeping(Index));

		//save off previous velocities
		MParticles.PreV(Index) = MParticles.V(Index);
		MParticles.PreW(Index) = MParticles.W(Index);

		InitForceRule.Apply(MParticles, Dt, Index);
		for (auto ForceRule : MForceRules)
		{
			ForceRule(MParticles, Dt, Index);
		}
		EulerStepVelocityRule.Apply(MParticles, Dt, Index);
		EtherDragRule.Apply(MParticles, Dt, Index);
		EulerStepRule.Apply(MParticles, Dt, Index);
	});
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("Per ParticleUpdate Time is %f"), Time);
	AddSubstep();
}

DECLARE_CYCLE_STAT(TEXT("AdvanceOneTimestep"), STAT_AdvanceOneTimeStep, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("UpdateContactGraph"), STAT_UpdateContactGraph, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("Apply+PushOut"), STAT_ApplyApplyPushOut, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("ParticleUpdateVelocity"), STAT_ParticleUpdateVelocity, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("SleepInactive"), STAT_SleepInactive, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("ParticleUpdatePosition"), STAT_ParticleUpdatePosition, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("CollisionContactsCallback"), STAT_CollisionContactsCallback, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("BreakingCallback"), STAT_BreakingCallback, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("TrailingCallback"), STAT_TrailingCallback, STATGROUP_Chaos);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumActiveParticles"), STAT_NumActiveParticles, STATGROUP_Chaos);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumActiveConstraints"), STAT_NumActiveConstraints, STATGROUP_Chaos);

int32 SelectedParticle = 1;
FAutoConsoleVariableRef CVarSelectedParticle(TEXT("p.SelectedParticle"), SelectedParticle, TEXT("Debug render for a specific particle"));
int32 ShowCollisionParticles = 0;
FAutoConsoleVariableRef CVarShowCollisionParticles(TEXT("p.ShowCollisionParticles"), ShowCollisionParticles, TEXT("Debug render the collision particles (can be very slow)"));
int32 ShowCenterOfMass = 1;
FAutoConsoleVariableRef CVarShowCenterOfMass(TEXT("p.ShowCenterOfMass"), ShowCenterOfMass, TEXT("Debug render of the center of mass, you will likely need wireframe mode on"));
int32 ShowBounds = 1;
FAutoConsoleVariableRef CVarShowBounds(TEXT("p.ShowBounds"), ShowBounds, TEXT(""));
int32 ShowLevelSet = 0;
FAutoConsoleVariableRef CVarShowLevelSet(TEXT("p.ShowLevelSet"), ShowLevelSet, TEXT(""));
float MaxVisualizePhiDistance = 10.f;
FAutoConsoleVariableRef CVarMaxPhiDistance(TEXT("p.MaxVisualizePhiDistance"), MaxVisualizePhiDistance, TEXT(""));
float CullPhiVisualizeDistance = 0.f;
FAutoConsoleVariableRef CVarCullPhiDistance(TEXT("p.CullPhiVisualizeDistance"), CullPhiVisualizeDistance, TEXT(""));

template<class T, int d>
void TPBDRigidsEvolutionGBF<T, d>::AdvanceOneTimeStep(const T Dt)
{
	if (DisableSim)
	{
		return;
	}
	SCOPE_CYCLE_COUNTER(STAT_AdvanceOneTimeStep);
	UE_LOG(LogChaos, Verbose, TEXT("START FRAME with Dt %f"), Dt);
	double FrameTime = 0, Time = 0;
	MActiveIndicesArray = MActiveIndices.Array();
	Integrate(MActiveIndicesArray, Dt);

	SET_DWORD_STAT(STAT_NumActiveParticles, MActiveIndicesArray.Num());

	MCollisionRule.Reset(MParticles, MPushOutIterations, MPushOutPairIterations, (T)0, MRestitution, MFriction);
	MCollisionRule.ComputeConstraints(MParticles, Dt);

	// @todo(mlentine): Constraints need to be considered for islands
	SET_DWORD_STAT(STAT_NumActiveConstraints, MCollisionRule.GetAllConstraints().Num());
	MCollisionRule.UpdateIslandsFromConstraints(MParticles, MIslandParticles, IslandSleepCounts, MActiveIndices);

	TArray<bool> SleepedIslands;
	SleepedIslands.SetNum(MIslandParticles.Num());
	{
		SCOPE_CYCLE_COUNTER(STAT_ApplyApplyPushOut);
		PhysicsParallelFor(MIslandParticles.Num(), [&](int32 Island) {
			TArray<int32> ActiveIndices = MIslandParticles[Island].Array();
			// Per level and per color
			MCollisionRule.UpdateAccelerationStructures(MParticles, ActiveIndices, Island);
			for (int i = 0; i < MNumIterations; ++i)
			{
				for (auto ConstraintRule : MConstraintRules)
				{
					ConstraintRule(MParticles, Dt, Island);
				}
				// Resolve collisions
				MCollisionRule.Apply(MParticles, Dt, Island);
			}
			MCollisionRule.ApplyPushOut(MParticles, Dt, ActiveIndices, Island);
			MParticleUpdateVelocity(MParticles, Dt, ActiveIndices);
			// Turn off if not moving
			SleepedIslands[Island] = MCollisionRule.SleepInactive(MParticles, ActiveIndices, IslandSleepCounts[Island], Island, SleepLinearThreshold, SleepAngularThreshold);
		});
	}

	for (int32 i = 0; i < MIslandParticles.Num(); ++i)
	{
		if (SleepedIslands[i])
		{
			for (const int32 Index : MIslandParticles[i])
			{
				MActiveIndices.Remove(Index);
			}
		}
	}

	MCollisionRule.CopyOutConstraints(MIslandParticles.Num());

	AddSubstep();
	MClustering.AdvanceClustering(Dt, MCollisionRule);
	AddSubstep();

	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleUpdatePosition);
		MParticleUpdatePosition(MParticles, Dt);
	}

#if CHAOS_DEBUG_DRAW
	if (FDebugDrawQueue::IsDebugDrawingEnabled())
	{
		if (1)
		{
			for (uint32 Idx = 0; Idx < MParticles.Size(); ++Idx)
			{
				if (MParticles.Disabled(Idx)) { continue; }
				if (ShowCollisionParticles && (SelectedParticle == Idx || ShowCollisionParticles == -1))
				{
					if (MParticles.CollisionParticles(Idx))
					{
						for (uint32 CollisionIdx = 0; CollisionIdx < MParticles.CollisionParticles(Idx)->Size(); ++CollisionIdx)
						{
							const TVector<T, d>& X = MParticles.CollisionParticles(Idx)->X(CollisionIdx);
							const TVector<T, d> WorldX = TRigidTransform<T, d>(MParticles.X(Idx), MParticles.R(Idx)).TransformPosition(X);
							FDebugDrawQueue::GetInstance().DrawDebugPoint(WorldX, FColor::Purple, false, 1e-4, 0, 10.f);
						}
					}
				}

				if (ShowCenterOfMass && (SelectedParticle == Idx || ShowCenterOfMass == -1))
				{
					FColor AxisColors[] = { FColor::Red, FColor::Green, FColor::Blue };
					for (int i = 0; i < d; ++i)
					{
						const TVector<T, d> WorldDir = MParticles.R(Idx) * TVector<T, d>::AxisVector(i) * 100;
						FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(MParticles.X(Idx), MParticles.X(Idx) + WorldDir, 3, AxisColors[i], false, 1e-4, 0, 2.f);

					}
					FDebugDrawQueue::GetInstance().DrawDebugSphere(MParticles.X(Idx), 20.f, 16, FColor::Yellow, false, 1e-4);
				}

				if (ShowBounds && (SelectedParticle == Idx || ShowBounds == -1) && MParticles.Geometry(Idx)->HasBoundingBox())
				{
					const TBox<T,d>& Bounds = MParticles.Geometry(Idx)->BoundingBox();
					const TRigidTransform<T, d> TM(MParticles.X(Idx), MParticles.R(Idx));
					const TVector<T, d> Center = TM.TransformPosition(Bounds.Center());
					FDebugDrawQueue::GetInstance().DrawDebugBox(Center, Bounds.Extents() * 0.5f, TM.GetRotation(), FColor::Yellow, false, 1e-4, 0, 2.f);
				}

				if (ShowLevelSet && (SelectedParticle == Idx || ShowLevelSet == -1))
				{
					auto RenderLevelSet = [](const TRigidTransform<T,d>& LevelSetToWorld, const TLevelSet<T,d>& LevelSet)
					{
						const TUniformGrid<T, d>& Grid = LevelSet.GetGrid();
						const int32 NumCells = Grid.GetNumCells();
						const TArrayND<T, 3>& PhiArray = LevelSet.GetPhiArray();
						for (int32 CellIdx = 0; CellIdx < NumCells; ++CellIdx)
						{
							const TVector<T, d> GridSpaceLocation = Grid.Center(CellIdx);
							const TVector<T, d> WorldSpaceLocation = LevelSetToWorld.TransformPosition(GridSpaceLocation);
							const T Phi = PhiArray(Grid.GetIndex(CellIdx));
							//FDebugDrawQueue::GetInstance().DrawDebugPoint(WorldSpaceLocation, Phi > 0 ? FColor::Purple : FColor::Green, false, 1e-4, 0, 10.f);
							//FDebugDrawQueue::GetInstance().DrawDebugSphere(WorldSpaceLocation, Grid.Dx().GetAbsMin(), 16, Phi > 0 ? FColor::Purple : FColor::Green, false, 1e-4);
							if (Phi <= CullPhiVisualizeDistance)
							{
								const T LocalPhi = Phi - CullPhiVisualizeDistance;
								const T MaxPhi =  (-LocalPhi / MaxVisualizePhiDistance) * 255;
								const uint8 MaxPhiInt = MaxPhi > 255 ? 255 : MaxPhi;
								FDebugDrawQueue::GetInstance().DrawDebugPoint(WorldSpaceLocation, FColor(255, MaxPhiInt, 255, 255), false, 1e-4, 0, 30.f);
							}
						}
					};

					if (const TLevelSet<T,d>* LevelSet = MParticles.Geometry(Idx)->template GetObject<TLevelSet<T, d>>())
					{
						RenderLevelSet(TRigidTransform<T,d>(MParticles.X(Idx), MParticles.R(Idx)), *LevelSet);
					}
					else if (TImplicitObjectTransformed<T, d>* Transformed = MParticles.Geometry(Idx)->template GetObject<TImplicitObjectTransformed<T, d>>())
					{
						if (const TLevelSet<T, d>* InnerLevelSet = Transformed->GetTransformedObject()->template GetObject<TLevelSet<T, d>>())
						{
							RenderLevelSet(Transformed->GetTransform() * TRigidTransform<T, d>(MParticles.X(Idx), MParticles.R(Idx)), *InnerLevelSet);
						}
					}
				}
			}
		}
		FDebugDrawQueue::GetInstance().Flush();
	}
#endif

	// Callback for PBDCollisionConstraint
	if (MCollisionContacts)
	{
		SCOPE_CYCLE_COUNTER(STAT_CollisionContactsCallback);
		MCollisionContacts(MParticles, MCollisionRule);
	}

	// Callback for Breaking
	if (MBreaking)
	{
		SCOPE_CYCLE_COUNTER(STAT_BreakingCallback);
		MBreaking(MParticles);
	}

	// Callback for Trailing
	if (MTrailing)
	{
		SCOPE_CYCLE_COUNTER(STAT_TrailingCallback);
		MTrailing(MParticles);
	}

	MTime += Dt;
}

template class Chaos::TPBDRigidsEvolutionGBF<float, 3>;

#undef LOCTEXT_NAMESPACE
