// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/StaticMeshSimulationComponent.h"

#include "Async/ParallelFor.h"
#include "Components/BoxComponent.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/StaticMeshSolverCallbacks.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/DebugDrawQueue.h"
#include "DrawDebugHelpers.h"
#include "GeometryCollection/StaticMeshSimulationComponentPhysicsProxy.h"
#include "Modules/ModuleManager.h"
#include "ChaosSolversModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"


DEFINE_LOG_CATEGORY_STATIC(UStaticMeshSimulationComponentLogging, NoLogging, All);

UStaticMeshSimulationComponent::UStaticMeshSimulationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Simulating(false)
	, ObjectType(EObjectTypeEnum::Chaos_Object_Dynamic)
	, Mass(1.0)
	, CollisionType(ECollisionTypeEnum::Chaos_Surface_Volumetric)
	, InitialVelocityType(EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
	, Friction(0.8)
	, Bouncyness(0.0)
	, ChaosSolverActor(nullptr)
{
}

void UStaticMeshSimulationComponent::BeginPlay()
{
	Super::BeginPlay();

#if INCLUDE_CHAOS
	// #BG TODO - Find a better place for this - something that gets hit once on scene begin.
	FChaosSolversModule* ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
	Chaos::PBDRigidsSolver* Solver = ChaosSolverActor != nullptr ? ChaosSolverActor->GetSolver() : FPhysScene_Chaos::GetInstance()->GetSolver();
	ChaosModule->GetDispatcher()->EnqueueCommand(Solver,
		[InFriction = Friction
		, InRestitution = Bouncyness
		, InCollisionIters = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionIterations : 5
		, InPushOutIters = ChaosSolverActor != nullptr ? ChaosSolverActor->PushOutIterations : 1
		, InPushOutPairIters = ChaosSolverActor != nullptr ? ChaosSolverActor->PushOutPairIterations : 1
		, InCollisionDataSizeMax = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionDataSizeMax : 1024
		, InCollisionDataTimeWindow = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionDataTimeWindow : 0.1
		, InHasFloor = ChaosSolverActor != nullptr ? ChaosSolverActor->HasFloor : true
		, InFloorHeight = ChaosSolverActor != nullptr ? ChaosSolverActor->FloorHeight : 0.f]
		(Chaos::PBDRigidsSolver* InSolver)
	{
		InSolver->SetFriction(InFriction);
		InSolver->SetRestitution(InRestitution);
		InSolver->SetIterations(InCollisionIters);
		InSolver->SetPushOutIterations(InPushOutIters);
		InSolver->SetPushOutPairIterations(InPushOutPairIters);
		InSolver->SetMaxCollisionDataSize(InCollisionDataSizeMax);
		InSolver->SetCollisionDataTimeWindow(InCollisionDataTimeWindow);
		InSolver->SetHasFloor(InHasFloor);
		InSolver->SetFloorHeight(InFloorHeight);
		InSolver->SetEnabled(true);
	});
#endif
}

#if INCLUDE_CHAOS
Chaos::PBDRigidsSolver* GetSolver(const UStaticMeshSimulationComponent& StaticMeshSimulationComponent)
{
	return	StaticMeshSimulationComponent.ChaosSolverActor != nullptr ? StaticMeshSimulationComponent.ChaosSolverActor->GetSolver() : FPhysScene_Chaos::GetInstance()->GetSolver();
}
#endif

void UStaticMeshSimulationComponent::EndPlay(EEndPlayReason::Type ReasonEnd)
{
#if INCLUDE_CHAOS
	// @todo : This needs to be removed from the component. 
	Chaos::PBDRigidsSolver* Solver = GetSolver(*this);
	ensure(Solver);

	FChaosSolversModule* ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
	ChaosModule->GetDispatcher()->EnqueueCommand(Solver, [](Chaos::PBDRigidsSolver* InSolver)
	{
		InSolver->Reset();
	});
#endif

	Super::EndPlay(ReasonEnd);
}

void UStaticMeshSimulationComponent::OnCreatePhysicsState()
{
	// Skip the chain - don't care about body instance setup
	UActorComponent::OnCreatePhysicsState();

#if INCLUDE_CHAOS
	const bool bValidWorld = GetWorld() && GetWorld()->IsGameWorld();

	// Need to see if we actually have a target for the component
	AActor* OwningActor = GetOwner();
	UStaticMeshComponent* TargetComponent = OwningActor->FindComponentByClass<UStaticMeshComponent>();

	if(bValidWorld && TargetComponent)
	{
		auto InitFunc = [this, TargetComponent](FStaticMeshSolverCallbacks::Params& InParams)
		{
			GetPathName(this, InParams.Name);
			InParams.InitialTransform = GetOwner()->GetTransform();
			if(InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
			{
				InParams.InitialLinearVelocity = InitialLinearVelocity;
				InParams.InitialAngularVelocity = InitialAngularVelocity;
			}

			InParams.Mass = Mass;
			InParams.ObjectType = ObjectType;

			UStaticMesh* StaticMesh = TargetComponent->GetStaticMesh();

			if(StaticMesh)
			{
				FStaticMeshVertexBuffers& VB = StaticMesh->RenderData->LODResources[0].VertexBuffers;
				const int32 NumVerts = VB.PositionVertexBuffer.GetNumVertices();
				InParams.MeshVertexPositions.Reset(NumVerts);

				for(int32 VertexIndex = 0; VertexIndex < NumVerts; ++VertexIndex)
				{
					InParams.MeshVertexPositions.Add(VB.PositionVertexBuffer.VertexPosition(VertexIndex));
				}

				if(NumVerts > 0)
				{
					TargetComponent->SetMobility(EComponentMobility::Movable);
					InParams.bSimulating = Simulating;
				}
			}
		};

		auto SyncFunc = [TargetComponent](const FTransform& InTransform)
		{
			TargetComponent->SetWorldTransform(InTransform);
		};

		PhysicsProxy = new FStaticMeshSimulationComponentPhysicsProxy(InitFunc, SyncFunc);

		TSharedPtr<FPhysScene_Chaos> Scene = GetPhysicsScene();
		Scene->AddProxy(PhysicsProxy);
	}
#endif
}

void UStaticMeshSimulationComponent::OnDestroyPhysicsState()
{
	UActorComponent::OnDestroyPhysicsState();

#if INCLUDE_CHAOS
	if(PhysicsProxy)
	{
		// Handle scene remove, right now we rely on the reset of EndPlay to clean up
		TSharedPtr<FPhysScene_Chaos> Scene = GetPhysicsScene();
		Scene->RemoveProxy(PhysicsProxy);

		// Discard the pointer
		PhysicsProxy = nullptr;
	}
#endif
}

bool UStaticMeshSimulationComponent::ShouldCreatePhysicsState() const
{
	return true;
}

bool UStaticMeshSimulationComponent::HasValidPhysicsState() const
{
	return PhysicsProxy != nullptr;
}

#if INCLUDE_CHAOS
const TSharedPtr<FPhysScene_Chaos> UStaticMeshSimulationComponent::GetPhysicsScene() const
{ 
	if (ChaosSolverActor)
	{
		return ChaosSolverActor->GetPhysicsScene();
	}
	else
	{
		return FPhysScene_Chaos::GetInstance();
	}
}
#endif

