// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Vector.h"
#include "CoreMinimal.h"

namespace Chaos
{
/**/
template<class T, int d>
struct TRigidBodyContactConstraint
{
	TRigidBodyContactConstraint() : AccumulatedImpulse(0.f) {}
	int32 ParticleIndex, LevelsetIndex;
	TVector<T, d> Normal;
	TVector<T, d> Location;
	T Phi;
	TVector<T, d> AccumulatedImpulse;

	FString ToString() const
	{
		return FString::Printf(TEXT("ParticleIndex:%d, LevelsetIndex:%d, Normal:%s, Location:%s, Phi:%f, AccumulatedImpulse:%s"), ParticleIndex, LevelsetIndex, *Normal.ToString(), *Location.ToString(), Phi, *AccumulatedImpulse.ToString());
	}
};

template<class T, int d>
struct TRigidBodyContactConstraintPGS
{
	TRigidBodyContactConstraintPGS() : AccumulatedImpulse(0.f) {}
	int32 ParticleIndex, LevelsetIndex;
	TArray<TVector<T, d>> Normal;
	TArray<TVector<T, d>> Location;
	TArray<T> Phi;
	TVector<T, d> AccumulatedImpulse;
};

/*
CollisionData passed from the solver to Niagara
*/
template<class T, int d>
struct TCollisionData
{
	float Time;
	TVector<T, d> Location;
	TVector<T, d> AccumulatedImpulse;
	TVector<T, d> Normal;
	TVector<T, d> Velocity1, Velocity2;
	T Mass1, Mass2;
	int32 ParticleIndex, LevelsetIndex;
};

/*
BreakingData passed from the solver to Niagara
*/
template<class T, int d>
struct TBreakingData
{
	float Time;
	FVector BreakingRegionCentroid;
	FVector BreakingRegionNormal;
	float BreakingRegionRadius;
	TVector<T, d> Velocity;
	T Mass;
	int32 ParticleIndex;
};

/*
TrailingData passed from the solver to Niagara
*/
template<class T, int d>
struct TTrailingData
{
	float TimeTrailingStarted;
	TVector<T, d> Location;
	T ExtentMin;
	T ExtentMax;
	TVector<T, d> Velocity;
	TVector<T, d> AngularVelocity;
	T Mass;
	int32 ParticleIndex;

	friend inline uint32 GetTypeHash(const TTrailingData& Other)
	{
		return ::GetTypeHash(Other.ParticleIndex);
	}

	friend bool operator==(const TTrailingData& A, const TTrailingData& B)
	{
		return A.ParticleIndex == B.ParticleIndex;
	}
};

}
