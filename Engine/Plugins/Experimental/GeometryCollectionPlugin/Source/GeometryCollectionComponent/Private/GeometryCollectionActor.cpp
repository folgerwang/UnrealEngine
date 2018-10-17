// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GeometryCollectionActor.cpp: AGeometryCollectionActor methods.
=============================================================================*/

#include "GeometryCollectionActor.h"

#include "GeometryCollectionAlgo.h"
#include "GeometryCollectionComponent.h"
#include "GeometryCollectionUtility.h"
#include "GeometryCollectionBoneNode.h"
#include "Engine/SkeletalMesh.h"
#include "Math/Box.h"

#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"
#include "Apeiron/Utilities.h"
#include "Apeiron/Plane.h"
#include "Apeiron/Box.h"
#include "Apeiron/Sphere.h"

DEFINE_LOG_CATEGORY_STATIC(AGeometryCollectionActorLogging, Log, All);

bool AGeometryCollectionActor::InitializedState = false;
int8 AGeometryCollectionActor::Invalid = -1;

FTransform TransformMatrix(const FTransform& A, const FTransform& B) { return B * A; }

AGeometryCollectionActor::AGeometryCollectionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DamageThreshold(250.0)
	, Friction(0.5)
	, Bouncyness(0.1)
	, RigidBodyIdArray(new TManagedArray<int32>())
	, CenterOfMassArray(new TManagedArray<FVector>())
{
	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollectionComponent0"));
	RootComponent = GeometryCollectionComponent;
	AGeometryCollectionActor::InitializedState = false;

	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
}

void AGeometryCollectionActor::Tick(float DeltaTime) 
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::Tick()"));
	InitializeSimulation();
}

void AGeometryCollectionActor::InitializeSimulation()
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::InitializeSimulation()"));

	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (Collection && !AGeometryCollectionActor::InitializedState)
	{	
		Collection->AddAttribute<int32>("RigidBodyID", UGeometryCollection::TransformGroup, RigidBodyIdArray);
		Collection->AddAttribute<FVector>("CenterOfMass", UGeometryCollection::TransformGroup, CenterOfMassArray);

		Scene.SetKinematicUpdateFunction([this](FParticleType&, const float, const float, const int32) {
		});

		Scene.SetStartFrameFunction([this](const float StartFrame) {
			StartFrameCallback(StartFrame);
		});

		Scene.SetEndFrameFunction([this](const float EndFrame) {
			EndFrameCallback(EndFrame);
		});

		Scene.SetCreateBodiesFunction([this](FParticleType& Particles) {
			CreateRigidBodyCallback(Particles);
		});

		Scene.SetParameterUpdateFunction([this](FParticleType&, const float, const int32) {
		});

		Scene.SetDisableCollisionsUpdateFunction([this](TSet<TTuple<int32, int32>>&) {
		});

		Scene.AddPBDConstraintFunction([this](FParticleType&, const float) {
		});

#if INCLUDE_APEIRON
		Scene.AddForceFunction(Apeiron::Utilities::GetRigidsGravityFunction(Apeiron::TVector<float, 3>(0.f, 0.f, -1.f), 980.f));
#else
		Scene.AddForceFunction([this](FParticleType& Particles, const float, const int32 Index) {
			Particles[Index]->AddForce(FVector(0, 0, -980.f));
		});
#endif

		AGeometryCollectionActor::InitializedState = true;

#if !INCLUDE_APEIRON
		Scene.Init();
#endif
	}

	int32 NumTimeSteps = 1.f;
	float dt = GWorld->DeltaTimeSeconds / (float)NumTimeSteps;
	for (int i = 0; i < NumTimeSteps; i++)
	{
		Scene.Tick(dt);
	}
}

#if INCLUDE_APEIRON
void AGeometryCollectionActor::StartFrameCallback(float EndFrame)
{
	Scene.SetFriction(Friction);
	Scene.SetRestitution(Bouncyness);
}

void AGeometryCollectionActor::ResetAttributes()
{
	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
	{
		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;

		ParallelFor(Collection->NumElements(UGeometryCollection::TransformGroup), [&](int32 Index)
		{
			RigidBodyId[Index] = Invalid;
		});
	}
}

void AGeometryCollectionActor::CreateRigidBodyCallback(FParticleType& Particles)
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::AddRigidBodiesCallback()"));
	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (!Particles.Size() && Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
	{
		Particles.AddArray(&ExternalID);
		ResetAttributes();

		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;
		TManagedArray<FVector> & CenterOfMass = *CenterOfMassArray;
		
		TManagedArray<int32> & BoneMap = *Collection->BoneMap;
		TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchy = *Collection->BoneHierarchy;
		TManagedArray<FVector> & Vertex = *Collection->Vertex;

		TArray<FTransform> Transform;
		GeometryCollectionAlgo::GlobalMatrices(Collection, Transform);
		check(Collection->Transform->Num() == Transform.Num());

		// hard-coded floor (@todo on solver instead)
		Particles.AddParticles(1);
		ExternalID[0] = Invalid;
		Particles.X(0) = Apeiron::TVector<float, 3>(0.f, 0.f, 0.f);
		Particles.V(0) = Apeiron::TVector<float, 3>(0.f, 0.f, 0.f);
		Particles.R(0) = Apeiron::TRotation<float, 3>::MakeFromEuler(Apeiron::TVector<float, 3>(0.f, 0.f, 0.f));
		Particles.W(0) = Apeiron::TVector<float, 3>(0.f, 0.f, 0.f);
		Particles.M(0) = 1.f;
		Particles.InvM(0) = 0.f;
		Particles.I(0) = Apeiron::PMatrix<float, 3, 3>(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);
		Particles.InvI(0) = Apeiron::PMatrix<float, 3, 3>(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		Particles.Geometry(0) = new Apeiron::TPlane<float, 3>(Apeiron::TVector<float, 3>(0.f, 0.f, 0.f), Apeiron::TVector<float, 3>(0.f, 0.f, 1.f));

		// calculate bounds and center of mass ( @todo MassProperties)
		FVector Scale = GeometryCollectionComponent->GetComponentTransform().GetScale3D();
		ensure(Scale.X==1.0&&Scale.Y==1.0&&Scale.Z==1.0);

		TArray<FBox> Bounds;
		Bounds.AddZeroed(Collection->NumElements(UGeometryCollection::TransformGroup));

		TArray<int32> SurfaceParticlesCount;
		SurfaceParticlesCount.AddZeroed(Collection->NumElements(UGeometryCollection::TransformGroup));

		TArray<FVector> SumOfMass;
		SumOfMass.AddZeroed(Collection->NumElements(UGeometryCollection::TransformGroup));

		for (int i = 0; i < Vertex.Num(); i++)
		{
			int32 ParticleIndex = BoneMap[i];
			Bounds[ParticleIndex] += Vertex[i];
			SurfaceParticlesCount[ParticleIndex]++;
			SumOfMass[ParticleIndex] += Vertex[i];
		}

		// Determine which collection particles to simulate
		TArray< TPair<int32, int32> > SimulatableParticles;
		for (int i = 0; i < Collection->Transform->Num(); i++)
		{
			if (!BoneHierarchy[i].Children.Num())
			{
				if (SurfaceParticlesCount[i] && 0.f < Bounds[i].GetSize().SizeSquared())
				{
					SimulatableParticles.Add(TPair<int32, int32>(i, -1));
				}
			}
		}

		// Add entries into simulation array
		int NumRigids = Particles.Size();
		Particles.AddParticles(SimulatableParticles.Num());
		ParallelFor(SimulatableParticles.Num(), [&](int32 Index)
		{
			SimulatableParticles[Index].Value = NumRigids + Index;
		});


		// Add the rigids
		ParallelFor(SimulatableParticles.Num(), [&](int32 Index)
		{
			int32 i = SimulatableParticles[Index].Key;
			int32 RigidBodyIndex = SimulatableParticles[Index].Value;

			ExternalID[RigidBodyIndex] = i;
			RigidBodyId[i] = RigidBodyIndex;

			CenterOfMass[i] = SumOfMass[i] / SurfaceParticlesCount[i];
			Bounds[i] = Bounds[i].InverseTransformBy(FTransform(CenterOfMass[i]));

			FTransform WorldTransform = Transform[i]*GeometryCollectionComponent->GetComponentTransform();
			Particles.X(RigidBodyIndex) = WorldTransform.TransformPosition(CenterOfMass[i]);
			Particles.V(RigidBodyIndex) = Apeiron::TVector<float, 3>(0.f, 0.f, 0.f);
			Particles.R(RigidBodyIndex) = WorldTransform.GetRotation();
			Particles.W(RigidBodyIndex) = Apeiron::TVector<float, 3>(0.f, 0.f, 0.f);

			Particles.M(RigidBodyIndex) = 1.f;
			Particles.InvM(RigidBodyIndex) = 1.f;

			float SideSquared = Bounds[i].GetSize()[0] * Bounds[i].GetSize()[0] / 6.f;
			float InvSideSquared = 1.f / SideSquared;
			Particles.I(RigidBodyIndex) = Apeiron::PMatrix<float, 3, 3>(SideSquared, 0.f, 0.f, 0.f, SideSquared, 0.f, 0.f, 0.f, SideSquared);
			Particles.InvI(RigidBodyIndex) = Apeiron::PMatrix<float, 3, 3>(InvSideSquared, 0.f, 0.f, 0.f, InvSideSquared, 0.f, 0.f, 0.f, InvSideSquared);
			//Particles.I(RigidBodyIndex) = Apeiron::Matrix<float, 3, 3>(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);
			//Particles.InvI(RigidBodyIndex) = Apeiron::Matrix<float, 3, 3>(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

			Particles.Geometry(RigidBodyIndex) = new Apeiron::TBox<float, 3>(Bounds[i].Min, Bounds[i].Max);
			//Particles.Geometry(RigidBodyIndex) = new Apeiron::Sphere<float, 3>(FVector(0, 0, 0), Bounds[i].GetExtent().GetMin());

			if (false)
			{
				Particles.CollisionParticles(RigidBodyIndex).AddParticles(SurfaceParticlesCount[i]);
				for (int VertexIndex = 0, CollisionIndex = 0; VertexIndex < Collection->NumElements(UGeometryCollection::VerticesGroup); VertexIndex++)
				{
					if (BoneMap[VertexIndex] == i)
					{
						Particles.CollisionParticles(RigidBodyIndex).X(CollisionIndex++) = Vertex[VertexIndex];
					}
				}
			}
		});

		// build clusters.
		for (int i = 0; i < Collection->Transform->Num(); i++)
		{
			if (BoneHierarchy[i].Parent == FGeometryCollectionBoneNode::InvalidBone
				&& BoneHierarchy[i].Children.Num() > 0)
			{
				InitializeClustering(i, Particles);
			}
		}

		Scene.InitializeFromParticleData();
	}
}

void AGeometryCollectionActor::BuildClusters(const TMap<uint32, TArray<uint32> > & ClusterMap)
{
	if (ClusterMap.Num())
	{
		UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
		if (Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
		{
			TManagedArray<int32> & RigidBodyID = *RigidBodyIdArray;
			TManagedArray<FGeometryCollectionBoneNode> & Bone = *Collection->BoneHierarchy;

			// create clusters
			for (const auto& Entry : ClusterMap)
			{
				const auto ClusterIndex = Entry.Key;
				TArray<uint32> Bodies = Entry.Value;
				int NewSolverClusterID = Scene.CreateClusterParticle(Bodies);

				// two-way mapping
				RigidBodyID[ClusterIndex] = NewSolverClusterID;
				ExternalID[NewSolverClusterID] = ClusterIndex;

				Scene.SetClusterStrain(NewSolverClusterID, DamageThreshold);
			}

			Scene.InitializeFromParticleData();

		}
	}
}


void AGeometryCollectionActor::InitializeClustering(uint32 ParentIndex, FParticleType& Particles)
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::InitializeClustering()"));

	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
	{
		TManagedArray<int32> & RigidBodyID = *RigidBodyIdArray;
		TManagedArray<FGeometryCollectionBoneNode> & Bone = *Collection->BoneHierarchy;

		// gather cluster arrays based on root transforms
		TMap<uint32, TArray<uint32> > ClusterMap;
		TArray<uint32> ChildSet;
		for (const auto ChildIndex : Bone[ParentIndex].Children)
		{
			if (Bone[ChildIndex].Children.Num())
			{
				InitializeClustering(ChildIndex, Particles);
			}
			ChildSet.Add(RigidBodyID[ChildIndex]);
		}
		if (ChildSet.Num())
		{
			ClusterMap.Add(ParentIndex, ChildSet);
		}

		BuildClusters(ClusterMap);
	}
}


void AGeometryCollectionActor::EndFrameCallback(float EndFrame)
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::EndFrameFunction()"));
	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
	{
		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;
		TManagedArray<FVector> & CenterOfMass = *CenterOfMassArray;
		TManagedArray<FTransform> & Transform = *Collection->Transform;
		TManagedArray<FGeometryCollectionBoneNode> & Hierarchy = *Collection->BoneHierarchy;

		const Apeiron::TPBDRigidParticles<float, 3>& Particles = Scene.GetRigidParticles();

		FTransform InverseComponentTransform = GeometryCollectionComponent->GetComponentTransform().Inverse();
		ParallelFor(Collection->NumElements(UGeometryCollection::TransformGroup), [&](int32 i)
		{
			if (!Hierarchy[i].Children.Num())
			{
				Transform[i].SetTranslation(InverseComponentTransform.TransformPosition(Particles.X(RigidBodyId[i])));
				Transform[i].SetRotation(InverseComponentTransform.TransformRotation(Particles.R(RigidBodyId[i])));
			}
			else 
			{
				Transform[i].SetTranslation(FVector::ZeroVector);
				Transform[i].SetRotation(FQuat::Identity);
			}
		});

		GeometryCollectionComponent->SetRenderStateDirty();
	}
}
#else
void AGeometryCollectionActor::StartFrameCallback(float EndFrame)
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::StartFrameCallback()"));
	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (!Scene.GetSimulation()->NumActors() && Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
	{
		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;
		TManagedArray<FVector> & CenterOfMass = *CenterOfMassArray;
		
		TManagedArray<FTransform> & Transform = *Collection->Transform;
		TManagedArray<int32> & BoneMap = *Collection->BoneMap;
		TManagedArray<FVector> & Vertex = *Collection->Vertex;

		PxMaterial* NewMaterial = GPhysXSDK->createMaterial(0, 0, 0);

		// floor
		FTransform FloorTransform;
		PxRigidStatic* FloorActor = GPhysXSDK->createRigidStatic(PxTransform());
		PxShape* FloorShape = PxRigidActorExt::createExclusiveShape(*FloorActor, PxBoxGeometry(U2PVector(FVector(10000.f, 10000.f, 10.f))), *NewMaterial);
		// This breaks threading correctness in a general sense but is needed until we can call this in create rigid bodies
		const_cast<ImmediatePhysics::FSimulation*>(Scene.GetSimulation())->CreateStaticActor(FloorActor, FloorTransform);

		FVector Scale = GeometryCollectionComponent->GetComponentTransform().GetScale3D();

		TArray<FBox> Bounds;
		Bounds.AddZeroed(Collection->NumElements(UGeometryCollection::TransformGroup));

		TArray<int32> SurfaceParticlesCount;
		SurfaceParticlesCount.AddZeroed(Collection->NumElements(UGeometryCollection::TransformGroup));

		TArray<FVector> SumOfMass;
		SumOfMass.AddZeroed(Collection->NumElements(UGeometryCollection::TransformGroup));

		for (int i = 0; i < Vertex.Num(); i++)
		{
			FVector ScaledVertex = Scale*Vertex[i];
			int32 ParticleIndex = BoneMap[i];
			Bounds[ParticleIndex] += ScaledVertex;
			SurfaceParticlesCount[ParticleIndex]++;
			SumOfMass[ParticleIndex] += ScaledVertex;
		}


		for (int32 i = 0; i < Collection->Transform->Num(); ++i)
		{
			if (SurfaceParticlesCount[i] && 0.f<Bounds[i].GetSize().SizeSquared())
			{
				CenterOfMass[i] = SumOfMass[i] / SurfaceParticlesCount[i];
				Bounds[i] = Bounds[i].InverseTransformBy(FTransform(CenterOfMass[i]));

				RigidBodyId[i] = i;
				int32 RigidBodyIndex = RigidBodyId[i];

				FTransform NewTransform = TransformMatrix(GeometryCollectionComponent->GetComponentTransform(), Transform[i]);
				float SideSquared = Bounds[i].GetSize()[0] * Bounds[i].GetSize()[0] / 6.f;

				PxRigidDynamic* NewActor = GPhysXSDK->createRigidDynamic(PxTransform());
				NewActor->setLinearVelocity(U2PVector(FVector(0.f, 0.f, 0.f)));
				NewActor->setAngularVelocity(U2PVector(FVector(0.f, 0.f, 0.f)));
				NewActor->setMass(1.f);
				NewActor->setMassSpaceInertiaTensor(U2PVector(FVector(SideSquared, SideSquared, SideSquared)));
				PxShape* NewShape = PxRigidActorExt::createExclusiveShape(*NewActor, PxBoxGeometry(U2PVector((Bounds[i].Max - Bounds[i].Min) / 2.f)), *NewMaterial);
				const_cast<ImmediatePhysics::FSimulation*>(Scene.GetSimulation())->CreateDynamicActor(NewActor, NewTransform);
			}
		}
	}
}

void AGeometryCollectionActor::CreateRigidBodyCallback(FParticleType& Particles)
{
}

void AGeometryCollectionActor::EndFrameCallback(float EndFrame)
{
	UE_LOG(AGeometryCollectionActorLogging, Log, TEXT("AGeometryCollectionActor::EndFrameFunction()"));
	UGeometryCollection* Collection = GeometryCollectionComponent->GetDynamicCollection();
	if (Collection->HasAttribute("RigidBodyID", UGeometryCollection::TransformGroup))
	{
		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;
		TManagedArray<FVector> & CenterOfMass = *CenterOfMassArray;
		TManagedArray<FTransform> & Transform = *Collection->Transform;

		const TArray<ImmediatePhysics::FActorHandle*>& Actors = Scene.GetSimulation()->GetActorHandles();

		FTransform InverseComponentTransform = GeometryCollectionComponent->GetComponentTransform().Inverse();
		for (int i = 0; i < Collection->NumElements(UGeometryCollection::TransformGroup); i++)
		{
			int32 RigidBodyIndex = RigidBodyId[i];
			Transform[i] = TransformMatrix(InverseComponentTransform, Actors[RigidBodyIndex]->GetWorldTransform());
		}

		GeometryCollectionComponent->SetRenderStateDirty();
	}
}
#endif
