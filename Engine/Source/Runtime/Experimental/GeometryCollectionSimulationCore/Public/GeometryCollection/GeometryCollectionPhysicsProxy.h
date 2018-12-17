// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#if INCLUDE_CHAOS

#include "Framework/PhysicsProxy.h"
#include "Framework/BufferedData.h"
#include "PBDRigidsSolver.h"

#include "GeometryCollection/ManagedArray.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "GeometryCollection/RecordedTransformTrack.h"

#include "Field/FieldSystem.h"

class UFieldSystem;
class FGeometryCollectionSolverCallbacks;
class FGeometryCollection;

struct FGeometryCollectionResults
{
	FGeometryCollectionResults();

	TSharedPtr<TManagedArray<FTransform>> Transforms;
	TSharedPtr<TManagedArray<int32>> RigidBodyIds;
	TSharedPtr<TManagedArray<FGeometryCollectionBoneNode>> BoneHierarchy;
};

class GEOMETRYCOLLECTIONSIMULATIONCORE_API FGeometryCollectionPhysicsProxy : public Chaos::FPhysicsProxy
{
public:

	using FInitFunc = TFunction<void(FSimulationParameters&, FFieldSystem&)>;
	using FCacheSyncFunc = TFunction<void(const TManagedArray<int32>&)>;
	using FFinalSyncFunc = TFunction<void(const FRecordedTransformTrack&)>;

	FGeometryCollectionPhysicsProxy(FGeometryCollection* InDynamicCollection, FInitFunc InInitFunc, FCacheSyncFunc InCacheSyncFunc, FFinalSyncFunc InFinalSyncFunc);

	// FPhysicsProxy interface
	virtual void SyncBeforeDestroy() override;
	virtual void OnRemoveFromScene() override;

	virtual void CacheResults() override;
	virtual void FlipCache() override;
	virtual void SyncToCache() override;
	//////////////////////////////////////////////////////////////////////////

	void MergeRecordedTracks(const FRecordedTransformTrack& A, const FRecordedTransformTrack& B, FRecordedTransformTrack& Target);
	FRecordedFrame& InsertRecordedFrame(FRecordedTransformTrack& InTrack, float InTime);

private:

	virtual FSolverCallbacks* OnCreateCallbacks() override;
	virtual void OnDestroyCallbacks(FSolverCallbacks* InCallbacks) override;
	
	void UpdateRecordedState(float SolverTime, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles, const FSolverCallbacks::FCollisionConstraintsType& CollisionRule);

	// Duplicated dynamic collection for use on the physics thread, copied to the game thread on sync
	FGeometryCollection* SimulationCollection;

	// Dynamic collection on the game thread - used to populate the simulated collection
	FGeometryCollection* GTDynamicCollection;

	// Callbacks created by this proxy to interface with the solver
	FGeometryCollectionSolverCallbacks* Callbacks;

	// Storage for the recorded frame information when we're caching the geometry component results.
	// Synced back to the component with SyncBeforeDestroy
	FRecordedTransformTrack RecordedTracks;

	// Duplicated field system from the game thread
	// #BG TODO When we get rid of the global physics scene these should be stored mapped to the world/solver/scene
	// and referenced here by index instead of duplicated per-component
	FFieldSystem FieldSystem;

	// Functions to handle engine-side events
	FInitFunc InitFunc;
	FCacheSyncFunc CacheSyncFunc;
	FFinalSyncFunc FinalSyncFunc;

	// Sync frame numbers so we don't do many syncs when physics is running behind
	uint32 LastSyncCountGT;

	// Double buffer of geom collection result data
	Chaos::TBufferedData<FGeometryCollectionResults> Results;
}; 

#endif
