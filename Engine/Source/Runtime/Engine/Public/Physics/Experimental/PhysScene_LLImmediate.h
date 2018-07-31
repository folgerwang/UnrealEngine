// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
};
