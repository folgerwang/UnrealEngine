// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#if INCLUDE_CHAOS
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Defines.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/Transform.h"
#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Framework/Dispatcher.h"
#include "Field/FieldSystem.h"

#define USE_PGS 0

#if USE_PGS
typedef Chaos::TPBDRigidsEvolutionPGS<float, 3> FPBDRigidsEvolution;
#else
typedef Chaos::TPBDRigidsEvolutionGBF<float, 3> FPBDRigidsEvolution;
#endif

class FPhysInterface_Chaos;
namespace Chaos
{
	class PBDRigidsSolver;
	class AdvanceOneTimeStepTask;
	class FPersistentPhysicsTask;
	class FPhysicsCommand;
};

struct FKinematicProxy
{
	TArray<int32> Ids;
	TArray<FVector> Position;
	TArray<FQuat> Rotation;
	TArray<FVector> NextPosition;
	TArray<FQuat> NextRotation;
};

/**
*
*/
class CHAOSSOLVERS_API FSolverCallbacks
{
public:
	typedef Chaos::TPBDRigidParticles<float, 3> FParticlesType;
	typedef Chaos::TPBDCollisionConstraint<float, 3> FCollisionConstraintsType;
	typedef Chaos::TArrayCollectionArray<int32> IntArray;

	FSolverCallbacks()
		: Solver(nullptr)
	{}

	virtual ~FSolverCallbacks() {}

	virtual bool IsSimulating() const { return true; }
	virtual void UpdateKinematicBodiesCallback(const FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Index) {}
	virtual void StartFrameCallback(const float, const float) {}
	virtual void EndFrameCallback(const float) {}
	virtual void CreateRigidBodyCallback(FParticlesType&) {}
	virtual void BindParticleCallbackMapping(const int32 & CallbackIndex, FSolverCallbacks::IntArray & ParticleCallbackMap) {}
	virtual void ParameterUpdateCallback(FParticlesType&, const float) {}
	virtual void DisableCollisionsCallback(TSet<TTuple<int32, int32>>&) {}
	virtual void AddConstraintCallback(FParticlesType&, const float, const int32) {}
	virtual void AddForceCallback(FParticlesType&, const float, const int32) {}
	virtual void CollisionContactsCallback(FParticlesType&, FCollisionConstraintsType&) {}
	virtual void BreakingCallback(FParticlesType&) {}
	virtual void TrailingCallback(FParticlesType&) {}

	void SetSolver(Chaos::PBDRigidsSolver * SolverIn) { Solver = SolverIn; }
	Chaos::PBDRigidsSolver* GetSolver() { check(Solver);  return Solver; }

private:
	Chaos::PBDRigidsSolver * Solver;
};


class CHAOSSOLVERS_API FSolverFieldCallbacks : public FSolverCallbacks
{
public:
	FSolverFieldCallbacks() {}

	FSolverFieldCallbacks(const FFieldSystem & System)
		: FSolverCallbacks()
	{
		FieldSystem.BuildFrom(System);
	}

	virtual ~FSolverFieldCallbacks() {}

	virtual void CommandUpdateCallback(FParticlesType&, Chaos::TArrayCollectionArray<FVector> &, const float) {}

protected:

	FFieldSystem FieldSystem;
	TArray<FFieldSystemCommand> FieldCommands;
};

/**
*
*/
namespace Chaos
{

	/**
	*
	*/
	class CHAOSSOLVERS_API PBDRigidsSolver
	{
	public:
		friend class Chaos::AdvanceOneTimeStepTask;
		friend class Chaos::FPersistentPhysicsTask;

		template<Chaos::DispatcherMode Mode>
		friend class Chaos::Dispatcher;

		friend class FPhysInterface_Chaos;
		static int8 Invalid;
		typedef TPBDRigidParticles<float, 3> FParticlesType;
		typedef TPBDCollisionConstraint<float, 3> FCollisionConstraintsType;
		typedef TArray<TCollisionData<float, 3>> FCollisionDataArray;
		struct FCollisionData
		{
			float TimeCreated;
			int32 NumCollisions;
			FCollisionDataArray CollisionDataArray;
		};
		typedef TArray<TBreakingData<float, 3>> FBreakingDataArray;
		struct FBreakingData
		{
			float TimeCreated;
			int32 NumBreakings;
			FBreakingDataArray BreakingDataArray;
		};
		typedef TSet<TTrailingData<float, 3>> FTrailingDataSet;
		struct FTrailingData
		{
			float TimeLastUpdated;
			FTrailingDataSet TrailingDataSet;
		};

		PBDRigidsSolver();

		/* Object Callbacks Registration and Management*/
		void RegisterCallbacks(FSolverCallbacks* Callbacks);
		void UnregisterCallbacks(FSolverCallbacks* Callbacks);			
		const TArray<FSolverCallbacks*>& GetCallbacks() const { return Callbacks; }

		/* Field Callbacks Registration and Management*/
		void RegisterFieldCallbacks(FSolverFieldCallbacks* Callbacks);
		void UnregisterFieldCallbacks(FSolverFieldCallbacks* Callbacks);
		const TArray<FSolverFieldCallbacks*>& GetFieldCallbacks() const { return FieldCallbacks; }

		void ClearCallbacks();


		/**/
		void Reset();

		/**/
		void AdvanceSolverBy(float DeltaTime);

		/* Particle Update and access*/
		void InitializeFromParticleData() { MEvolution->InitializeFromParticleData(); }
		const FParticlesType& GetRigidParticles() const { return MEvolution->Particles(); }
		FParticlesType& GetRigidParticles() { return MEvolution->Particles(); }

		FCollisionConstraintsType& GetCollisionRule() { return MEvolution->MCollisionRule; }
		FCollisionData& GetCollisionData() { return CollisionData; }
		FBreakingData& GetBreakingData() { return BreakingData; }
		FTrailingData& GetTrailingData() { return TrailingData; }

		const TSet<TTuple<int32, int32>>& GetDisabledCollisionPairs() const { return MEvolution->DisabledCollisions(); }

		/**/
		void SetCurrentFrame(const int32 CurrentFrameIn) { CurrentFrame = CurrentFrameIn; }
		int32 GetCurrentFrame() { return CurrentFrame; }

		bool Enabled() const;
		void SetEnabled(bool bEnabledIn) { bEnabled = bEnabledIn; }

		/* Clustering Access */
		const TArrayCollectionArray<ClusterId> & ClusterIds() const { return MEvolution->ClusterIds(); }
		const TArrayCollectionArray<TRigidTransform<float,3>>& ClusterChildToParentMap() const { return MEvolution->ClusterChildToParentMap(); }
		const TArrayCollectionArray<bool>& ClusterInternalCluster() const { return MEvolution->ClusterInternalCluster(); }
		int32 CreateClusterParticle(const TArray<uint32>& Children) { return MEvolution->CreateClusterParticle(Children); }
		TSet<uint32> DeactivateClusterParticle(const uint32 ClusterIndex) { return MEvolution->DeactivateClusterParticle(ClusterIndex); }
		void SetClusterStrain(const uint32 ClusterId, float Strain) { MEvolution->Strain(ClusterId) = Strain; }

		/**/
		const FCollisionData GetCollisionDataArray() { return CollisionData; }
		const FBreakingData GetBreakingDataArray() { return BreakingData; }
		const FTrailingData GetTrailingDataArray() { return TrailingData; }
		const float GetSolverTime() { return MTime; }
		const float GetLastDt() { return MLastDt; }
		const int32 GetMaxCollisionDataSize() { return MaxCollisionDataSize; }
		const float GetCollisionDataTimeWindow() { return CollisionDataTimeWindow; }
		const int32 GetMaxBreakingDataSize() { return MaxBreakingDataSize; }
		const float GetBreakingDataTimeWindow() { return BreakingDataTimeWindow; }
		const int32 GetMaxTrailingDataSize() { return MaxTrailingDataSize; }
		const float GetTrailingDataTimeWindow() { return TrailingDataTimeWindow; }

		/**/
		void SetTimeStepMultiplier(float TimeStepMultiplierIn) { ensure(TimeStepMultiplierIn > 0); TimeStepMultiplier = TimeStepMultiplierIn; }
		void SetHasFloor(bool bHasFloorIn) { bHasFloor = bHasFloorIn; }
		void SetIsFloorAnalytic(bool bIsAnalytic) { bIsFloorAnalytic = bIsAnalytic; }
		void SetFriction(float Friction) { MEvolution->SetFriction(Friction); }
		void SetRestitution(float Restitution) { MEvolution->SetRestitution(Restitution); }
		void SetSleepThresholds(const float LinearThreshold, const float AngularThreshold) { MEvolution->SetSleepThresholds(LinearThreshold, AngularThreshold); }
		void SetIterations(int32 Iterations) { MEvolution->SetIterations(Iterations); }
		void SetPushOutIterations(int32 PushOutIterations) { MEvolution->SetPushOutIterations(PushOutIterations); }
		void SetPushOutPairIterations(int32 PushOutPairIterations) { MEvolution->SetPushOutPairIterations(PushOutPairIterations); }
		void SetMaxCollisionDataSize(int32 MaxDataSize) { MaxCollisionDataSize = MaxDataSize; }
		void SetCollisionDataTimeWindow(float TimeWindow) { CollisionDataTimeWindow = TimeWindow; }
		void SetDoCollisionDataSpatialHash(bool DoSpatialHash) { DoCollisionDataSpatialHash = DoSpatialHash; }
		void SetCollisionDataSpatialHashRadius(float Radius) { CollisionDataSpatialHashRadius = Radius; }
		void SetMaxCollisionPerCell(int32 MaxCollision) { MaxCollisionPerCell = MaxCollision; }
		void SetMaxBreakingDataSize(int32 MaxDataSize) { MaxBreakingDataSize = MaxDataSize; }
		void SetBreakingDataTimeWindow(float TimeWindow) { BreakingDataTimeWindow = TimeWindow; }
		void SetDoBreakingDataSpatialHash(bool DoSpatialHash) { DoBreakingDataSpatialHash = DoSpatialHash; }
		void SetBreakingDataSpatialHashRadius(float Radius) { BreakingDataSpatialHashRadius = Radius; }
		void SetMaxBreakingPerCell(int32 MaxBreaking) { MaxBreakingPerCell = MaxBreaking; }
		void SetMaxTrailingDataSize(int32 MaxDataSize) { MaxTrailingDataSize = MaxDataSize; }
		void SetTrailingDataTimeWindow(float TimeWindow) { TrailingDataTimeWindow = TimeWindow; }
		void SetTrailingMinSpeedThreshold(float Threshold) { TrailingMinSpeedThreshold = Threshold; }
		void SetTrailingMinVolumeThreshold(float Threshold) { TrailingMinVolumeThreshold = Threshold; }
		void SetFloorHeight(float Height) { FloorHeight = Height; }

		TQueue<TFunction<void(PBDRigidsSolver*)>, EQueueMode::Mpsc>& GetCommandQueue() { return CommandQueue; }

	protected:
		/**/
		void CreateRigidBodyCallback(FParticlesType& Particles);

		/**/
		void ParameterUpdateCallback(FParticlesType& Particles, const float Time);

		/**/
		void ForceUpdateCallback(FParticlesType& Particles, const float Time);

		/**/
		void DisableCollisionsCallback(TSet<TTuple<int32, int32>>& CollisionPairs);

		/**/
		void StartFrameCallback(const float Dt, const float Time);

		/**/
		void EndFrameCallback(const float EndFrame);

		/**/
		void KinematicUpdateCallback(FParticlesType& Particles, const float Dt, const float Time);

		/**/
		void AddConstraintCallback(FParticlesType& Particles, const float Time, const int32 Island);

		/**/
		void AddForceCallback(FParticlesType& Particles, const float Time, const int32 Index);

		/**/
		void CollisionContactsCallback(FParticlesType& Particles, FCollisionConstraintsType& CollisionConstraints);

		/**/
		void BreakingCallback(FParticlesType& Particles);

		/**/
		void TrailingCallback(FParticlesType& Particles);

	private:

		int32 CurrentFrame;
		float MTime;
		float MLastDt;
		float MMaxDeltaTime;
		float TimeStepMultiplier;

		bool bEnabled;
		bool bHasFloor;
		bool bIsFloorAnalytic;
		float FloorHeight;

		int32 MaxCollisionDataSize;
		float CollisionDataTimeWindow;
		bool DoCollisionDataSpatialHash;
		float CollisionDataSpatialHashRadius;
		int32 MaxCollisionPerCell;

		int32 MaxBreakingDataSize;
		float BreakingDataTimeWindow;
		bool DoBreakingDataSpatialHash;
		float BreakingDataSpatialHashRadius;
		int32 MaxBreakingPerCell;

		int32 MaxTrailingDataSize;
		float TrailingDataTimeWindow;
		float TrailingMinSpeedThreshold;
		float TrailingMinVolumeThreshold;

		TSharedPtr<FEvent> MCurrentEvent;
		TSharedPtr<FCriticalSection> MCurrentLock;

		TUniquePtr<FPBDRigidsEvolution> MEvolution;
		TArray<FSolverCallbacks*> Callbacks;
		TArray<FSolverFieldCallbacks*> FieldCallbacks;

		int32 FieldForceNum;
		Chaos::TArrayCollectionArray<FVector> FieldForce;
		TArray<FKinematicProxy> KinematicProxies;

		FSolverCallbacks::IntArray ParticleCallbackMapping;

		FCollisionData CollisionData;
		FBreakingData BreakingData;
		FTrailingData TrailingData;

		TQueue<TFunction<void(PBDRigidsSolver*)>, EQueueMode::Mpsc> CommandQueue;
	};
}

#endif
