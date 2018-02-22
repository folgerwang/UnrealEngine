// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsReplication.h
	Manage replication of physics bodies
=============================================================================*/

#pragma once

#include "Engine/EngineTypes.h"

struct FReplicatedPhysicsTarget
{
	/** The target state replicated by server */
	FRigidBodyState TargetState;

	/** Client time when target state arrived */
	float ArrivedTimeSeconds;
};

struct FBodyInstance;
struct FRigidBodyErrorCorrection;
class UWorld;
class FPhysScene;

class ENGINE_API FPhysicsReplication
{
public:
	FPhysicsReplication(FPhysScene* PhysScene);
	virtual ~FPhysicsReplication() {}

	/** Tick and update all body states according to replicated targets */
	void Tick(float DeltaSeconds);

	/** Sets the latest replicated target for a body instance */
	void SetReplicatedTarget(FBodyInstance* BI, const FRigidBodyState& ReplicatedTarget);

protected:

	/** Update the physics body state given a set of replicated targets */
	virtual void OnTick(float DeltaSeconds, TMap<FBodyInstance*, FReplicatedPhysicsTarget>& BodyToTargetMap);
	bool ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, const FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection);

private:
	TMap<FBodyInstance*, FReplicatedPhysicsTarget> BodiesToTargets;
	FPhysScene* PhysScene;

};