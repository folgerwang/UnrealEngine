// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDCollisionConstraint.h"
#include "Chaos/PBDCollisionConstraintPGS.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Transform.h"
#include "HAL/Event.h"

namespace Chaos
{
template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
class TPBDRigidsEvolutionBase
{
  public:
	// TODO(mlentine): Init particles from some type of input
	CHAOS_API TPBDRigidsEvolutionBase(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations = 1);
	CHAOS_API ~TPBDRigidsEvolutionBase() {}

	CHAOS_API inline void EnableDebugMode()
	{
		check(IsInGameThread());
		MDebugMode = true;
		MDebugLock.Lock();
	}

	CHAOS_API inline void InitializeFromParticleData()
	{
		MActiveIndices.Reset();
		for (uint32 i = 0; i < MParticles.Size(); ++i)
		{
			if (MParticles.Sleeping(i) || MParticles.Disabled(i))
				continue;
			MActiveIndices.Add(i);
		}
	}

	CHAOS_API void WakeIsland(const int32 i)
	{
		for (const auto& Index : MIslandParticles[i])
		{
			MParticles.SetSleeping(Index, false);
			IslandSleepCounts[i] = 0;
		}
	}

	CHAOS_API void ReconcileIslands()
	{
		for (int32 i = 0; i < MIslandParticles.Num(); ++i)
		{
			bool IsSleeping = true;
			bool IsSet = false;
			for (const auto& Index : MIslandParticles[i])
			{
				if (!IsSet)
				{
					IsSet = true;
					IsSleeping = MParticles.Sleeping(Index);
				}
				if (MParticles.Sleeping(Index) != IsSleeping)
				{
					WakeIsland(i);
					break;
				}
			}
		}
	}

	//CHAOS_API void SetKinematicUpdateFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T, const T, const int32)> KinematicUpdate) { MKinematicUpdate = KinematicUpdate; }
	CHAOS_API void SetParticleUpdateVelocityFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T, const TArray<int32>& ActiveIndices)> ParticleUpdate) { MParticleUpdateVelocity = ParticleUpdate; }
	CHAOS_API void SetParticleUpdatePositionFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T)> ParticleUpdate) { MParticleUpdatePosition = ParticleUpdate; }
	CHAOS_API void AddPBDConstraintFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)> ConstraintFunction) { MConstraintRules.Add(ConstraintFunction); }
	CHAOS_API void AddForceFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)> ForceFunction) { MForceRules.Add(ForceFunction); }
	CHAOS_API void SetCollisionContactsFunction(TFunction<void(TPBDRigidParticles<T, d>&, TPBDCollisionConstraint<T, d>&)> CollisionContacts) { MCollisionContacts = CollisionContacts; }
	CHAOS_API void SetBreakingFunction(TFunction<void(TPBDRigidParticles<T, d>&)> Breaking) { MBreaking = Breaking; }
	CHAOS_API void SetTrailingFunction(TFunction<void(TPBDRigidParticles<T, d>&)> Trailing) { MTrailing = Trailing; }

	/**/
	CHAOS_API TPBDRigidParticles<T, d>& Particles() { return MParticles; }
	CHAOS_API const TPBDRigidParticles<T, d>& Particles() const { return MParticles; }

	/**/
	CHAOS_API TArray<TSet<int32>>& IslandParticles() { return MIslandParticles; }
	CHAOS_API const TArray<TSet<int32>>& IslandParticles() const { return MIslandParticles; }

	/**/
	CHAOS_API TArray<int32>& ActiveIndicesArray() { return MActiveIndicesArray; }
	CHAOS_API const TArray<int32>& ActiveIndicesArray() const { return MActiveIndicesArray; }

	/**/
	CHAOS_API TSet<int32>& ActiveIndices() { return MActiveIndices; }
	CHAOS_API const TSet<int32>& ActiveIndices() const { return MActiveIndices; }

	/**/
	CHAOS_API TSet<TTuple<int32, int32>>& DisabledCollisions() { return MDisabledCollisions; }
	CHAOS_API const TSet<TTuple<int32, int32>>& DisabledCollisions() const { return MDisabledCollisions; }

	/**/
	CHAOS_API inline const TArrayCollectionArray<ClusterId> & ClusterIds() const { return MClustering.ClusterIds(); }
	CHAOS_API inline const TArrayCollectionArray<TRigidTransform<T, d>>& ClusterChildToParentMap() const{ return MClustering.ChildToParentMap(); }
	CHAOS_API inline const TArrayCollectionArray<bool>& ClusterInternalCluster() const { return MClustering.InternalCluster(); }
	CHAOS_API inline int32 CreateClusterParticle(const TArray<uint32>& Children) { return MClustering.CreateClusterParticle(Children); }
	CHAOS_API inline TSet<uint32> DeactivateClusterParticle(const uint32 ClusterIndex) { return MClustering.DeactivateClusterParticle(ClusterIndex); }
	CHAOS_API inline const T Strain(const uint32 Index) const { return MClustering.Strain(Index); }
	CHAOS_API inline T& Strain(const uint32 Index) { return MClustering.Strain(Index); }

	/**/
	CHAOS_API inline void SetFriction(T Friction ) { MFriction = Friction; }
	CHAOS_API inline void SetRestitution(T Restitution) { MRestitution = Restitution; }
	CHAOS_API inline void SetIterations(int32 Iterations) { MNumIterations = Iterations; }
	CHAOS_API inline void SetPushOutIterations(int32 PushOutIterations) { MPushOutIterations = PushOutIterations; }
	CHAOS_API inline void SetPushOutPairIterations(int32 PushOutPairIterations) { MPushOutPairIterations = PushOutPairIterations; }
	CHAOS_API inline void SetSleepThresholds(const T LinearThrehsold, const T AngularThreshold)
	{
		SleepLinearThreshold = LinearThrehsold;
		SleepAngularThreshold = AngularThreshold;
	}

  protected:
	inline void AddSubstep()
	{
		if (!MDebugMode)
			return;
		check(!IsInGameThread());
		while (!MDebugLock.TryLock())
		{
			MWaitEvent->Wait(1);
		}
		MDebugLock.Unlock();
	}
	inline void ProgressSubstep()
	{
		if (!MDebugMode)
			return;
		check(IsInGameThread());
		MDebugLock.Unlock();
		MWaitEvent->Trigger();
		MDebugLock.Lock();
	}

	TPBDRigidParticles<T, d> MParticles;
	TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d> MClustering;

	TArray<TSet<int32>> MIslandParticles;
	TArray<int32> IslandSleepCounts;
	TSet<int32> MActiveIndices;
	TArray<int32> MActiveIndicesArray;
	TSet<TTuple<int32, int32>> MDisabledCollisions;
	T MTime;

	// User query data
	TArrayCollectionArray<bool> MCollided;
	TArray<TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)>> MForceRules;
	TArray<TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)>> MConstraintRules;
	TFunction<void(TPBDRigidParticles<T, d>&, const T, const TArray<int32>&)> MParticleUpdateVelocity;
	TFunction<void(TPBDRigidParticles<T, d>&, const T)> MParticleUpdatePosition;
	TFunction<void(TPBDRigidParticles<T, d>&, const T, const T, const int32)> MKinematicUpdate;
	TFunction<void(TPBDRigidParticles<T, d>&, TPBDCollisionConstraint<T, d>&)> MCollisionContacts;
	TFunction<void(TPBDRigidParticles<T, d>&)> MBreaking;
	TFunction<void(TPBDRigidParticles<T, d>&)> MTrailing;

	TPBDCollisionConstraint<T, d> MCollisionRule;

	FCriticalSection MDebugLock;
	TUniquePtr<FEvent> MWaitEvent;
	bool MDebugMode;

	T MFriction;
	T MRestitution;
	T SleepLinearThreshold;
	T SleepAngularThreshold;
	int32 MNumIterations;
	int32 MPushOutIterations;
	int32 MPushOutPairIterations;
};

template<class T, int d>
class TPBDRigidsEvolutionPGS : public TPBDRigidsEvolutionBase<TPBDRigidsEvolutionPGS<T, d>, TPBDCollisionConstraintPGS<T, d>, T, d>
{
	typedef TPBDRigidsEvolutionBase<TPBDRigidsEvolutionPGS<T, d>, TPBDCollisionConstraintPGS<T, d>, T, d> Base;
  public:
	typedef TPBDCollisionConstraintPGS<T, d> FPBDCollisionConstraint;

    using Base::ActiveIndices;
    using Base::AddSubstep;
    using Base::IslandParticles;
	using Base::IslandSleepCounts;
    using Base::MActiveIndices;
    using Base::MActiveIndicesArray;
    using Base::MConstraintRules;
    using Base::MCollisionContacts;
	using Base::MBreaking;
	using Base::MTrailing;
	using Base::MClustering;
	using Base::MCollisionRule;
	using Base::MCollided;
    using Base::MForceRules;
    using Base::MFriction;
    using Base::MIslandParticles;
    using Base::MKinematicUpdate;
    using Base::MNumIterations;
    using Base::MParticles;
    using Base::MParticleUpdatePosition;
    using Base::MParticleUpdateVelocity;
    using Base::MPushOutIterations;
    using Base::MPushOutPairIterations;
    using Base::MRestitution;
    using Base::MTime;
    using Base::SetParticleUpdatePositionFunction;
    using Base::SetParticleUpdateVelocityFunction;
	using Base::SleepLinearThreshold;
	using Base::SleepAngularThreshold;

	// TODO(mlentine): Init particles from some type of input
	CHAOS_API TPBDRigidsEvolutionPGS(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations = 1);
	CHAOS_API ~TPBDRigidsEvolutionPGS() {}

	CHAOS_API void IntegrateV(const TArray<int32>& ActiveIndices, const T dt);
	CHAOS_API void IntegrateX(const TArray<int32>& ActiveIndices, const T dt);
	CHAOS_API void AdvanceOneTimeStep(const T dt);
};

template<class T, int d>
class TPBDRigidsEvolutionGBF : public TPBDRigidsEvolutionBase<TPBDRigidsEvolutionGBF<T, d>, TPBDCollisionConstraint<T, d>, T, d>
{
	typedef TPBDRigidsEvolutionBase<TPBDRigidsEvolutionGBF<T, d>, TPBDCollisionConstraint<T, d>, T, d> Base;
  public:
	typedef TPBDCollisionConstraint<T, d> FPBDCollisionConstraint;

    using Base::ActiveIndices;
    using Base::AddSubstep;
    using Base::IslandParticles;
	using Base::IslandSleepCounts;
    using Base::MActiveIndices;
    using Base::MActiveIndicesArray;
    using Base::MConstraintRules;
    using Base::MCollisionContacts;
	using Base::MBreaking;
	using Base::MTrailing;
	using Base::MClustering;
	using Base::MCollisionRule;
    using Base::MCollided;
    using Base::MForceRules;
    using Base::MFriction;
    using Base::MIslandParticles;
    using Base::MKinematicUpdate;
    using Base::MNumIterations;
    using Base::MParticles;
    using Base::MParticleUpdatePosition;
    using Base::MParticleUpdateVelocity;
    using Base::MPushOutIterations;
    using Base::MPushOutPairIterations;
    using Base::MRestitution;
    using Base::MTime;
    using Base::SetParticleUpdatePositionFunction;
    using Base::SetParticleUpdateVelocityFunction;
	using Base::SleepLinearThreshold;
	using Base::SleepAngularThreshold;

	// TODO(mlentine): Init particles from some type of input
	CHAOS_API TPBDRigidsEvolutionGBF(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations = 1);
	CHAOS_API ~TPBDRigidsEvolutionGBF() {}

	CHAOS_API void Integrate(const TArray<int32>& ActiveIndices, const T dt);
	CHAOS_API void AdvanceOneTimeStep(const T dt);
};
}
