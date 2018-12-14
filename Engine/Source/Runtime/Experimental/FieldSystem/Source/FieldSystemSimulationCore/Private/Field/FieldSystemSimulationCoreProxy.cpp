// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "Field/FieldSystemSimulationCoreProxy.h"
#include "Async/ParallelFor.h"
#include "PBDRigidsSolver.h"


FFieldSystemSimulationProxy::FFieldSystemSimulationProxy(const FFieldSystem& InFieldSystem) : 
	Chaos::FPhysicsProxy()
	, FieldSystem(InFieldSystem)
	, Callbacks(nullptr)
{
	check(IsInGameThread());
}

FSolverCallbacks* FFieldSystemSimulationProxy::OnCreateCallbacks()
{
	check(IsInGameThread());
	
	if (Callbacks)
	{
		delete Callbacks;
		Callbacks = nullptr;
	}

	Callbacks = new FFieldSystemSolverCallbacks(FieldSystem);
	
	return Callbacks;
}

void FFieldSystemSimulationProxy::OnDestroyCallbacks(FSolverCallbacks* InCallbacks)
{
	check(InCallbacks == Callbacks);
	delete InCallbacks;
}

void FFieldSystemSimulationProxy::OnRemoveFromScene()
{
}

void FFieldSystemSimulationProxy::CacheResults()
{
	
}

void FFieldSystemSimulationProxy::FlipCache()
{
	
}

void FFieldSystemSimulationProxy::SyncToCache()
{
	
}

void FFieldSystemSimulationProxy::SyncBeforeDestroy()
{
}

#endif
