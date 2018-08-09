// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/PBDRigidClustering.h"
#include "Apeiron/PBDRigidParticles.h"
#include "Apeiron/Transform.h"

namespace Apeiron
{
template<class T, int d>
class TPBDRigidsEvolution
{
  public:
	// TODO(mlentine): Init particles from some type of input
	APEIRON_API TPBDRigidsEvolution(TPBDRigidParticles<T, d>&& InParticles, int32 NumIterations = 1);
	APEIRON_API ~TPBDRigidsEvolution() {}

	APEIRON_API void AdvanceOneTimeStep(const T dt);

	APEIRON_API inline void EnableDebugMode()
	{
		check(IsInGameThread());
		MDebugMode = true;
		MDebugLock.Lock();
	}

	APEIRON_API inline void InitializeFromParticleData()
	{
		MActiveIndices.Reset();
		for (uint32 i = 0; i < MParticles.Size(); ++i)
		{
			if (MParticles.Sleeping(i) || MParticles.Disabled(i))
				continue;
			MActiveIndices.Add(i);
		}
	}

	APEIRON_API void SetKinematicUpdateFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T, const T, const int32)> KinematicUpdate) { MKinematicUpdate = KinematicUpdate; }
	APEIRON_API void SetParticleUpdateFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T)> ParticleUpdate) { MParticleUpdate = ParticleUpdate; }
	APEIRON_API void AddPBDConstraintFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T)> ConstraintFunction) { MConstraintRules.Add(ConstraintFunction); }
	APEIRON_API void AddForceFunction(TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)> ForceFunction) { MForceRules.Add(ForceFunction); }

	/**/
	APEIRON_API TPBDRigidParticles<T, d>& Particles() { return MParticles; }
	APEIRON_API const TPBDRigidParticles<T, d>& Particles() const { return MParticles; }

	/**/
	APEIRON_API TArray<TSet<int32>>& IslandParticles() { return MIslandParticles; }
	APEIRON_API const TArray<TSet<int32>>& IslandParticles() const { return MIslandParticles; }

	/**/
	APEIRON_API TArray<int32>& ActiveIndicesArray() { return MActiveIndicesArray; }
	APEIRON_API const TArray<int32>& ActiveIndicesArray() const { return MActiveIndicesArray; }

	/**/
	APEIRON_API TSet<int32>& ActiveIndices() { return MActiveIndices; }
	APEIRON_API const TSet<int32>& ActiveIndices() const { return MActiveIndices; }

	/**/
	APEIRON_API TSet<TTuple<int32, int32>>& DisabledCollisions() { return MDisabledCollisions; }
	APEIRON_API const TSet<TTuple<int32, int32>>& DisabledCollisions() const { return MDisabledCollisions; }

	/**/
	APEIRON_API inline int32 CreateClusterParticle(const TArray<uint32>& Children) { return MClustering.CreateClusterParticle(Children); }
	APEIRON_API inline const T Strain(const uint32 Index) const { return MClustering.Strain(Index); }
	APEIRON_API inline T& Strain(const uint32 Index) { return MClustering.Strain(Index); }

	/**/
	APEIRON_API inline void SetFriction(T InFriction ) { MFriction = InFriction; }
	APEIRON_API inline void SetRestitution(T InRestitution) { MRestitution = InRestitution; }


  private:
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
	TPBDRigidClustering<T, d> MClustering;
	TArray<TSet<int32>> MIslandParticles;
	TSet<int32> MActiveIndices;
	TArray<int32> MActiveIndicesArray;
	TSet<TTuple<int32, int32>> MDisabledCollisions;
	int32 MNumIterations;
	T MTime;

	// User query data
	TArrayCollectionArray<bool> MCollided;
	TArray<TFunction<void(TPBDRigidParticles<T, d>&, const T, const int32)>> MForceRules;
	TArray<TFunction<void(TPBDRigidParticles<T, d>&, const T)>> MConstraintRules;
	TFunction<void(TPBDRigidParticles<T, d>&, const T)> MParticleUpdate;
	TFunction<void(TPBDRigidParticles<T, d>&, const T, const T, const int32)> MKinematicUpdate;

	FCriticalSection MDebugLock;
	TUniquePtr<FEvent> MWaitEvent;
	bool MDebugMode;

	T MFriction;
	T MRestitution;
};
}
