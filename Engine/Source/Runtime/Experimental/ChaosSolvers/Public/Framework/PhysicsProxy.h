// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#pragma once

#include "CoreMinimal.h"

class FSolverCallbacks;

namespace Chaos
{
	class PBDRigidsSolver;
}

/**
 * Proxy class for physics objects that will be requested when using multi-threaded
 * physics. Analogous to the scene proxy created for rendering. The component will
 * create a proxy that will be dispatched to the physics system to manage. Components
 * wishing to have concurrent physics state should have a derived physics proxy with
 * an appropriate implementation for their use case.
 * If a component does not create a proxy when requested then the FBodyInstance
 * inside the component will be responsible for the physics representation.
 */
namespace Chaos
{
	class CHAOSSOLVERS_API FPhysicsProxy
	{
	public:
		friend class FPersistentPhysicsTask;

		FPhysicsProxy();
		virtual ~FPhysicsProxy() {}


		/**
		 * Gets/Creates the solver callbacks for this object. Derived classes
		 * should override OnCreateCallbacks to handle callback creation when
		 * requested
		 */
		FSolverCallbacks* GetCallbacks();
		void DestroyCallbacks();

		/**
		 * The scene will call this during setup to populate the solver so it's available
		 * to the proxy
		 */
		void SetSolver(PBDRigidsSolver* InSolver);
		PBDRigidsSolver* GetSolver()
		{
			return Solver;
		}

		/**
		 * Utility to find out whether the system is running in a multithreaded
		 * context. Useful for skipping data duplication in single-threaded contexts
		 */
		bool IsMultithreaded() const;

		/**
		 * CONTEXT: GAMETHREAD
		 * Called during the gamethread sync after the proxy has been removed from its solver
		 * intended for final handoff of any data the proxy has that the gamethread may
		 * be interested in
		 */
		virtual void SyncBeforeDestroy() {};

		/**
		 * CONTEXT: PHYSICSTHREAD
		 * Called on the physics thread when the engine is shutting down the proxy and we need to remove it from
		 * any active simulations. Proxies are expected to entirely clean up their simulation
		 * state within this method. This is run in the task command step by the scene
		 * so the simulation will currently be idle
		 */
		virtual void OnRemoveFromScene() = 0;

		/**
		 * CONTEXT: PHYSICSTHREAD
		 * Called per-tick after the simulation has completed. The proxy should cache the results of their
		 */
		virtual void CacheResults() = 0;

		/**
		 * CONTEXT: PHYSICSTHREAD (Write Locked)
		 * Called by the physics thread to signal that it is safe to perform any double-buffer flips here.
		 * The physics thread has pre-locked an RW lock for this operation so the game thread won't be reading
		 * the data
		 */
		virtual void FlipCache() = 0;

		/**
		 * CONTEXT: GAMETHREAD (Read Locked)
		 * Perform a similar operation to Sync, but take the data from a gamethread-safe cache. This will be called
		 * from the game thread when it cannot sync to the physics thread. The simulation is very likely to be running
		 * when this happens so never read any physics thread data here!
		 *
		 * Note: A read lock will have been aquired for this - so the physics thread won't force a buffer flip while this
		 * sync is ongoing
		 */
		virtual void SyncToCache() = 0;

	private:

		// Internal versions of the callback creation. Derived classes should override these and
		// provide the solver with a callback object.
		virtual FSolverCallbacks* OnCreateCallbacks() = 0;
		virtual void OnDestroyCallbacks(FSolverCallbacks* InCallbacks) = 0;

		// The solver that owns this proxy
		PBDRigidsSolver* Solver;

		// The solver callbacks object for this proxy
		FSolverCallbacks* Callbacks;
	};
}

#endif
