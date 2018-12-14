// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsReplication.h
	Manage replication of physics bodies
=============================================================================*/

#pragma once

#include "Engine/EngineTypes.h"
#include "Physics/PhysicsInterfaceDeclares.h"

class FPhysScene_PhysX;

struct FReplicatedPhysicsTarget
{
	FReplicatedPhysicsTarget() :
		ArrivedTimeSeconds(0.0f),
		AccumulatedErrorSeconds(0.0f)
	{ }

	/** The target state replicated by server */
	FRigidBodyState TargetState;

	/** The bone name used to find the body */
	FName BoneName;

	/** Client time when target state arrived */
	float ArrivedTimeSeconds;

	/** Physics sync error accumulation */
	float AccumulatedErrorSeconds;

	/** Correction values from previous update */
	FVector PrevPosTarget;
	FVector PrevPos;

#if !UE_BUILD_SHIPPING
	FDebugFloatHistory ErrorHistory;
#endif
};

struct FBodyInstance;
struct FRigidBodyErrorCorrection;
class UWorld;
class UPrimitiveComponent;

class ENGINE_API FPhysicsReplication
{
public:
	FPhysicsReplication(FPhysScene* PhysScene);
	virtual ~FPhysicsReplication() {}

	/** Tick and update all body states according to replicated targets */
	void Tick(float DeltaSeconds);

	/** Sets the latest replicated target for a body instance */
	virtual void SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget);

	/** Remove the replicated target*/
	virtual void RemoveReplicatedTarget(UPrimitiveComponent* Component);

protected:

	/** Update the physics body state given a set of replicated targets */
	virtual void OnTick(float DeltaSeconds, TMap<TWeakObjectPtr<UPrimitiveComponent>, FReplicatedPhysicsTarget>& ComponentsToTargets);
	virtual void OnTargetRestored(TWeakObjectPtr<UPrimitiveComponent> Component, const FReplicatedPhysicsTarget& Target) {}

	/** Called when a dynamic rigid body receives a physics update */
	virtual bool ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection, const float PingSecondsOneWay);

	UWorld* GetOwningWorld();
	const UWorld* GetOwningWorld() const;

private:

	/** Get the ping from this machine to the server */
	float GetLocalPing() const;

	/** Get the ping from  */
	float GetOwnerPing(const AActor* const Owner, const FReplicatedPhysicsTarget& Target) const;

private:
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FReplicatedPhysicsTarget> ComponentToTargets;
	FPhysScene* PhysScene;

};