// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterPawn.h"

#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/RotatingMovementComponent.h"

#include "DisplayClusterPawnDefault.generated.h"


/**
 * Extended VR root. Implements some basic features.
 */
UCLASS()
class DISPLAYCLUSTER_API ADisplayClusterPawnDefault
	: public ADisplayClusterPawn
{
	GENERATED_UCLASS_BODY()

public:

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawn")
	float BaseTurnRate;

	/** Base lookup rate, in deg/sec. Other scaling may affect final lookup rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pawn")
	float BaseLookUpRate;

public:
	virtual UPawnMovementComponent* GetMovementComponent() const override
	{ return MovementComponent; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// APawn
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaSeconds) override;

public:
	/**
	* Input callback to move forward in local space (or backward if Val is negative).
	* @param Val Amount of movement in the forward direction (or backward if negative).
	* @see APawn::AddMovementInput()
	*/
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MoveForward(float Val);

	/**
	* Input callback to strafe right in local space (or left if Val is negative).
	* @param Val Amount of movement in the right direction (or left if negative).
	* @see APawn::AddMovementInput()
	*/
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MoveRight(float Val);

	/**
	* Input callback to move up in world space (or down if Val is negative).
	* @param Val Amount of movement in the world up direction (or down if negative).
	* @see APawn::AddMovementInput()
	*/
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MoveUp(float Val);

	/**
	* Called via input to turn at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void TurnAtRate(float Rate);

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void TurnAtRate2(float Rate);

	/**
	* Called via input to look up at a given rate (or down if Rate is negative).
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void LookUpAtRate(float Rate);

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

protected:
	/** Movement component */
	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UFloatingPawnMovement* MovementComponent;

	/** Rotating movement */
	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	URotatingMovementComponent* RotatingComponent;

	UPROPERTY(Category = Pawn, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	URotatingMovementComponent * RotatingComponent2;

private:
	IPDisplayClusterGameManager* GameMgr = nullptr;

	bool bIsCluster = false;
};
