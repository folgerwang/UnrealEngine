// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "GeometryCollection/StaticMeshSimulationComponentPhysicsProxy.h"
#include "GeometryCollection/StaticMeshSolverCallbacks.h"
#include "PBDRigidsSolver.h"

FStaticMeshSimulationComponentPhysicsProxy::FStaticMeshSimulationComponentPhysicsProxy(FCallbackInitFunc InInitFunc, FSyncDynamicFunc InSyncFunc)
	: Callbacks(nullptr)
	, InitialiseCallbackParamsFunc(InInitFunc)
	, SyncDynamicTransformFunc(InSyncFunc)
{
	check(IsInGameThread());

	Results.Get(0) = FTransform::Identity;
	Results.Get(1) = FTransform::Identity;
}

void FStaticMeshSimulationComponentPhysicsProxy::OnRemoveFromScene()
{
	// No callbacks means we haven't got anything in the scene yet
	if(!Callbacks)
	{
		return;
	}

	// Disable the particle we added
	const int32 ParticleId = Callbacks->GetRigidBodyId();
	Chaos::PBDRigidsSolver* CurrSolver = GetSolver();

	if(CurrSolver && ParticleId != INDEX_NONE)
	{
		// #BG TODO Special case here because right now we reset/realloc the evolution per geom component
		// in endplay which clears this out. That needs to not happen and be based on world shutdown
		if(CurrSolver->GetRigidParticles().Size() == 0)
		{
			return;
		}

		CurrSolver->GetRigidParticles().Disabled(ParticleId) = true;
		CurrSolver->InitializeFromParticleData();
	}
}

void FStaticMeshSimulationComponentPhysicsProxy::CacheResults()
{
	Results.GetPhysicsDataForWrite() = SimTransform;
}

void FStaticMeshSimulationComponentPhysicsProxy::FlipCache()
{
	Results.Flip();
}

void FStaticMeshSimulationComponentPhysicsProxy::SyncToCache()
{
	const FStaticMeshSolverCallbacks::Params& CurrentParams = Callbacks->GetParameters();
	
	if(CurrentParams.ObjectType == EObjectTypeEnum::Chaos_Object_Dynamic && CurrentParams.bSimulating && SyncDynamicTransformFunc)
	{
		// Send transform to update callable
		SyncDynamicTransformFunc(Results.GetGameDataForRead());
	}
}

FSolverCallbacks* FStaticMeshSimulationComponentPhysicsProxy::OnCreateCallbacks()
{
	check(IsInGameThread());

	if(Callbacks)
	{
		delete Callbacks;
		Callbacks = nullptr;
	}

	// Safe here - we're not in the solver yet if we're creating callbacks
	Results.Get(0) = FTransform::Identity;
	Results.Get(1) = FTransform::Identity;

	FStaticMeshSolverCallbacks::Params CallbackParams;

	InitialiseCallbackParamsFunc(CallbackParams);

	CallbackParams.TargetTransform = &SimTransform;

	Callbacks = new FStaticMeshSolverCallbacks();
	Callbacks->SetParameters(CallbackParams);
	Callbacks->Initialize();

	return Callbacks;
}

void FStaticMeshSimulationComponentPhysicsProxy::OnDestroyCallbacks(FSolverCallbacks* InCallbacks)
{
	check(Callbacks == InCallbacks);
	delete InCallbacks;
}

#endif
