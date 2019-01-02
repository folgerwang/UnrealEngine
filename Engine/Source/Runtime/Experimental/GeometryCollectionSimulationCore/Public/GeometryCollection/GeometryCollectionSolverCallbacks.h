// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionCollisionStructureManager.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Field/FieldSystem.h"

class UGeometryCollection;
class FGeometryCollectionPhysicsProxy;

#if INCLUDE_CHAOS
#include "PBDRigidsSolver.h"

class GEOMETRYCOLLECTIONSIMULATIONCORE_API FGeometryCollectionSolverCallbacks : public FSolverCallbacks
{
public:

	FGeometryCollectionSolverCallbacks( );
	virtual ~FGeometryCollectionSolverCallbacks( ) { }

	/**/
	static int8 Invalid;

	/**/
	void Initialize();

	/**/
	void Reset();

	/**/
	virtual bool IsSimulating() const override;

	/**/
	virtual void UpdateKinematicBodiesCallback(const FSolverCallbacks::FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy) override;

	/**/
	virtual void StartFrameCallback(const float Dt, const float Time) override;

	/**/
	virtual void EndFrameCallback(const float EndFrame) override;

	/**/
	virtual void CreateRigidBodyCallback(FSolverCallbacks::FParticlesType& Particles) override;

	/**/
	virtual void BindParticleCallbackMapping(const int32 & CallbackIndex, FSolverCallbacks::IntArray & ParticleCallbackMap);

	/**/
	virtual void ParameterUpdateCallback(FSolverCallbacks::FParticlesType& Particles, const float Time) override;

	/**/
	virtual void DisableCollisionsCallback(TSet<TTuple<int32, int32>>& CollisionPairs) override;

	/**/
	virtual void AddConstraintCallback(FSolverCallbacks::FParticlesType& Particles, const float Time, const int32 Island) override;

	/**/
	virtual void AddForceCallback(FSolverCallbacks::FParticlesType& Particles, const float Dt, const int32 Index) override;

	/**/
	void InitializeClustering(uint32 ParentIndex, FParticlesType& Particles);

	/**/
	void BuildClusters(uint32 CollectionClusterIndex, const TArray<uint32>& CollectionChildIDs, const TArray<uint32>& ChildIDs);

	/**/
	const TManagedArray<int32>& GetRigidBodyIdArray() const { return *RigidBodyIdArray; }

	/**/
	void UpdateParameters(const FSimulationParameters & ParametersIn)
	{
		Parameters = ParametersIn;
	}

	const FSimulationParameters& GetParameters()
	{
		return Parameters;
	}

	void SetResetAnimationCacheFunction(TFunction<void(void)> ResetAnimationCacheCallbackIn) { ResetAnimationCacheCallback = ResetAnimationCacheCallbackIn; }
	void SetUpdateTransformsFunction(TFunction<void(const TArrayView<FTransform> & )> UpdateTransformsCallbackIn) { UpdateTransformsCallback = UpdateTransformsCallbackIn; }
	void SetUpdateRestStateFunction(TFunction<void(const int32 & CurrentFrame, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles)> UpdateRestStateCallbackIn) { UpdateRestStateCallback = UpdateRestStateCallbackIn; }
	void SetUpdateRecordedStateFunction(TFunction<void(float SolverTime, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles, const FSolverCallbacks::FCollisionConstraintsType& CollisionRule)> InCallable) { UpdateRecordedStateCallback = InCallable; }
	void SetCommitRecordedStateFunction(TFunction<void(FRecordedTransformTrack& InTrack)> InCallable) { CommitRecordedStateCallback = InCallable; }

	int32 GetBaseParticleIndex() const
	{
		return BaseParticleIndex;
	}

	int32 GetNumParticles() const
	{
		return NumParticles;
	}

protected:
	void CreateDynamicAttributes();

	void IdentifySimulatableElements();

	void InitializeCollisionStructures();

private:
	bool InitializedState;
	TSharedRef< TManagedArray<FTransform> > LocalToMassArray;
	TSharedRef< TManagedArray<int32> > CollisionMaskArray;
	TSharedRef< TManagedArray<int32> > CollisionStructureIDArray;
	TSharedRef< TManagedArray<int32> > DynamicStateArray;
	TSharedRef< TManagedArray<int32> > RigidBodyIdArray;
	TSharedRef< TManagedArray<int32> > SolverClusterIDArray;
	TSharedRef< TManagedArray<bool> > SimulatableParticlesArray;
	TSharedRef< TManagedArray<float> > VolumeArray;

	int32 StayDynamicFieldIndex;
	FSimulationParameters Parameters;
	FCollisionStructureManager CollisionStructures;

	TFunction<void(void)> ResetAnimationCacheCallback;
	TFunction<void(const TArrayView<FTransform> &)> UpdateTransformsCallback;
	TFunction<void(const int32 & CurrentFrame, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles)> UpdateRestStateCallback;
	TFunction<void(float SolverTime, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles, const FSolverCallbacks::FCollisionConstraintsType& CollisionRule)> UpdateRecordedStateCallback;
	TFunction<void(FRecordedTransformTrack& InTrack)> CommitRecordedStateCallback;

	int32 BaseParticleIndex;
	int32 NumParticles;

	float ProxySimDuration;
};
#else
// Stub solver callbacks for non Chaos builds. 
class GEOMETRYCOLLECTIONSIMULATIONCORE_API FGeometryCollectionSolverCallbacks
{
public:
	FGeometryCollectionSolverCallbacks();

	TSharedRef< TManagedArray<int32> > RigidBodyIdArray;
	const TManagedArray<int32>& GetRigidBodyIdArray() const { return *RigidBodyIdArray; }

};
#endif
