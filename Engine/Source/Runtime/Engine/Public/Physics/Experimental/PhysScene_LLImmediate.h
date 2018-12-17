// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "Containers/Set.h"

#include "Physics/PhysScene.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"

namespace ImmediatePhysics
{
	struct FSimulation;
}


class ENGINE_API FPhysScene_LLImmediate
{
public:

	typedef TArray<ImmediatePhysics::FActorHandle*> DataType;

	FPhysScene_LLImmediate();
	~FPhysScene_LLImmediate();

	void Init();
	void Tick(float InDeltaSeconds);

	void SetKinematicUpdateFunction(TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const float, const int32)> InKinematicUpdate)
	{
		KinematicUpdateFunction = InKinematicUpdate;
	}

	void SetStartFrameFunction(TFunction<void(const float)> InStartFrame)
	{
		StartFrameFunction = InStartFrame;
	}

	void SetEndFrameFunction(TFunction<void(const float)> InEndFrame)
	{
		EndFrameFunction = InEndFrame;
	}

	void SetCreateBodiesFunction(TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&)> InCreateBodies)
	{
		CreateBodiesFunction = InCreateBodies;
	}

	void SetParameterUpdateFunction(TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const int32)> InParameterUpdate)
	{
		ParameterUpdateFunction = InParameterUpdate;
	}

	void SetDisableCollisionsUpdateFunction(TFunction<void(TSet<TTuple<int32, int32>>&)> InDisableCollisionsUpdate)
	{
		DisableCollisionsUpdateFunction = InDisableCollisionsUpdate;
	}

	void AddPBDConstraintFunction(TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float)> InConstraintFunction)
	{
		ConstraintFunctions.Add(InConstraintFunction);
	}

	void AddForceFunction(TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const int32)> InForceFunction)
	{
		ForceFunctions.Add(InForceFunction);
	}

	ImmediatePhysics::FSimulation* GetSimulation() const
	{
		return Simulation;
	}

	const int32 GetCurrentFrame() const { return CurrentFrame; }

private:

	TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const float, const int32)> KinematicUpdateFunction;
	TFunction<void(const float)> StartFrameFunction;
	TFunction<void(const float)> EndFrameFunction;
	TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&)> CreateBodiesFunction;
	TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const int32)> ParameterUpdateFunction;
	TFunction<void(TSet<TTuple<int32, int32>>&)> DisableCollisionsUpdateFunction;
	TArray<TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float)>> ConstraintFunctions;
	TArray<TFunction<void(TArray<ImmediatePhysics::FActorHandle*>&, const float, const int32)>> ForceFunctions;

	ImmediatePhysics::FSimulation* Simulation;
	float SimulationTime;
	int32 CurrentFrame;
};

#if ! INCLUDE_CHAOS
// stub solver callbacks for when Chaos is not included.
class FSolverCallbacks
{
public:
	typedef FPhysScene_LLImmediate::DataType FParticlesType;

	virtual ~FSolverCallbacks() {}
	virtual void UpdateKinematicBodiesCallback(const FParticlesType& Particles, const float Dt, const float Time) {};
	virtual void StartFrameCallback(const float) {};
	virtual void EndFrameCallback(const float) {};
	virtual void CreateRigidBodyCallback(FParticlesType&) {};
	virtual void ParameterUpdateCallback(FParticlesType&, const float) {};
	virtual void DisableCollisionsCallback(TSet<TTuple<int32, int32>>&) {};
	virtual void AddConstraintCallback(FParticlesType&, const float, const int32) {};
	virtual void AddForceCallback(FParticlesType&, const float, const int32) {};
};
#endif
