// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosSolverActor.h"
#include "UObject/ConstructorHelpers.h"
#include "PBDRigidsSolver.h"
#include "ChaosModule.h"

#include "Components/BillboardComponent.h"

//DEFINE_LOG_CATEGORY_STATIC(AFA_Log, NoLogging, All);

AChaosSolverActor::AChaosSolverActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimeStepMultiplier(1.f)
	, CollisionIterations(5)
	, PushOutIterations(1)
	, PushOutPairIterations(1)
	, CollisionDataSizeMax(1024)
	, CollisionDataTimeWindow(0.1f)
	, DoCollisionDataSpatialHash(true)
	, CollisionDataSpatialHashRadius(15.f)
	, MaxCollisionPerCell(1)
	, BreakingDataSizeMax(1024)
	, BreakingDataTimeWindow(0.1f)
	, DoBreakingDataSpatialHash(true)
	, BreakingDataSpatialHashRadius(15.f)
	, MaxBreakingPerCell(1)
	, TrailingDataSizeMax(1024)
	, TrailingDataTimeWindow(0.1f)
	, TrailingMinSpeedThreshold(100.f)
	, TrailingMinVolumeThreshold(1000.f)
	, HasFloor(true)
	, FloorHeight(0.f)
#if INCLUDE_CHAOS
	, Solver(nullptr)
#endif
{
#if INCLUDE_CHAOS
	// @question(Benn) : Does this need to be created on the Physics thread using a queued command?
	PhysScene = MakeShareable(new FPhysScene_Chaos());
	Solver = PhysScene->GetSolver();
#endif
	/*
	* Display icon in the editor
	*/
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		// A helper class object we use to find target UTexture2D object in resource package
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> NoteTextureObject;

		// Icon sprite category name
		FName ID_Notes;

		// Icon sprite display name
		FText NAME_Notes;

		FConstructorStatics()
			// Use helper class object to find the texture
			// "/Engine/EditorResources/S_Note" is resource path
			: NoteTextureObject(TEXT("/Engine/EditorResources/S_Note"))
			, ID_Notes(TEXT("Notes"))
			, NAME_Notes(NSLOCTEXT("SpriteCategory", "Notes", "Notes"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// We need a scene component to attach Icon sprite
	USceneComponent* SceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	SpriteComponent = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->Sprite = ConstructorStatics.NoteTextureObject.Get();		// Get the sprite texture from helper class object
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Notes;		// Assign sprite category name
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Notes;	// Assign sprite display name
		SpriteComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		SpriteComponent->Mobility = EComponentMobility::Static;
	}
#endif // WITH_EDITORONLY_DATA
}

void AChaosSolverActor::BeginPlay()
{
	Super::BeginPlay();

#if INCLUDE_CHAOS
	Chaos::IDispatcher* PhysDispatcher = PhysScene->GetDispatcher();
	if (PhysDispatcher)
	{
		PhysDispatcher->EnqueueCommand(Solver,
			[InTimeStepMultiplier = TimeStepMultiplier
			, InCollisionIterations = CollisionIterations
			, InPushOutIterations = PushOutIterations
			, InPushOutPairIterations = PushOutPairIterations
			, InCollisionDataSizeMax = CollisionDataSizeMax
			, InCollisionDataTimeWindow = CollisionDataTimeWindow
			, InDoCollisionDataSpatialHash = DoCollisionDataSpatialHash
			, InCollisionDataSpatialHashRadius = CollisionDataSpatialHashRadius
			, InMaxCollisionPerCell = MaxCollisionPerCell
			, InBreakingDataSizeMax = BreakingDataSizeMax
			, InBreakingDataTimeWindow = BreakingDataTimeWindow
			, InDoBreakingDataSpatialHash = DoBreakingDataSpatialHash
			, InBreakingDataSpatialHashRadius = BreakingDataSpatialHashRadius
			, InMaxBreakingPerCell = MaxBreakingPerCell
			, InTrailingDataSizeMax = TrailingDataSizeMax
			, InTrailingDataTimeWindow = TrailingDataTimeWindow
			, InTrailingMinSpeedThreshold = TrailingMinSpeedThreshold
			, InTrailingMinVolumeThreshold = TrailingMinVolumeThreshold
			, InHasFloor = HasFloor
			, InFloorHeight = FloorHeight]
		(Chaos::PBDRigidsSolver* InSolver)
		{
			InSolver->SetTimeStepMultiplier(InTimeStepMultiplier);
			InSolver->SetIterations(InCollisionIterations);
			InSolver->SetPushOutIterations(InPushOutIterations);
			InSolver->SetPushOutPairIterations(InPushOutPairIterations);			
			InSolver->SetMaxCollisionDataSize(InCollisionDataSizeMax);
			InSolver->SetCollisionDataTimeWindow(InCollisionDataTimeWindow);
			InSolver->SetDoCollisionDataSpatialHash(InDoCollisionDataSpatialHash);
			InSolver->SetCollisionDataSpatialHashRadius(InCollisionDataSpatialHashRadius);
			InSolver->SetMaxCollisionPerCell(InMaxCollisionPerCell);
			InSolver->SetMaxBreakingDataSize(InBreakingDataSizeMax);
			InSolver->SetBreakingDataTimeWindow(InBreakingDataTimeWindow);
			InSolver->SetDoBreakingDataSpatialHash(InDoBreakingDataSpatialHash);
			InSolver->SetBreakingDataSpatialHashRadius(InBreakingDataSpatialHashRadius);
			InSolver->SetMaxBreakingPerCell(InMaxBreakingPerCell);
			InSolver->SetMaxTrailingDataSize(InTrailingDataSizeMax);
			InSolver->SetTrailingDataTimeWindow(InTrailingDataTimeWindow);
			InSolver->SetTrailingMinSpeedThreshold(InTrailingMinSpeedThreshold);
			InSolver->SetTrailingMinVolumeThreshold(InTrailingMinVolumeThreshold);
			InSolver->SetHasFloor(InHasFloor);
			InSolver->SetFloorHeight(InFloorHeight);	
			InSolver->SetEnabled(true);
		});
	}
#endif
}

void AChaosSolverActor::EndPlay(EEndPlayReason::Type ReasonEnd)
{
#if INCLUDE_CHAOS
	Chaos::IDispatcher* PhysDispatcher = PhysScene->GetDispatcher();
	if (PhysDispatcher)
	{
		PhysDispatcher->EnqueueCommand(Solver, [](Chaos::PBDRigidsSolver* InSolver)
		{
			InSolver->Reset();
		});
	}
#endif
}

#if WITH_EDITOR
void AChaosSolverActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if INCLUDE_CHAOS
	if (Solver && PropertyChangedEvent.Property)
	{
		Chaos::IDispatcher* PhysDispatcher = PhysScene->GetDispatcher();
		if (PhysDispatcher)
		{
			if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, TimeStepMultiplier))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InTimeStepMultiplier = TimeStepMultiplier]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetTimeStepMultiplier(InTimeStepMultiplier);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, CollisionIterations))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InCollisionIterations = CollisionIterations]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetIterations(InCollisionIterations);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, PushOutIterations))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InPushOutIterations = PushOutIterations]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetPushOutIterations(InPushOutIterations);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, PushOutPairIterations))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InPushOutPairIterations = PushOutPairIterations]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetPushOutPairIterations(InPushOutPairIterations);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, CollisionDataSizeMax))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InCollisionDataSizeMax = CollisionDataSizeMax]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetMaxCollisionDataSize(InCollisionDataSizeMax);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, CollisionDataTimeWindow))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InCollisionDataTimeWindow = CollisionDataTimeWindow]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetCollisionDataTimeWindow(InCollisionDataTimeWindow);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, DoCollisionDataSpatialHash))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InDoCollisionDataSpatialHash = DoCollisionDataSpatialHash]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetDoCollisionDataSpatialHash(InDoCollisionDataSpatialHash);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, CollisionDataSpatialHashRadius))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InCollisionDataSpatialHashRadius = CollisionDataSpatialHashRadius]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetCollisionDataSpatialHashRadius(InCollisionDataSpatialHashRadius);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, MaxCollisionPerCell))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InMaxCollisionPerCell = MaxCollisionPerCell]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetMaxCollisionPerCell(InMaxCollisionPerCell);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, BreakingDataSizeMax))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InBreakingDataSizeMax = BreakingDataSizeMax]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetMaxBreakingDataSize(InBreakingDataSizeMax);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, BreakingDataTimeWindow))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InBreakingDataTimeWindow = BreakingDataTimeWindow]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetBreakingDataTimeWindow(InBreakingDataTimeWindow);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, DoBreakingDataSpatialHash))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InDoBreakingDataSpatialHash = DoBreakingDataSpatialHash]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetDoBreakingDataSpatialHash(InDoBreakingDataSpatialHash);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, BreakingDataSpatialHashRadius))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InBreakingDataSpatialHashRadius = BreakingDataSpatialHashRadius]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetBreakingDataSpatialHashRadius(InBreakingDataSpatialHashRadius);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, MaxBreakingPerCell))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InMaxBreakingPerCell = MaxBreakingPerCell]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetMaxBreakingPerCell(InMaxBreakingPerCell);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, TrailingDataSizeMax))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InTrailingDataSizeMax = TrailingDataSizeMax]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetMaxTrailingDataSize(InTrailingDataSizeMax);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, TrailingDataTimeWindow))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InTrailingDataTimeWindow = TrailingDataTimeWindow]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetTrailingDataTimeWindow(InTrailingDataTimeWindow);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, TrailingMinSpeedThreshold))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InTrailingMinSpeedThreshold = TrailingMinSpeedThreshold]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetTrailingMinSpeedThreshold(InTrailingMinSpeedThreshold);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, TrailingMinVolumeThreshold))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InTrailingMinVolumeThreshold = TrailingMinVolumeThreshold]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetTrailingMinVolumeThreshold(InTrailingMinVolumeThreshold);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, HasFloor))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InHasFloor = HasFloor]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetHasFloor(InHasFloor);
				});
			}
			else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AChaosSolverActor, FloorHeight))
			{
				PhysDispatcher->EnqueueCommand(Solver, [InFloorHeight = FloorHeight]
				(Chaos::PBDRigidsSolver* InSolver)
				{
					InSolver->SetFloorHeight(InFloorHeight);
				});
			}
		}
	}
#endif
}
#endif

