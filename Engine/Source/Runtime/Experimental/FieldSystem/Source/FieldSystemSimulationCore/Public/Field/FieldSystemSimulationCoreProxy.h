// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Framework/PhysicsProxy.h"
#include "PBDRigidsSolver.h"

#include "Field/FieldSystem.h"
#include "Field/FieldSystemSimulationCoreCallbacks.h"

#if INCLUDE_CHAOS

class UFieldSystem;
class FGeometryCollectionSolverCallbacks;
class FGeometryCollection;

class FIELDSYSTEMSIMULATIONCORE_API FFieldSystemSimulationProxy : public Chaos::FPhysicsProxy
{
public:
	FFieldSystemSimulationProxy(const FFieldSystem& InFieldSystem);

	// FPhysicsProxy interface
	virtual void SyncBeforeDestroy() override;
	virtual void OnRemoveFromScene() override;
	virtual void CacheResults() override;
	virtual void FlipCache() override;
	virtual void SyncToCache() override;
	//////////////////////////////////////////////////////////////////////////

private:

	virtual FSolverCallbacks* OnCreateCallbacks() override;
	virtual void OnDestroyCallbacks(FSolverCallbacks* InCallbacks) override;

	// input field system to copy
	const FFieldSystem& FieldSystem;

	// callbacks onto the solver.
	FFieldSystemSolverCallbacks * Callbacks;
};

#endif