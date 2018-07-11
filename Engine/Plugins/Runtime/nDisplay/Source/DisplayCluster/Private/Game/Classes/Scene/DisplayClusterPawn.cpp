// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterPawn.h"

#include "Engine/CollisionProfile.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Game/IPDisplayClusterGameManager.h"

#include "DisplayClusterSceneComponentSyncParent.h"

#include "IPDisplayCluster.h"
#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterSettings.h"
#include "DisplayClusterGameMode.h"
#include "DisplayClusterGlobals.h"


ADisplayClusterPawn::ADisplayClusterPawn(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	// Collision component
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent0"));
	CollisionComponent->InitSphereRadius(35.0f);
	CollisionComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CollisionComponent->CanCharacterStepUpOn = ECB_No;
	CollisionComponent->SetCanEverAffectNavigation(true);
	CollisionComponent->bDynamicObstacle = true;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Collision component must always be a root
	RootComponent = CollisionComponent;

	// Collision offset component
	CollisionOffsetComponent = CreateDefaultSubobject<UDisplayClusterSceneComponent>(TEXT("DisplayCluster_offset"));
	CollisionOffsetComponent->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));

	// DisplayCluster sync
	DisplayClusterSyncRoot = CreateDefaultSubobject<UDisplayClusterSceneComponentSyncParent>(TEXT("DisplayCluster_root_sync"));
	DisplayClusterSyncRoot->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));

	DisplayClusterSyncCollisionOffset = CreateDefaultSubobject<UDisplayClusterSceneComponentSyncParent>(TEXT("DisplayCluster_colloffset_sync"));
	DisplayClusterSyncCollisionOffset->AttachToComponent(CollisionOffsetComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));

	// Camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("DisplayCluster_camera"));
	CameraComponent->AttachToComponent(CollisionOffsetComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
	CameraComponent->bUsePawnControlRotation = false;
	CameraComponent->bAbsoluteLocation = false;
	CameraComponent->bAbsoluteRotation = false;

	PrimaryActorTick.bCanEverTick = true;
	bFindCameraComponentWhenViewTarget = true;
	bCanBeDamaged = false;
	bReplicates = false;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
}

void ADisplayClusterPawn::BeginPlay()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::BeginPlay();

	if (!GDisplayCluster->IsModuleInitialized())
	{
		return;
	}

	GameMgr = GDisplayCluster->GetPrivateGameMgr();
	bIsCluster = (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);

	// No collision by default
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Enable collision if needed
	if (GameMgr && GameMgr->IsDisplayClusterActive())
	{
		const ADisplayClusterSettings* const pDisplayClusterSettings = GameMgr->GetDisplayClusterSceneSettings();

		const IPDisplayClusterClusterManager* const ClusterMgr = GDisplayCluster->GetPrivateClusterMgr();
		if (ClusterMgr && ClusterMgr->IsMaster())
		{
			if (pDisplayClusterSettings && pDisplayClusterSettings->bEnableCollisions)
			{
				// Enable collisions
				CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				// Apply collision related offset to DisplayCluster hierarchy
				const FVector CollisionOffset(0.f, 0.f, -CollisionComponent->GetUnscaledSphereRadius());
				CollisionOffsetComponent->SetRelativeLocation(CollisionOffset);
				UE_LOG(LogDisplayClusterGame, Log, TEXT("Collision offset: %s"), *CollisionOffset.ToString());
			}
		}
	}
}

void ADisplayClusterPawn::BeginDestroy()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::BeginDestroy();
}

void ADisplayClusterPawn::Tick(float DeltaSeconds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::Tick(DeltaSeconds);
}
