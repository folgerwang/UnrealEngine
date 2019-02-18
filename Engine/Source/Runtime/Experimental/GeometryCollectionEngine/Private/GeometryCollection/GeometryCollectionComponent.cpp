// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionComponent.h"

#include "Async/ParallelFor.h"
#include "Components/BoxComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponentPluginPrivate.h"
#include "GeometryCollection/GeometryCollectionSceneProxy.h"
#include "GeometryCollection/GeometryCollectionSQAccelerator.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionPhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionCache.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/DebugDrawQueue.h"
#include "DrawDebugHelpers.h"
#include "Modules/ModuleManager.h"
#include "ChaosSolversModule.h"
#include "ChaosStats.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Editor.h"
#endif


DEFINE_LOG_CATEGORY_STATIC(UGCC_LOG, NoLogging, All);

#if INCLUDE_CHAOS && WITH_PHYSX
FGeometryCollectionSQAccelerator GlobalGeomCollectionAccelerator;	//todo(ocohen): proper lifetime management needed

void HackRegisterGeomAccelerator(UGeometryCollectionComponent& Component)
{
	if (UWorld* World = Component.GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			PhysScene->GetSQAcceleratorUnion()->AddSQAccelerator(&GlobalGeomCollectionAccelerator);
		}
	}
}
#endif

FGeomComponentCacheParameters::FGeomComponentCacheParameters()
	: CacheMode(EGeometryCollectionCacheType::None)
	, TargetCache(nullptr)
	, ReverseCacheBeginTime(0.0f)
	, SaveCollisionData(false)
	, CollisionDataMaxSize(1024)
	, DoCollisionDataSpatialHash(true)
	, SpatialHashRadius(15.f)
	, MaxCollisionPerCell(1)
	, SaveTrailingData(false)
	, TrailingDataSizeMax(1024)
	, TrailingMinSpeedThreshold(100.f)
	, TrailingMinVolumeThreshold(10000.f)
{

}

UGeometryCollectionComponent::UGeometryCollectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ChaosSolverActor(nullptr)
	, Simulating(true)
	, ObjectType(EObjectTypeEnum::Chaos_Object_Dynamic)
	, EnableClustering(true)
	, MaxClusterLevel(100)
	, DamageThreshold({250.0})
	, CollisionType(ECollisionTypeEnum::Chaos_Surface_Volumetric)
	, ImplicitType(EImplicitTypeEnum::Chaos_Implicit_Cube)
	, MinLevelSetResolution(5)
	, MaxLevelSetResolution(10)
	, MassAsDensity(false)
	, Mass(1.0)
	, MinimumMassClamp(0.1)
	, CollisionParticlesFraction(1.0)
	, Friction(0.8)
	, Bouncyness(0.0)
	, LinearSleepingThreshold(1.f)
	, AngularSleepingThreshold(1.f)
	, InitialVelocityType(EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
	, InitialLinearVelocity(0.f, 0.f, 0.f)
	, InitialAngularVelocity(0.f, 0.f, 0.f)
	, bRenderStateDirty(true)
	, ShowBoneColors(false)
	, ShowSelectedBones(false)
	, ViewLevel(-1)
	, PhysicsProxy(nullptr)
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	, EditorActor(nullptr)
#endif
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;

	DummyBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("DummyBoxComponent"));	//TODO(ocohen):this is just a placeholder for now so we can keep using physx API for SQ
	DummyBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DummyBoxComponent->SetCollisionResponseToAllChannels(ECR_Block);
	DummyBoxComponent->SetWorldLocation(FVector(0.f, 0.f, -9999999.f));		//we rely on the shape's collision filter so we need to hide this thing. All of this needs to go away when refactor is finished
	//DummyBoxComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
}

#if INCLUDE_CHAOS
Chaos::PBDRigidsSolver* GetSolver(const UGeometryCollectionComponent& GeometryCollectionComponent)
{
	return	GeometryCollectionComponent.ChaosSolverActor != nullptr ? GeometryCollectionComponent.ChaosSolverActor->GetSolver() : FPhysScene_Chaos::GetInstance()->GetSolver();
}
#endif

void UGeometryCollectionComponent::BeginPlay()
{
	Super::BeginPlay();
#if INCLUDE_CHAOS
#if WITH_PHYSX
	GlobalGeomCollectionAccelerator.AddComponent(this);
	HackRegisterGeomAccelerator(*this);
#endif

	if (DynamicCollection)
	{
		//////////////////////////////////////////////////////////////////////////
		// Commenting out these callbacks for now due to the threading model. The callbacks here
		// expect the rest collection to be mutable which is not the case when running in multiple
		// threads. Ideally we have some separate animation collection or track that we cache to
		// without affecting the data we've dispatched to the physics thread
		//////////////////////////////////////////////////////////////////////////
		// ---------- SolverCallbacks->SetResetAnimationCacheFunction([&]()
		// ---------- {
		// ---------- 	FGeometryCollectionEdit Edit = EditRestCollection();
		// ---------- 	Edit.GetRestCollection()->RecordedData.SetNum(0);
		// ---------- });
		// ---------- SolverCallbacks->SetUpdateTransformsFunction([&](const TArrayView<FTransform>&)
		// ---------- {
		// ---------- 	// todo : Move the update to the array passed here...
		// ---------- });
		// ---------- 
		// ---------- SolverCallbacks->SetUpdateRestStateFunction([&](const int32 & CurrentFrame, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles)
		// ---------- {
		// ---------- 	FGeometryCollectionEdit Edit = EditRestCollection();
		// ---------- 	UGeometryCollection * RestCollection = Edit.GetRestCollection();
		// ---------- 	check(RestCollection);
		// ---------- 
		// ---------- 	if (CurrentFrame >= RestCollection->RecordedData.Num())
		// ---------- 	{
		// ---------- 		RestCollection->RecordedData.SetNum(CurrentFrame + 1);
		// ---------- 		RestCollection->RecordedData[CurrentFrame].SetNum(RigidBodyID.Num());
		// ---------- 		ParallelFor(RigidBodyID.Num(), [&](int32 i)
		// ---------- 		{
		// ---------- 			if (!Hierarchy[i].Children.Num())
		// ---------- 			{
		// ---------- 				RestCollection->RecordedData[CurrentFrame][i].SetTranslation(Particles.X(RigidBodyID[i]));
		// ---------- 				RestCollection->RecordedData[CurrentFrame][i].SetRotation(Particles.R(RigidBodyID[i]));
		// ---------- 			}
		// ---------- 			else
		// ---------- 			{
		// ---------- 				RestCollection->RecordedData[CurrentFrame][i].SetTranslation(FVector::ZeroVector);
		// ---------- 				RestCollection->RecordedData[CurrentFrame][i].SetRotation(FQuat::Identity);
		// ---------- 			}
		// ---------- 		});
		// ---------- 	}
		// ---------- });
		//////////////////////////////////////////////////////////////////////////

		// #BG TODO - Find a better place for this - something that gets hit once on scene begin.
		FChaosSolversModule* ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
		Chaos::PBDRigidsSolver* Solver = GetSolver(*this);
		ensure(Solver);

		ChaosModule->GetDispatcher()->EnqueueCommand(Solver,
			[ InFriction = Friction
			, InRestitution = Bouncyness
			, InLinearSleepThreshold = LinearSleepingThreshold
			, InAngularSleepThreshold = AngularSleepingThreshold
			, InDtMultiplier = ChaosSolverActor != nullptr ? ChaosSolverActor->TimeStepMultiplier : 1.f
			, InCollisionIters = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionIterations : 5
			, InPushOutIters = ChaosSolverActor != nullptr ? ChaosSolverActor->PushOutIterations : 1
			, InPushOutPairIters = ChaosSolverActor != nullptr ? ChaosSolverActor->PushOutPairIterations : 1
			, InCollisionDataSizeMax = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionDataSizeMax : 1024
			, InCollisionDataTimeWindow = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionDataTimeWindow : 0.1f
			, InDoCollisionDataSpatialHash = ChaosSolverActor != nullptr ? ChaosSolverActor->DoCollisionDataSpatialHash : true
			, InCollisionDataSpatialHashRadius = ChaosSolverActor != nullptr ? ChaosSolverActor->CollisionDataSpatialHashRadius : 15.f
			, InMaxCollisionPerCell = ChaosSolverActor != nullptr ? ChaosSolverActor->MaxCollisionPerCell : 1		
			, InTrailingDataSizeMax = ChaosSolverActor != nullptr ? ChaosSolverActor->TrailingDataSizeMax : 1024
			, InTrailingDataTimeWindow = ChaosSolverActor != nullptr ? ChaosSolverActor->TrailingDataTimeWindow : 0.1f
			, InTrailingMinSpeedThreshold = ChaosSolverActor != nullptr ? ChaosSolverActor->TrailingMinSpeedThreshold : 100.f
			, InTrailingMinVolumeThreshold = ChaosSolverActor != nullptr ? ChaosSolverActor->TrailingMinVolumeThreshold : 10000.f
			, InHasFloor = ChaosSolverActor != nullptr ? ChaosSolverActor->HasFloor : true
			, InFloorHeight = ChaosSolverActor != nullptr ? ChaosSolverActor->FloorHeight : 0.f]
			(Chaos::PBDRigidsSolver* InSolver)
		{
			InSolver->SetFriction(InFriction);
			InSolver->SetRestitution(InRestitution);
			InSolver->SetSleepThresholds(InLinearSleepThreshold, InAngularSleepThreshold);
			InSolver->SetTimeStepMultiplier(InDtMultiplier);
			InSolver->SetIterations(InCollisionIters);
			InSolver->SetPushOutIterations(InPushOutIters);
			InSolver->SetPushOutPairIterations(InPushOutPairIters);
			InSolver->SetMaxCollisionDataSize(InCollisionDataSizeMax);
			InSolver->SetCollisionDataTimeWindow(InCollisionDataTimeWindow);
			InSolver->SetDoCollisionDataSpatialHash(InDoCollisionDataSpatialHash);
			InSolver->SetCollisionDataSpatialHashRadius(InCollisionDataSpatialHashRadius);
			InSolver->SetMaxCollisionPerCell(InMaxCollisionPerCell);
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


void UGeometryCollectionComponent::EndPlay(EEndPlayReason::Type ReasonEnd)
{
#if INCLUDE_CHAOS
#if WITH_PHYSX
	GlobalGeomCollectionAccelerator.RemoveComponent(this);
#endif

	Chaos::PBDRigidsSolver* Solver = GetSolver(*this);
	ensure(Solver);

	FChaosSolversModule* ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
	ChaosModule->GetDispatcher()->EnqueueCommand(Solver, [](Chaos::PBDRigidsSolver* InSolver)
	{
		InSolver->Reset();
	});

#endif

#if WITH_EDITOR && WITH_EDITORONLY_DATA
	// Track our editor component if needed for syncing simulations back from PIE on shutdown
	EditorActor = EditorUtilities::GetEditorWorldCounterpartActor(GetTypedOuter<AActor>());
#endif

	Super::EndPlay(ReasonEnd);
}

FBoxSphereBounds UGeometryCollectionComponent::CalcBounds(const FTransform& LocalToWorldIn) const
{
	SCOPE_CYCLE_COUNTER(STAT_GCUpdateBounds);
	
	if (DynamicCollection && DynamicCollection->HasVisibleGeometry())
	{
		FGeometryCollection* Collection = DynamicCollection->GetGeometryCollection().Get();
		check(Collection);

		FBox BoundingBox(ForceInit);

		const TManagedArray<FBox>& BoundingBoxes = *Collection->BoundingBox;
		const TManagedArray<int32>& TransformIndices = *Collection->TransformIndex;
		const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *Collection->BoneHierarchy;

		TArray<FTransform> Transforms;
		GeometryCollectionAlgo::GlobalMatrices(Collection, Transforms);

		const int32 NumBoxes = BoundingBoxes.Num();
		for(int32 BoxIdx = 0; BoxIdx < NumBoxes; ++BoxIdx)
		{
			int32 TransformIndex = TransformIndices[BoxIdx];

			if(BoneHierarchyArray[TransformIndex].IsGeometry())
			{
				BoundingBox += BoundingBoxes[BoxIdx].TransformBy(Transforms[TransformIndices[BoxIdx]] * LocalToWorldIn);
			}
		}

		return FBoxSphereBounds(BoundingBox);
	}
	return FBoxSphereBounds(ForceInitToZero);
}

void UGeometryCollectionComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	if (SceneProxy && DynamicCollection && DynamicCollection->HasVisibleGeometry())
	{
		FGeometryCollectionConstantData * ConstantData = ::new FGeometryCollectionConstantData;
		InitConstantData(ConstantData);

		FGeometryCollectionDynamicData * DynamicData = ::new FGeometryCollectionDynamicData;
		InitDynamicData(DynamicData);

		// Enqueue command to send to render thread
		FGeometryCollectionSceneProxy* GeometryCollectionSceneProxy = (FGeometryCollectionSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(FSendGeometryCollectionData)(
			[GeometryCollectionSceneProxy, ConstantData, DynamicData](FRHICommandListImmediate& RHICmdList)
			{
				GeometryCollectionSceneProxy->SetConstantData_RenderThread(ConstantData);
				GeometryCollectionSceneProxy->SetDynamicData_RenderThread(DynamicData);
			});
	}
}


FPrimitiveSceneProxy* UGeometryCollectionComponent::CreateSceneProxy()
{
	if (DynamicCollection)
	{
		return new FGeometryCollectionSceneProxy(this);
	}
	return nullptr;
}

bool UGeometryCollectionComponent::ShouldCreatePhysicsState() const
{
	// Geometry collections always create physics state, not relying on the
	// underlying implementation that requires the body instance to decide
	return true;
}

bool UGeometryCollectionComponent::HasValidPhysicsState() const
{
	return PhysicsProxy != nullptr;
}

void UGeometryCollectionComponent::InitConstantData(FGeometryCollectionConstantData * ConstantData)
{
	check(ConstantData);
	check(DynamicCollection);
	FGeometryCollection* Collection = DynamicCollection->GetGeometryCollection().Get();
	check(Collection);

	int32 NumPoints = Collection->NumElements(FGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& Vertex = *Collection->Vertex;
	TManagedArray<int32>& BoneMap = *Collection->BoneMap;
	TManagedArray<FVector>& TangentU = *Collection->TangentU;
	TManagedArray<FVector>& TangentV = *Collection->TangentV;
	TManagedArray<FVector>& Normal = *Collection->Normal;
	TManagedArray<FVector2D>& UV = *Collection->UV;
	TManagedArray<FLinearColor>& Color = *Collection->Color;
	TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchy = *Collection->BoneHierarchy;
	TManagedArray<FLinearColor>& BoneColors = *Collection->BoneColor;

	ConstantData->Vertices.AddUninitialized(NumPoints);
	ConstantData->BoneMap.AddUninitialized(NumPoints);
	ConstantData->TangentU.AddUninitialized(NumPoints);
	ConstantData->TangentV.AddUninitialized(NumPoints);
	ConstantData->Normals.AddUninitialized(NumPoints);
	ConstantData->UVs.AddUninitialized(NumPoints);
	ConstantData->Colors.AddUninitialized(NumPoints);
	ConstantData->BoneColors.AddUninitialized(NumPoints);

	ParallelFor(NumPoints, [&](int32 PointIdx)
	{
		ConstantData->Vertices[PointIdx] = Vertex[PointIdx];
		ConstantData->BoneMap[PointIdx] = BoneMap[PointIdx];
		ConstantData->TangentU[PointIdx] = TangentU[PointIdx];
		ConstantData->TangentV[PointIdx] = TangentV[PointIdx];
		ConstantData->Normals[PointIdx] = Normal[PointIdx];
		ConstantData->UVs[PointIdx] = UV[PointIdx];
		ConstantData->Colors[PointIdx] = Color[PointIdx];
		int32 BoneIndex = ConstantData->BoneMap[PointIdx];
		ConstantData->BoneColors[PointIdx] = BoneColors[BoneIndex];
	});

	int32 NumIndices = 0;
	TManagedArray<FIntVector>& Indices = *Collection->Indices;
	TManagedArray<bool>& Visible = *Collection->Visible;
	TManagedArray<int32>& MaterialIndex = *Collection->MaterialIndex;
	for (int vdx = 0; vdx < Collection->NumElements(FGeometryCollection::FacesGroup); vdx++)
	{
		NumIndices += static_cast<int>(Visible[vdx]);
	}

	ConstantData->Indices.AddUninitialized(NumIndices);
	for (int IndexIdx = 0, cdx = 0; IndexIdx < Collection->NumElements(FGeometryCollection::FacesGroup); IndexIdx++)
	{
		if (Visible[ MaterialIndex[IndexIdx] ])
		{
			ConstantData->Indices[cdx++] = Indices[ MaterialIndex[IndexIdx] ];
		}
	}

	// We need to correct the section index start point & number of triangles since only the visible ones have been copied across in the code above
	int32 NumMaterialSections = Collection->NumElements(FGeometryCollection::MaterialGroup);
	ConstantData->Sections.AddUninitialized(NumMaterialSections);
	TManagedArray<FGeometryCollectionSection>& Sections = *Collection->Sections;
	for (int sdx = 0; sdx < NumMaterialSections; sdx++)
	{
		FGeometryCollectionSection Section = Sections[sdx]; // deliberate copy

		for (int32 TriangleIndex = 0; TriangleIndex < Sections[sdx].FirstIndex / 3; TriangleIndex++)
		{
			if (!Visible[MaterialIndex[TriangleIndex]])
				Section.FirstIndex-=3;
		}

		for (int32 TriangleIndex = 0; TriangleIndex < Sections[sdx].NumTriangles; TriangleIndex++)
		{
			if (!Visible[MaterialIndex[Sections[sdx].FirstIndex / 3 + TriangleIndex]])
				Section.NumTriangles--;
		}

		ConstantData->Sections[sdx] = Section;
	}

}

void UGeometryCollectionComponent::InitDynamicData(FGeometryCollectionDynamicData * DynamicData)
{
	check(DynamicData);
	check(DynamicCollection);
	FGeometryCollection* Collection = DynamicCollection->GetGeometryCollection().Get();
	check(Collection);

	TArray<FTransform> GlobalMatrices;
	GeometryCollectionAlgo::GlobalMatrices(Collection, GlobalMatrices);

	int32 NumTransforms = Collection->NumElements(FGeometryCollection::TransformGroup);
	DynamicData->Transforms.AddUninitialized(NumTransforms);

	check(GlobalMatrices.Num() == NumTransforms);
	//ParallelFor(NumTransforms, [&](int32 MatrixIdx)
	for(int MatrixIdx=0;MatrixIdx<NumTransforms;MatrixIdx++)
	{
		DynamicData->Transforms[MatrixIdx] = GlobalMatrices[MatrixIdx].ToMatrixWithScale();
		UE_LOG(UGCC_LOG, Log, TEXT("GeometryCollectionComponent::InitDynamicData::[%p][%d](%s)(%s)"), Collection, MatrixIdx,
			*GlobalMatrices[MatrixIdx].GetTranslation().ToString(), *GlobalMatrices[MatrixIdx].GetRotation().ToString());
	}//);
}

void UGeometryCollectionComponent::ForceInitRenderData()
{
	// Reset SceneProxy state to reflect the change in visible geometry
	FGeometryCollectionConstantData * ConstantData = ::new FGeometryCollectionConstantData;
	InitConstantData(ConstantData);

	FGeometryCollectionDynamicData * DynamicData = ::new FGeometryCollectionDynamicData;
	InitDynamicData(DynamicData);

	// Enqueue command to send to render thread
	FGeometryCollectionSceneProxy* GeometryCollectionSceneProxy = (FGeometryCollectionSceneProxy*)SceneProxy;
	ENQUEUE_RENDER_COMMAND(FSendGeometryCollectionData)(
		[GeometryCollectionSceneProxy, ConstantData, DynamicData](FRHICommandListImmediate& RHICmdList)
		{
			GeometryCollectionSceneProxy->SetConstantData_RenderThread(ConstantData, true);
			GeometryCollectionSceneProxy->SetDynamicData_RenderThread(DynamicData);
		});
}


#if CHAOS_DEBUG_DRAW
void DebugDrawChaos(UWorld* World)
{	
	if (!World->IsGameWorld())
	{
		return;
	}

	using namespace Chaos;
	TArray<FLatentDrawCommand> LatenetDrawCommands;

	FDebugDrawQueue::GetInstance().ExtractAllElements(LatenetDrawCommands);
	for (const FLatentDrawCommand& Command : LatenetDrawCommands)
	{
		switch (Command.Type)
		{
		case FLatentDrawCommand::EDrawType::Point:
		{
			DrawDebugPoint(World, Command.LineStart, Command.Thickness, Command.Color, Command.bPersistentLines, Command.LifeTime, Command.DepthPriority);
			break;
		}
		case FLatentDrawCommand::EDrawType::Line:
		{
			DrawDebugLine(World, Command.LineStart, Command.LineEnd, Command.Color, Command.bPersistentLines, Command.LifeTime, Command.DepthPriority, Command.Thickness);
			break;
		}
		case FLatentDrawCommand::EDrawType::DirectionalArrow:
		{
			DrawDebugDirectionalArrow(World, Command.LineStart, Command.LineEnd, Command.ArrowSize, Command.Color, Command.bPersistentLines, Command.LifeTime, Command.DepthPriority, Command.Thickness);
			break;
		}
		case FLatentDrawCommand::EDrawType::Sphere:
		{
			DrawDebugSphere(World, Command.LineStart, Command.Radius, Command.Segments, Command.Color, Command.bPersistentLines, Command.LifeTime, Command.DepthPriority, Command.Thickness);
			break;
		}
		case FLatentDrawCommand::EDrawType::Box:
		{
			DrawDebugBox(World, Command.Center, Command.Extent, Command.Rotation, Command.Color, Command.bPersistentLines, Command.LifeTime, Command.DepthPriority, Command.Thickness);
			break;
		}
		default:
			break;
		}
	}
}
#endif

void UGeometryCollectionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	//UE_LOG(UGCC_LOG, Log, TEXT("GeometryCollectionComponent[%p]::TickComponent()"), this);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bRenderStateDirty && DynamicCollection)
	{
		check(DynamicCollection->GetGeometryCollection());
		if (DynamicCollection->HasVisibleGeometry() || DynamicCollection->GetGeometryCollection()->IsDirty())
		{
			// #BGallagher below is required for dynamic bounds update but
			// it's too slow on large components currently, re-enable when faster
			//MarkRenderTransformDirty();
			MarkRenderDynamicDataDirty();
			bRenderStateDirty = false;
			DynamicCollection->GetGeometryCollection()->MakeClean();
		}
	}

#if CHAOS_DEBUG_DRAW
	using namespace Chaos;

	// General debug drawing
	if (FDebugDrawQueue::EnableDebugDrawing)
	{
		if (UWorld* World = GetWorld())
		{
			DebugDrawChaos(World);
		}
	}
#endif
}

void UGeometryCollectionComponent::OnRegister()
{
	//UE_LOG(UGCC_LOG, Log, TEXT("GeometryCollectionComponent[%p]::OnRegister()[%p]"), this,RestCollection );
	Super::OnRegister();
	ResetDynamicCollection();
}

void UGeometryCollectionComponent::ResetDynamicCollection()
{
	//UE_LOG(UGCC_LOG, Log, TEXT("GeometryCollectionComponent[%p]::ResetDynamicCollection()"), static_cast<const void*>(this));
	if (RestCollection)
	{
		DynamicCollection = NewObject<UGeometryCollection>(this);
		DynamicCollection->Initialize(*RestCollection->GetGeometryCollection());
		DynamicCollection->GetGeometryCollection()->LocalizeAttribute("Transform", FGeometryCollection::TransformGroup);
		DynamicCollection->GetGeometryCollection()->LocalizeAttribute("BoneHierarchy", FGeometryCollection::TransformGroup);
		SetRenderStateDirty();
	}
}

void UGeometryCollectionComponent::OnCreatePhysicsState()
{
	// Skip the chain - don't care about body instance setup
	UActorComponent::OnCreatePhysicsState();

#if WITH_EDITOR && WITH_EDITORONLY_DATA
	EditorActor = nullptr;
#endif

#if INCLUDE_CHAOS
	const bool bValidWorld = GetWorld() && GetWorld()->IsGameWorld();
	const bool bValidCollection = DynamicCollection && DynamicCollection->GetGeometryCollection().Get()->Transform->Num() > 0;
	if(bValidWorld && bValidCollection)
	{
		auto InitFunc = [this](FSimulationParameters& InParams, FFieldSystem& InFieldSystem)
		{
			InParams.RestCollection = GetRestCollection()->GetGeometryCollection().Get();
			InParams.Simulating = Simulating;
			InParams.WorldTransform = GetComponentToWorld();
			InParams.ObjectType = ObjectType;
			InParams.CollisionType = CollisionType;
			InParams.ImplicitType = ImplicitType;
			InParams.MinLevelSetResolution = MinLevelSetResolution;
			InParams.MaxLevelSetResolution = MaxLevelSetResolution;
			InParams.EnableClustering = EnableClustering;
			InParams.MaxClusterLevel = MaxClusterLevel;
			InParams.DamageThreshold = DamageThreshold;
			InParams.MassAsDensity = MassAsDensity;
			InParams.Mass = Mass;
			InParams.MinimumMassClamp = MinimumMassClamp;
			InParams.CollisionParticlesFraction = CollisionParticlesFraction;
			InParams.Friction = Friction;
			InParams.Bouncyness = Bouncyness;
			InParams.InitialVelocityType = InitialVelocityType;
			InParams.InitialLinearVelocity = InitialLinearVelocity;
			InParams.InitialAngularVelocity = InitialAngularVelocity;
			InParams.bClearCache = true;
			InParams.CacheType = CacheParameters.CacheMode;
			InParams.ReverseCacheBeginTime = CacheParameters.ReverseCacheBeginTime;
			InParams.SaveCollisionData = CacheParameters.SaveCollisionData;
			InParams.CollisionDataMaxSize = CacheParameters.CollisionDataMaxSize;
			InParams.DoCollisionDataSpatialHash = CacheParameters.DoCollisionDataSpatialHash;
			InParams.SpatialHashRadius = CacheParameters.SpatialHashRadius;
			InParams.MaxCollisionPerCell = CacheParameters.MaxCollisionPerCell;
			InParams.SaveTrailingData = CacheParameters.SaveTrailingData;
			InParams.TrailingDataSizeMax = CacheParameters.TrailingDataSizeMax;
			InParams.TrailingMinSpeedThreshold = CacheParameters.TrailingMinSpeedThreshold;
			InParams.TrailingMinVolumeThreshold = CacheParameters.TrailingMinVolumeThreshold;

			InParams.RecordedTrack = (InParams.IsCachePlaying() && CacheParameters.TargetCache) ? CacheParameters.TargetCache->GetData() : nullptr;

			if(FieldSystem && FieldSystem->GetFieldSystemComponent() 
				&& FieldSystem->GetFieldSystemComponent()->GetFieldSystem() )
			{
				InFieldSystem.BuildFrom(FieldSystem->GetFieldSystemComponent()->GetFieldSystem()->GetFieldData());
			}
		};

		auto CacheSyncFunc = [this](const TManagedArray<int32>& BodyIds)
		{
			RigidBodyIds.Init(BodyIds);
		};

		auto FinalSyncFunc = [this](const FRecordedTransformTrack& InTrack)
		{
#if WITH_EDITOR && WITH_EDITORONLY_DATA
			if(CacheParameters.CacheMode == EGeometryCollectionCacheType::Record && InTrack.Records.Num() > 0)
			{
				Modify();
				if(!CacheParameters.TargetCache)
				{
					CacheParameters.TargetCache = UGeometryCollectionCache::CreateCacheForCollection(RestCollection);
				}

				if(CacheParameters.TargetCache)
				{
					// Queue this up to be dirtied after PIE ends
					TSharedPtr<FPhysScene_Chaos> Scene = GetPhysicsScene();
					
					CacheParameters.TargetCache->PreEditChange(nullptr);
					CacheParameters.TargetCache->Modify();
					CacheParameters.TargetCache->SetFromRawTrack(InTrack);
					CacheParameters.TargetCache->PostEditChange();

					Scene->AddPieModifiedObject(CacheParameters.TargetCache);

					if(EditorActor)
					{
						UGeometryCollectionComponent* EditorComponent = Cast<UGeometryCollectionComponent>(EditorUtilities::FindMatchingComponentInstance(this, EditorActor));

						if(EditorComponent)
						{
							EditorComponent->PreEditChange(FindField<UProperty>(EditorComponent->GetClass(), GET_MEMBER_NAME_CHECKED(UGeometryCollectionComponent, CacheParameters)));
							EditorComponent->Modify();

							EditorComponent->CacheParameters.TargetCache = CacheParameters.TargetCache;

							EditorComponent->PostEditChange();

							Scene->AddPieModifiedObject(EditorComponent);
							Scene->AddPieModifiedObject(EditorActor);
						}

						EditorActor = nullptr;
					}
				}
			}
#endif
		};

		PhysicsProxy = new FGeometryCollectionPhysicsProxy(DynamicCollection->GetGeometryCollection().Get(), InitFunc, CacheSyncFunc, FinalSyncFunc);

		TSharedPtr<FPhysScene_Chaos> Scene = GetPhysicsScene();
		Scene->AddProxy(PhysicsProxy);
	}
#endif
}

void UGeometryCollectionComponent::OnDestroyPhysicsState()
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

void UGeometryCollectionComponent::SendRenderDynamicData_Concurrent()
{
	//UE_LOG(UGCC_LOG, Log, TEXT("GeometryCollectionComponent[%p]::SendRenderDynamicData_Concurrent()"), this);
	Super::SendRenderDynamicData_Concurrent();
	if (SceneProxy)
	{
		if (DynamicCollection)
		{
			FGeometryCollectionDynamicData * DynamicData = ::new FGeometryCollectionDynamicData;
			InitDynamicData(DynamicData);

			// Enqueue command to send to render thread
			FGeometryCollectionSceneProxy* GeometryCollectionSceneProxy = (FGeometryCollectionSceneProxy*)SceneProxy;
			ENQUEUE_RENDER_COMMAND(FSendGeometryCollectionData)(
				[GeometryCollectionSceneProxy, DynamicData](FRHICommandListImmediate& RHICmdList)
				{
					GeometryCollectionSceneProxy->SetDynamicData_RenderThread(DynamicData);
				});
		}
	}
}

void UGeometryCollectionComponent::SetRestCollection(UGeometryCollection * RestCollectionIn)
{
	//UE_LOG(UGCC_LOG, Log, TEXT("GeometryCollectionComponent[%p]::SetRestCollection()"), this);
	if (RestCollectionIn)
	{
		RestCollection = RestCollectionIn;

		// All rest states are shared across components and will have a AssetScope. 
		RestCollection->GetGeometryCollection()->SetArrayScopes(FManagedArrayCollection::EArrayScope::FScopeShared);
		ResetDynamicCollection();
		RestCollection->Modify();
	}
}

FGeometryCollectionEdit::FGeometryCollectionEdit(UGeometryCollectionComponent * InComponent, bool InUpdate /*= true*/)
	: Component(InComponent)
	, bUpdate(InUpdate)
{
	bHadPhysicsState = Component->HasValidPhysicsState();
	if(bUpdate && bHadPhysicsState)
	{
		Component->DestroyPhysicsState();
	}
}

FGeometryCollectionEdit::~FGeometryCollectionEdit()
{
	if (bUpdate)
	{
		Component->ResetDynamicCollection();
		if (GetRestCollection())
		{
			GetRestCollection()->Modify();
		}

		if(bHadPhysicsState)
		{
			Component->RecreatePhysicsState();
		}
	}
}

UGeometryCollection* FGeometryCollectionEdit::GetRestCollection()
{
	if (Component)
	{
		return Component->RestCollection;
	}
	return nullptr;
}


TArray<FLinearColor> FScopedColorEdit::RandomColors;

FScopedColorEdit::FScopedColorEdit(UGeometryCollectionComponent* InComponent) : Component(InComponent)
{
	if (RandomColors.Num() == 0)
	{
		for (int i = 0; i < 100; i++)
		{
			const FColor Color(FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, 255);
			RandomColors.Push(FLinearColor(Color));
		}
	}
}

FScopedColorEdit::~FScopedColorEdit()
{
	UpdateBoneColors();
}
void FScopedColorEdit::SetShowBoneColors(bool ShowBoneColorsIn)
{
	Component->ShowBoneColors = ShowBoneColorsIn;
}

bool FScopedColorEdit::GetShowBoneColors() const
{
	return Component->ShowBoneColors;
}

void FScopedColorEdit::SetShowSelectedBones(bool ShowSelectedBonesIn)
{
	Component->ShowSelectedBones = ShowSelectedBonesIn;
}

bool FScopedColorEdit::GetShowSelectedBones() const
{
	return Component->ShowSelectedBones;
}

bool FScopedColorEdit::IsBoneSelected(int BoneIndex) const
{
	return Component->SelectedBones.Contains(BoneIndex);
}

void FScopedColorEdit::SetSelectedBones(const TArray<int32>& SelectedBonesIn)
{
	Component->SelectedBones = SelectedBonesIn;
}

void FScopedColorEdit::AppendSelectedBones(const TArray<int32>& SelectedBonesIn)
{
	Component->SelectedBones.Append(SelectedBonesIn);
}

void FScopedColorEdit::AddSelectedBone(int32 BoneIndex)
{
	Component->SelectedBones.Push(BoneIndex);
}

void FScopedColorEdit::ClearSelectedBone(int32 BoneIndex)
{
	Component->SelectedBones.Remove(BoneIndex);
}

const TArray<int32>& FScopedColorEdit::GetSelectedBones() const
{
	return Component->GetSelectedBones();
}

void FScopedColorEdit::ResetBoneSelection()
{
	Component->SelectedBones.Empty();
}

void FScopedColorEdit::SelectBones(GeometryCollection::ESelectionMode SelectionMode)
{
	check(Component);
	const UGeometryCollection* GeometryCollection = Component->GetRestCollection();
	if (GeometryCollection)
	{
		TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();

		switch (SelectionMode)
		{
		case GeometryCollection::ESelectionMode::None:
			ResetBoneSelection();
			break;

		case GeometryCollection::ESelectionMode::AllGeometry:
		{
			TArray<int32> Roots;
			FGeometryCollectionClusteringUtility::GetRootBones(GeometryCollectionPtr.Get(), Roots);
			ResetBoneSelection();
			for (int32 RootElement : Roots)
			{
				TArray<int32> LeafBones;
				FGeometryCollectionClusteringUtility::GetLeafBones(GeometryCollectionPtr.Get(), RootElement, LeafBones);
				AppendSelectedBones(LeafBones);
			}

		}
		break;

		case GeometryCollection::ESelectionMode::InverseGeometry:
		{
			TArray<int32> Roots;
			FGeometryCollectionClusteringUtility::GetRootBones(GeometryCollectionPtr.Get(), Roots);
			TArray<int32> NewSelection;
			for (int32 RootElement : Roots)
			{
				TArray<int32> LeafBones;
				FGeometryCollectionClusteringUtility::GetLeafBones(GeometryCollectionPtr.Get(), RootElement, LeafBones);

				for (int32 Element : LeafBones)
				{
					if (!IsBoneSelected(Element))
					{
						NewSelection.Push(Element);
					}
				}
			}
			ResetBoneSelection();
			AppendSelectedBones(NewSelection);
		}
		break;

		default: 
			check(false); // unexpected selection mode
		break;
		}

		const TArray<int32> SelectedBones = GetSelectedBones();
		SetHighlightedBones(SelectedBones);
	}
}

bool FScopedColorEdit::IsBoneHighlighted(int BoneIndex) const
{
	return Component->HighlightedBones.Contains(BoneIndex);
}

void FScopedColorEdit::SetHighlightedBones(const TArray<int32>& HighlightedBonesIn)
{
	Component->HighlightedBones = HighlightedBonesIn;
}

void FScopedColorEdit::AddHighlightedBone(int32 BoneIndex)
{
	Component->HighlightedBones.Push(BoneIndex);
}

const TArray<int32>& FScopedColorEdit::GetHighlightedBones() const
{
	return Component->GetHighlightedBones();
}

void FScopedColorEdit::ResetHighlightedBones()
{
	Component->HighlightedBones.Empty();
}

void FScopedColorEdit::SetLevelViewMode(int ViewLevelIn)
{
	Component->ViewLevel = ViewLevelIn;
}

int FScopedColorEdit::GetViewLevel()
{
	return Component->ViewLevel;
}

void FScopedColorEdit::UpdateBoneColors()
{
	FGeometryCollectionEdit GeometryCollectionEdit = Component->EditRestCollection();
	UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();
	FGeometryCollection* Collection = GeometryCollection->GetGeometryCollection().Get();

	FLinearColor BlankColor(FColor(80, 80, 80, 50));

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchy = *Collection->BoneHierarchy;
	TManagedArray<FLinearColor>& BoneColors = *Collection->BoneColor;

	for (int BoneIndex = 0; BoneIndex < BoneHierarchy.Num(); BoneIndex++)
	{
		FLinearColor BoneColor = FLinearColor(FColor::Black);
		if (IsBoneHighlighted(BoneIndex))
		{
			BoneColor = FLinearColor(FColor::White);
		}
		else
		{
			if (Component->ViewLevel == -1)
			{
				BoneColor = RandomColors[BoneIndex % RandomColors.Num()];
			}
			else
			{
				if (BoneHierarchy[BoneIndex].Level >= Component->ViewLevel)
				{
					// go up until we find parent at the required ViewLevel
					int32 Bone = BoneIndex;
					while (Bone != -1 && BoneHierarchy[Bone].Level > Component->ViewLevel)
					{
						Bone = BoneHierarchy[Bone].Parent;
					}

					int32 ColorIndex = Bone + 1; // parent can be -1 for root, range [-1..n]
					BoneColor = RandomColors[ColorIndex % RandomColors.Num()];
				}
				else
				{
					BoneColor = BlankColor;
				}
			}
		}

		BoneColors[BoneIndex] = BoneColor;
	}

	Component->MarkRenderStateDirty();
	Component->MarkRenderDynamicDataDirty();
}

void UGeometryCollectionComponent::InitializeMaterials(const TArray<UMaterialInterface*> &Materials, int32 InteriorMaterialIndex, int32 BoneSelectedMaterialIndex)
{	
	// We assume that we are resetting all material slots on this component to belong to this array
	int CurrIndex = 0;
	for (UMaterialInterface *CurrMaterial : Materials)
	{
		SetMaterial(CurrIndex++, CurrMaterial);
	}

	InteriorMaterialID = InteriorMaterialIndex;
	BoneSelectedMaterialID = BoneSelectedMaterialIndex;
}

#if INCLUDE_CHAOS
const TSharedPtr<FPhysScene_Chaos> UGeometryCollectionComponent::GetPhysicsScene() const
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
