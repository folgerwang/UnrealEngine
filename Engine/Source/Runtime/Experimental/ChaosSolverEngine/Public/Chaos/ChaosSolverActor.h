// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class represents an ChaosSolver Actor. */

#include "Chaos/ChaosSolver.h"
#include "Components/BillboardComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "UObject/ObjectMacros.h"

#include "ChaosSolverActor.generated.h"


UCLASS()
class CHAOSSOLVERENGINE_API AChaosSolverActor: public AActor
{
	GENERATED_UCLASS_BODY()

public:
	/**
	* NumberOfSubSteps
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics")
	float TimeStepMultiplier;

	/**
	* Collision Iteration
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Iterations")
	int32 CollisionIterations;

	/**
	* PushOut Iteration
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Iterations")
	int32 PushOutIterations;

	/**
	* PushOut Iteration
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Iterations")
	int32 PushOutPairIterations;

	/*
	* Maximum number of collisions passed in a buffer to the ChaosNiagara dataInterface
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|CollisionData Generation", meta = (DisplayName = "Maximum Data Size", UIMin = 0))
	int32 CollisionDataSizeMax;

	/*
	* Width of the time window in seconds for collecting collisions in a buffer
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|CollisionData Generation", meta = (DisplayName = "Time Window", UIMin = 0.0))
	float CollisionDataTimeWindow;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|CollisionData Generation", meta = (DisplayName = "Use Spatial Hash"))
	bool DoCollisionDataSpatialHash;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|CollisionData Generation", meta = (DisplayName = "Spatial Hash Radius", UIMin = 0.01))
	float CollisionDataSpatialHashRadius;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|CollisionData Generation", meta = (DisplayName = "Max Number Collisions Per Cell", UIMin = 0))
	int32 MaxCollisionPerCell;

	/*
	* Maximum number of breakings passed in a buffer to the ChaosNiagara dataInterface
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|BreakingData Generation", meta = (DisplayName = "Maximum Data Size", UIMin = 0))
	int32 BreakingDataSizeMax;

	/*
	* Width of the time window in seconds for collecting breakings in a buffer
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|BreakingData Generation", meta = (DisplayName = "Time Window"))
	float BreakingDataTimeWindow;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|BreakingData Generation", meta = (DisplayName = "Use Spatial Hash"))
	bool DoBreakingDataSpatialHash;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|BreakingData Generation", meta = (DisplayName = "Spatial Hash Radius", UIMin = 0.01))
	float BreakingDataSpatialHashRadius;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|BreakingData Generation", meta = (DisplayName = "Max Number Breakings Per Cell", UIMin = 0))
	int32 MaxBreakingPerCell;

	/*
	* Maximum number of trailings passed in a buffer to the ChaosNiagara dataInterface
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|TrailingData Generation", meta = (DisplayName = "Maximum Data Size", UIMin = 0))
	int32 TrailingDataSizeMax;

	/*
	* Width of the time window in seconds for collecting trailings in a buffer
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|TrailingData Generation", meta = (DisplayName = "Time Window"))
	float TrailingDataTimeWindow;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|TrailingData Generation", meta = (DisplayName = "Min Speed Threshold", UIMin = 0.01))
	float TrailingMinSpeedThreshold;

	/*
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|TrailingData Generation", meta = (DisplayName = "Min Volume Threshold", UIMin = 0.01))
	float TrailingMinVolumeThreshold;

	/*
	* 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Floor", meta = (DisplayName = "Use Floor"))
	bool HasFloor;

	/*
	* 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Floor", meta = (DisplayName = "Floor Height"))
	float FloorHeight;

	/*
	* Display icon in the editor
	*/
	UPROPERTY()
	// A UBillboardComponent to hold Icon sprite
	UBillboardComponent* SpriteComponent;

	// Icon sprite
	UTexture2D* SpriteTexture;

#if INCLUDE_CHAOS
	TSharedPtr<FPhysScene_Chaos> GetPhysicsScene()
	{
		return PhysScene;
	}

	Chaos::PBDRigidsSolver* GetSolver()
	{
		return Solver;
	}

#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type ReasonEnd) override;

private:
#if INCLUDE_CHAOS
	TSharedPtr<FPhysScene_Chaos> PhysScene;
	Chaos::PBDRigidsSolver* Solver;
#endif
};