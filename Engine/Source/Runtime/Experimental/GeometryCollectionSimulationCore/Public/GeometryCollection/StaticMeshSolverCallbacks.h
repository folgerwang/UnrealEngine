// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArray.h"

#if INCLUDE_CHAOS

#include "PBDRigidsSolver.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"

class GEOMETRYCOLLECTIONSIMULATIONCORE_API FStaticMeshSolverCallbacks : public FSolverCallbacks
{
public:

	struct Params
	{
		Params()
			: Name("")
			, InitialTransform(FTransform::Identity)
			, InitialLinearVelocity(FVector::ZeroVector)
			, InitialAngularVelocity(FVector::ZeroVector)
			, ObjectType(EObjectTypeEnum::Chaos_Object_Dynamic)
			, bSimulating(false)
			, TargetTransform(nullptr)
			, Mass(0.0f)
		{}

		FString Name;
		TArray<FVector> MeshVertexPositions;
		FTransform InitialTransform;
		FVector InitialLinearVelocity;
		FVector InitialAngularVelocity;
		EObjectTypeEnum ObjectType;
		bool bSimulating;
		FTransform* TargetTransform;
		float Mass;
	};

	FStaticMeshSolverCallbacks();
	virtual ~FStaticMeshSolverCallbacks() {}

	/**/
	static int8 Invalid;

	void SetParameters(Params& InParams)
	{
		Parameters = InParams;
	}

	Params& GetParameters() { return Parameters; }
	const Params& GetParameters() const { return Parameters; }

	/**/
	void Initialize();

	/**/
	void Reset();

	/**/
	virtual bool IsSimulating() const override;

	/**/
	virtual void CreateRigidBodyCallback(FSolverCallbacks::FParticlesType& Particles) override;

	/**/
	virtual void BindParticleCallbackMapping(const int32 & CallbackIndex, FSolverCallbacks::IntArray & ParticleCallbackMap);

	/**/
	virtual void UpdateKinematicBodiesCallback(const FSolverCallbacks::FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Index) override;

	/**/
	virtual void StartFrameCallback(const float Dt, const float Time) override {}

	/**/
	virtual void EndFrameCallback(const float EndFrame) override;

	/**/
	virtual void ParameterUpdateCallback(FSolverCallbacks::FParticlesType& Particles, const float Time) override {}

	/**/
	virtual void DisableCollisionsCallback(TSet<TTuple<int32, int32>>& CollisionPairs) override {}

	/**/
	virtual void AddConstraintCallback(FSolverCallbacks::FParticlesType& Particles, const float Time, const int32 Island) override {}

	/**/
	virtual void AddForceCallback(FSolverCallbacks::FParticlesType& Particles, const float Dt, const int32 Index) override {}

	/**/
	int32 GetRigidBodyId() const { return RigidBodyId; }

	bool bEnableCollisionParticles;
	float DamageThreshold;

private:

	Params Parameters;

	bool InitializedState;
	int32 RigidBodyId;
	FVector CenterOfMass;
	FVector Scale;
};
#endif