// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "Framework/PhysicsProxy.h"
#include "Framework/BufferedData.h"
#include "StaticMeshSolverCallbacks.h"

class FSolverCallbacks;

class GEOMETRYCOLLECTIONSIMULATIONCORE_API FStaticMeshSimulationComponentPhysicsProxy : public Chaos::FPhysicsProxy
{
public:

	using FCallbackInitFunc = TFunction<void(FStaticMeshSolverCallbacks::Params&)>;
	using FSyncDynamicFunc = TFunction<void(const FTransform&)>;

	FStaticMeshSimulationComponentPhysicsProxy(FCallbackInitFunc InInitFunc, FSyncDynamicFunc InSyncFunc);

	virtual void OnRemoveFromScene() override;
	virtual void CacheResults() override;
	virtual void FlipCache() override;
	virtual void SyncToCache() override;

private:

	virtual FSolverCallbacks* OnCreateCallbacks() override;
	virtual void OnDestroyCallbacks(FSolverCallbacks* InCallbacks) override;

	// Transform that the callback object will write into during simulation.
	// During sync this will be pushed back to the component
	FTransform SimTransform;

	// Double buffered result data
	Chaos::TBufferedData<FTransform> Results;

	// Callback object to handle simulation events and object creation
	FStaticMeshSolverCallbacks* Callbacks;

	/**
	 *	External functions for setup and sync, called on the game thread during callback creation and syncing
	 */

	FCallbackInitFunc InitialiseCallbackParamsFunc;
	FSyncDynamicFunc SyncDynamicTransformFunc;
};

#endif
