// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterPawnDefault.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Game/IPDisplayClusterGameManager.h"

#include "DisplayClusterSceneComponentSyncParent.h"

#include "DisplayClusterSettings.h"
#include "DisplayClusterGameMode.h"
#include "DisplayClusterGlobals.h"

#include "Engine/World.h"
#include "Misc/DisplayClusterLog.h"
#include "GameFramework/WorldSettings.h"

#include "IPDisplayCluster.h"


ADisplayClusterPawnDefault::ADisplayClusterPawnDefault(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	// Movement component
	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent0"));
	MovementComponent->UpdatedComponent = RootComponent;

	// Rotation component
	RotatingComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComponent0"));
	RotatingComponent->UpdatedComponent = RootComponent;
	RotatingComponent->bRotationInLocalSpace = true;
	RotatingComponent->PivotTranslation = FVector::ZeroVector;
	RotatingComponent->RotationRate = FRotator::ZeroRotator;

	// Rotation component2
	RotatingComponent2 = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComponent1"));
	RotatingComponent2->UpdatedComponent = RootComponent;
	RotatingComponent2->bRotationInLocalSpace = false;
	RotatingComponent2->PivotTranslation = FVector::ZeroVector;
	RotatingComponent2->RotationRate = FRotator::ZeroRotator;

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
}

void ADisplayClusterPawnDefault::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	check(PlayerInputComponent);

	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindAxis("MoveForward", this, &ADisplayClusterPawnDefault::MoveForward);
		PlayerInputComponent->BindAxis("MoveRight",   this, &ADisplayClusterPawnDefault::MoveRight);
		PlayerInputComponent->BindAxis("MoveUp",      this, &ADisplayClusterPawnDefault::MoveUp);
		PlayerInputComponent->BindAxis("TurnRate",    this, &ADisplayClusterPawnDefault::TurnAtRate2);
		PlayerInputComponent->BindAxis("LookUpRate",  this, &ADisplayClusterPawnDefault::LookUpAtRate);
	}
}

void ADisplayClusterPawnDefault::BeginPlay()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::BeginPlay();

	if (!GDisplayCluster->IsModuleInitialized())
	{
		return;
	}

	GameMgr = GDisplayCluster->GetPrivateGameMgr();
	bIsCluster = (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);

	bUseControllerRotationYaw   = !bIsCluster;
	bUseControllerRotationPitch = !bIsCluster;
	bUseControllerRotationRoll  = !bIsCluster;

	// Enable collision if needed
	if (GameMgr && GameMgr->IsDisplayClusterActive())
	{
		const ADisplayClusterSettings* const pDisplayClusterSettings = GameMgr->GetDisplayClusterSceneSettings();
		if (pDisplayClusterSettings)
		{
			// Apply movement settings
			MovementComponent->MaxSpeed     = pDisplayClusterSettings->MovementMaxSpeed;
			MovementComponent->Acceleration = pDisplayClusterSettings->MovementAcceleration;
			MovementComponent->Deceleration = pDisplayClusterSettings->MovementDeceleration;
			MovementComponent->TurningBoost = pDisplayClusterSettings->MovementTurningBoost;

			// Apply rotation settings
			BaseTurnRate   = pDisplayClusterSettings->RotationSpeed;
			BaseLookUpRate = pDisplayClusterSettings->RotationSpeed;
		}
	}
}

void ADisplayClusterPawnDefault::BeginDestroy()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::BeginDestroy();
}

void ADisplayClusterPawnDefault::Tick(float DeltaSeconds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::Tick(DeltaSeconds);

	const float Mult = GetWorld()->GetWorldSettings()->WorldToMeters / 100.f;
	SetActorScale3D(FVector(Mult, Mult, Mult));
}

void ADisplayClusterPawnDefault::MoveRight(float Val)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (Val != 0.f)
	{
		UE_LOG(LogDisplayClusterGame, Verbose, TEXT("ADisplayClusterPawn::MoveRight: %f"), Val);

		const USceneComponent* const pComp = (TranslationDirection ? TranslationDirection : RootComponent);
		AddMovementInput(pComp->GetRightVector(), Val);
	}
}

void ADisplayClusterPawnDefault::MoveForward(float Val)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (Val != 0.f)
	{
		UE_LOG(LogDisplayClusterGame, Verbose, TEXT("ADisplayClusterPawn::MoveForward: %f"), Val);

		const USceneComponent* const pComp = (TranslationDirection ? TranslationDirection : RootComponent);
		AddMovementInput(pComp->GetForwardVector(), Val);
	}
}

void ADisplayClusterPawnDefault::MoveUp(float Val)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (Val != 0.f)
	{
		UE_LOG(LogDisplayClusterGame, Verbose, TEXT("ADisplayClusterPawn::MoveUp: %f"), Val);

		const USceneComponent* const pComp = (TranslationDirection ? TranslationDirection : RootComponent);
		AddMovementInput(pComp->GetUpVector(), Val);
	}
}

void ADisplayClusterPawnDefault::TurnAtRate(float Rate)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("ADisplayClusterPawn::TurnAtRate: %f"), Rate);

	if (bIsCluster)
	{
		IPDisplayClusterGameManager* const pMgr = GDisplayCluster->GetPrivateGameMgr();
		if (pMgr)
		{
			auto* const pCam = pMgr->GetActiveCamera();
			if (pCam)
			{
				if (RotatingComponent->UpdatedComponent)
				{
					const FTransform TransformToRotate = RotatingComponent->UpdatedComponent->GetComponentTransform();
					const FVector RotateAroundPivot = TransformToRotate.InverseTransformPositionNoScale(pCam->GetComponentLocation());
					RotatingComponent->PivotTranslation = RotateAroundPivot;
					RotatingComponent->RotationRate = FRotator(RotatingComponent->RotationRate.Pitch, Rate * BaseTurnRate, 0.f);
				}
			}
		}
	}
	else
	{
		if (Rate != 0.f)
		{
			AddControllerYawInput(BaseTurnRate * Rate * GetWorld()->GetDeltaSeconds() * CustomTimeDilation);
		}
	}
}

void ADisplayClusterPawnDefault::TurnAtRate2(float Rate)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("ADisplayClusterPawn::TurnAtRate2: %f"), Rate);

	if (bIsCluster)
	{
		IPDisplayClusterGameManager* const pMgr = GDisplayCluster->GetPrivateGameMgr();
		if (pMgr)
		{
			UDisplayClusterCameraComponent* const pCam = pMgr->GetActiveCamera();
			if (pCam)
			{
				if (RotatingComponent2->UpdatedComponent)
				{
					const FTransform TransformToRotate = RotatingComponent2->UpdatedComponent->GetComponentTransform();
					const FVector RotateAroundPivot = TransformToRotate.InverseTransformPositionNoScale(pCam->GetComponentLocation());
					RotatingComponent2->PivotTranslation = RotateAroundPivot;
					RotatingComponent2->RotationRate = FRotator(RotatingComponent2->RotationRate.Pitch, Rate * BaseTurnRate, 0.f);
				}
			}
		}
	}
	else
	{
		if (Rate != 0.f)
		{
			AddControllerYawInput(BaseTurnRate * Rate * GetWorld()->GetDeltaSeconds() * CustomTimeDilation);
		}
	}
}

void ADisplayClusterPawnDefault::LookUpAtRate(float Rate)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (bIsCluster)
	{
		//@note: usually CAVE-like systems don't use roll and pitch rotation since it can cause dizziness.
#if 0
		//@todo: rotate around active camera
		IPDisplayClusterGameManager* const pMgr = GDisplayCluster->GetPrivateGameMgr();
		if (pMgr)
		{
			auto* const pCam = pMgr->GetActiveCamera();
			if (pCam)
			{
				RotatingComponent->bRotationInLocalSpace = true;
				RotatingComponent->PivotTranslation = FVector::ZeroVector;
				RotatingComponent->RotationRate = FRotator(Rate * BaseLookUpRate, RotatingComponent->RotationRate.Yaw, 0.f);
			}
		}
#endif
	}
	else
	{
		if (Rate != 0.f)
		{
			AddControllerPitchInput(BaseTurnRate * Rate * GetWorld()->GetDeltaSeconds() * CustomTimeDilation);
		}
	}
}
