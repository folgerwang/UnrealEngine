// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MeshComponent.h"
#include "Chaos/ChaosSolverActor.h"
#include "GameFramework/Actor.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"

#include "StaticMeshSimulationComponent.generated.h"

class FStaticMeshSimulationComponentPhysicsProxy;

/**
*	UStaticMeshSimulationComponent
*/
UCLASS(ClassGroup = Physics, Experimental, meta = (BlueprintSpawnableComponent))
class GEOMETRYCOLLECTIONENGINE_API UStaticMeshSimulationComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type ReasonEnd) override;

	/** When Simulating is enabled the Component will initialize its rigid bodies within the solver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	bool Simulating;

	/** ObjectType defines how to initialize the rigid collision structures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	EObjectTypeEnum ObjectType;

	/** Damage threshold for clusters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	float Mass;

	/** CollisionType defines how to initialize the rigid collision structures.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	ECollisionTypeEnum CollisionType;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	EInitialVelocityTypeEnum InitialVelocityType;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	FVector InitialLinearVelocity;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	FVector InitialAngularVelocity;

	/** Damage threshold for clusters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float DamageThreshold;

	/** Uniform Friction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float Friction;

	/** Coefficient of Restitution (aka Bouncyness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float Bouncyness;

	/** Chaos RBD Solver */
	UPROPERTY(EditAnywhere, Category = "ChaosPhysics", meta = (DisplayName = "Chaos Solver"))
	AChaosSolverActor* ChaosSolverActor;

#if INCLUDE_CHAOS
	const TSharedPtr<FPhysScene_Chaos> GetPhysicsScene() const;
#endif
private : 

	FStaticMeshSimulationComponentPhysicsProxy* PhysicsProxy;

protected:

	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;

};