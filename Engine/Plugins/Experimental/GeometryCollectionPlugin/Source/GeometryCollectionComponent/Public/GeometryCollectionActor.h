// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class represents an APEX GeometryCollection Actor. */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection.h"
#include "Physics/Experimental/PhysScene_Apeiron.h"
#include "Physics/Experimental/PhysScene_LLImmediate.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"

#include "GeometryCollectionActor.generated.h"

class UGeometryCollectionComponent;

UCLASS()
class GEOMETRYCOLLECTIONCOMPONENT_API AGeometryCollectionActor : public AActor
{
	GENERATED_UCLASS_BODY()
public:

#if INCLUDE_APEIRON
	typedef Apeiron::TPBDRigidParticles<float, 3> FParticleType;
#else
	typedef TArray<ImmediatePhysics::FActorHandle*> FParticleType;
#endif

	static int8 Invalid;

	/* Game state callback */
	void Tick(float DeltaSeconds);

	/* GeometryCollectionComponent */
	UPROPERTY(VisibleAnywhere, Category = Destruction, meta = (ExposeFunctionCategories = "Components|GeometryCollection", AllowPrivateAccess = "true"))
	UGeometryCollectionComponent* GeometryCollectionComponent;
	UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return GeometryCollectionComponent; }

	/**
	* Damage threshold for clusters.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DamageThreshold;

	/**
	* Uniform Friction
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float Friction;

	/**
	* Coefficient of Restitution (aka Bouncyness)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float Bouncyness;

protected:

	/* Reset simulation specific attributes. */
	void ResetAttributes();

	/* Create and Advance the simulation*/
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void InitializeSimulation();

	/**/
	void BuildClusters(const TMap<uint32, TArray<uint32> > & ClusterMap);

	/* Bind the initial clustering configuration.*/
	void InitializeClustering(uint32 ParentIndex, FParticleType&);
	
	/* Start of frame physics solver callback  */
	void StartFrameCallback(float StartFrame);

	/* Create rigid body physics solver callback */
	void CreateRigidBodyCallback(FParticleType&);

	/* End of frame physics solver callback  */
	void EndFrameCallback(float EndFrame);

private:
#if INCLUDE_APEIRON 
	FPhysScene_Apeiron Scene;
	Apeiron::TArrayCollectionArray<int32> ExternalID;
#else
	FPhysScene_LLImmediate Scene;
#endif
	TSharedRef< TManagedArray<int32> > RigidBodyIdArray;
	TSharedRef< TManagedArray<FVector> > CenterOfMassArray;

	static bool InitializedState;
};