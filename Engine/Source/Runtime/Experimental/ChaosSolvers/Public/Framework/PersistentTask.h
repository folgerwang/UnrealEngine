// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#pragma once

#include "Async/AsyncWork.h"
#include "Containers/Queue.h"
#include "Dispatcher.h"
#include "HAL/ThreadSafeBool.h"

class FPhysScene_Chaos;
struct FSolverStateStorage;

namespace Chaos
{
	class PBDRigidsSolver;
	class FPhysicsProxy;
}

namespace Chaos
{

	class CHAOSSOLVERS_API FPersistentPhysicsTask : public FNonAbandonableTask
	{
		friend class FAsyncTask<FPersistentPhysicsTask>;

	public:
		FPersistentPhysicsTask(float InTargetDt, bool bInAvoidSpiral, Dispatcher<DispatcherMode::DedicatedThread>* InDispatcher);
		virtual ~FPersistentPhysicsTask();

		/**
		 * Entry point for the physics "thread". This function will never exit and act as a dedicated
		 * physics thread accepting commands from the game thread and running decoupled simulation
		 * iterations.
		 */
		void DoWork();

		/**
		 * Adds a solver to the internal list of solvers to run on the async task.
		 * Once the solver has been added to this task the game thread should never
		 * touch the internal state again unless performing a sync of the data
		 */
		void AddSolver(FSolverStateStorage* InSolverState);

		/**
		 * Removes a solver from the internal list of solvers to run on the async task
		 */
		void RemoveSolver(FSolverStateStorage* InSolverState);

		/**
		 *
		 */
		void AddProxy(FPhysicsProxy* InProxy);
		void RemoveProxy(FPhysicsProxy* InProxy);
		void AddFieldProxy(FPhysicsProxy* InProxy);
		void RemoveFieldProxy(FPhysicsProxy* InProxy);

		/**
		 * Synchronize proxies to their most recent gamethread readable results
		 * @param bFullSync Whether or not the physics thread has stalled. If it has then we can read from it here and
		 * perform some extra processing for removed objects
		 */
		void SyncProxiesFromCache(bool bFullSync = false);

		/**
		 * Request a shutdown of the current task. This will not happen immediately.
		 * Wait on the shutdown event (see GetShutdownEvent) to guarantee shutdown.
		 * Thread-safe, can be called from any thread to shut down the physics task
		 */
		void RequestShutdown();

		/**
		 * Get the shutdown event, which this task will trigger when the main
		 * running loop in DoWork is broken
		 */
		FEvent* GetShutdownEvent();

		/**
		 * Below functions alter the running task state and should be called using commands
		 * once the task is actually running
		 */

		 /**
		  * Sets the target per-tick Dt. Each physics update is always this length when running
		  * in fixed mode. The thread will stall after simulating if simulation takes less than
		  * this time. If it takes more than Dt seconds to do the simulation a warning is fired
		  * but the simulation will be running behind real-time
		  */
		void SetTargetDt(float InNewDt);

		/**
		 * Lock for handling caching for proxies. Read and write to either side of a double buffer counts
		 * as a read on this lock. It should only be write locked for flipping (happens after physics
		 * finishes a simulation)
		 */
		FRWLock CacheLock;

	private:

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPersistentPhysicsTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void HandleSolverCommands(PBDRigidsSolver* InSolver);
		void AdvanceSolver(PBDRigidsSolver* InSolver);

		FSolverStateStorage* GetSolverEntry(PBDRigidsSolver* InSolver);

		// Entries for each solver tracking the proxies currently registered to them
		TArray<FSolverStateStorage*> SolverEntries;

		// List of proxies that have been requested to be removed. Cached until the next
		// gamethread sync for final data handoff before being destroyed.
		TArray<FPhysicsProxy*> RemovedProxies;

		// Dt to run the simulation at when running a dedicated thread
		// #BG TODO Tick policies as this one gets bad if actual time > target time
		float TargetDt;

		// Whether the main physics loop is running in DoWork;
		FThreadSafeBool bRunning;

		// The dispatcher made by the Chaos module to enable the gamethread to communicate with this one.
		Dispatcher<DispatcherMode::DedicatedThread>* CommandDispatcher;

		// Event to fire after we've broken from the running physics loop as the thread shuts down
		FEvent* ShutdownEvent;
	};
}

#endif
