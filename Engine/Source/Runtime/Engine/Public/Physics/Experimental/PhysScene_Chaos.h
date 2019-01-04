// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if INCLUDE_CHAOS

#include "Tickable.h"
#include "Physics/PhysScene.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GCObject.h"

class AdvanceOneTimeStepTask;
class FPhysInterface_Chaos;
class FChaosSolversModule;
struct FForceFieldProxy;
struct FSolverStateStorage;

namespace Chaos
{
	class PBDRigidsSolver;
	class FPhysicsProxy;
	class IDispatcher;
}

/**
* Low level Chaos scene used when building custom simulations that don't exist in the main world physics scene.
*/
class ENGINE_API FPhysScene_Chaos : public FTickableGameObject, public FGCObject
{
public:

	FPhysScene_Chaos();
	virtual ~FPhysScene_Chaos();

	/* TEMPORARY - ATTACH TO WORLD */
	static TSharedPtr<FPhysScene_Chaos> GetInstance();

	// Begin FTickableGameObject implementation
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ChaosSolver, STATGROUP_Tickables); }
	// End FTickableGameObject

	/**
	 * Get the internal Chaos solver object
	 */
	Chaos::PBDRigidsSolver* GetSolver() const;

	/**
	 * Get the internal Dispatcher object
	 */
	Chaos::IDispatcher* GetDispatcher() const;

	/**
	 * Called during creation of the physics state for gamethread objects to pass off a proxy to the physics thread
	 */
	void AddProxy(Chaos::FPhysicsProxy* InProxy);
	void AddFieldProxy(Chaos::FPhysicsProxy* InProxy);

	/**
	 * Called during physics state destruction for the game thread to remove proxies from the simulation
	 * #BG TODO - Doesn't actually remove from the evolution at the moment
	 */
	void RemoveProxy(Chaos::FPhysicsProxy* InProxy);
	void RemoveFieldProxy(Chaos::FPhysicsProxy* InProxy);

	void Shutdown();

#if WITH_EDITOR
	void AddPieModifiedObject(UObject* InObj);
#endif

	// FGCObject Interface ///////////////////////////////////////////////////
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//////////////////////////////////////////////////////////////////////////
	
private:
	
#if WITH_EDITOR
	/**
	 * Callback when a world ends, to mark updated packages dirty. This can't be done in final
	 * sync as the editor will ignore packages being dirtied in PIE
	 */
	void OnWorldEndPlay();

	// List of objects that we modified during a PIE run for physics simulation caching.
	TArray<UObject*> PieModifiedObjects;
#endif

	// Control module for Chaos - cached to avoid constantly hitting the module manager
	FChaosSolversModule* ChaosModule;

	// Solver state we requested from the Chaos module. Thread safety depends on 
	// Chaos threading mode (dedicated thread can steal this)
	FSolverStateStorage* SolverStorage;
};
#endif
