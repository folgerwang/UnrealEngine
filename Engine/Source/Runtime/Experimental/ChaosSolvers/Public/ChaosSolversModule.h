// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#pragma once

#include "CoreMinimal.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Async/AsyncWork.h"

#include "Framework/Dispatcher.h"

namespace Chaos
{
	class FPersistentPhysicsTask;
	class FPhysicsProxy;
	class PBDRigidsSolver;
}

struct FChaosConsoleSinks
{
	static void OnCVarsChanged();
};

struct FSolverStateStorage
{
	friend class FChaosSolversModule;

	Chaos::PBDRigidsSolver* Solver;
	TArray<Chaos::FPhysicsProxy*> ActiveProxies;

private:

	// Private so only the module can actually make these so they can be tracked
	FSolverStateStorage();
	FSolverStateStorage(const FSolverStateStorage& InCopy) = default;
	FSolverStateStorage(FSolverStateStorage&& InSteal) = default;
	FSolverStateStorage& operator =(const FSolverStateStorage& InCopy) = default;
	FSolverStateStorage& operator =(FSolverStateStorage&& InSteal) = default;

};

class CHAOSSOLVERS_API FChaosSolversModule : public IModuleInterface
{
public:

	static FChaosSolversModule* GetModule();

	FChaosSolversModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Queries for multithreaded configurations
	 */
	bool IsPersistentTaskEnabled() const;
	bool IsPersistentTaskRunning() const;

	/**
	 * Creates and dispatches the physics thread task
	 */
	void StartPhysicsTask();

	/**
	 * Shuts down the physics thread task
	 * #BG TODO cleanup running task
	 */
	void EndPhysicsTask();

	/**
	 * Get the dispatcher interface currently being used. when running a multi threaded
	 * configuration this will safely marshal commands to the physics thread. in a
	 * single threaded configuration the commands will be called immediately
	 *
	 * Note: This should be queried for every scope that dispatches commands. the game
	 * thread has mechanisms to change the dispatcher implementation (CVar for threadmode)
	 * which means the ptr could be stale
	 * #BGallagher Make this pimpl? Swap out implementation and allow cached dispatcher?
	 */
	Chaos::IDispatcher* GetDispatcher() const;

	/**
	 * Gets an existing, idle dedicated physics task. If the task is currently
	 * running this will fail
	 */
	Chaos::FPersistentPhysicsTask* GetDedicatedTask() const;

	/**
	 * Called to request a sync between the game thread and the currently running physics task
	 * @param bForceBlockingSync forces this 
	 */
	void SyncTask(bool bForceBlockingSync = false);

	/**
	 * Create a new solver state storage object to contain a solver and proxy storage object. Intended
	 * to be used by the physics scene to create a common storage object that can be passed to a dedicated
	 * thread when it is enabled without having to link Engine from Chaos.
	 *
	 * Should be called from the game thread to create a new solver. Then passed to the physics thread
	 * if it exists after it has been initialized with a solver
	 */
	FSolverStateStorage* CreateSolverState();

	/**
	 * Shuts down and destroys a solver state
	 *
	 * Should be called on whichever thread currently owns the solver state
	 */
	void DestroySolverState(FSolverStateStorage* InState);

	/**
	 * Read access to the current solver-state objects, be aware which thread owns this data when
	 * attempting to use this. Physics thread will query when spinning up to get current world state
	 */
	const TArray<FSolverStateStorage*>& GetSolverStorage() const;

private:

	// Whether we actually spawned a physics task (distinct from whether we _should_ spawn it)
	bool bPersistentTaskSpawned;

	// The actually running tasks if running in a multi threaded configuration.
	FAsyncTask<Chaos::FPersistentPhysicsTask>* PhysicsAsyncTask;
	Chaos::FPersistentPhysicsTask* PhysicsInnerTask;

	// Current command dispatcher
	Chaos::IDispatcher* Dispatcher;

	// Core delegate signaling app shutdown, clean up and spin down threads before exit.
	FDelegateHandle PreExitHandle;

	// Allocated storage for solvers and proxies. Existing on the module makes it easier for hand off in multi threaded mode.
	// To actually use a solver, call CreateSolverState to receive one of these and use it to hold the solver. In the event
	// of switching to multi threaded mode these will be handed over to the other thread.
	//
	// Where these objects are valid for interaction depends on the current threading mode. Use IsPersistentTaskRunning to
	// check whether the physics thread owns these before manipulating. When adding/removing solver or proxy items in
	// multi threaded mode the physics thread must also be notified of the change.
	TArray<FSolverStateStorage*> SolverStorage;
};

/**
 * Scoped locking object for physics thread. Currently this will stall out the persistent
 * physics task if it is running. Use this in situations where another thread absolutely
 * must read or write.
 *
 * Will block on construction until the physics thread confirms it has stalled, then
 * the constructor returns. Will let the physics thread continue post-destruction
 *
 * Does a runtime check on the type of the dispatcher and will do nothing if we're
 * not running the dedicated thread mode.
 */
class CHAOSSOLVERS_API FChaosScopedPhysicsThreadLock
{
public:

	FChaosScopedPhysicsThreadLock();
	explicit FChaosScopedPhysicsThreadLock(uint32 InMsToWait);
	~FChaosScopedPhysicsThreadLock();

	bool DidGetLock() const;

private:

	// Only construction through the above constructor is valid
	FChaosScopedPhysicsThreadLock(const FChaosScopedPhysicsThreadLock& InCopy) = default;
	FChaosScopedPhysicsThreadLock(FChaosScopedPhysicsThreadLock&& InSteal) = default;
	FChaosScopedPhysicsThreadLock& operator=(const FChaosScopedPhysicsThreadLock& InCopy) = default;
	FChaosScopedPhysicsThreadLock& operator=(FChaosScopedPhysicsThreadLock&& InSteal) = default;

	FEvent* CompleteEvent;
	FEvent* PTStallEvent;
	FChaosSolversModule* Module;
	bool bGotLock;
};

#endif