// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"

#include "CoreMinimal.h"
#include "Misc/CoreMiscDefines.h"

#if INCLUDE_CHAOS
#include "Async/ParallelFor.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionPhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Chaos/MassProperties.h"
#include "ChaosStats.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/PBDCollisionTypes.h"
#include "PBDRigidsSolver.h"

DEFINE_LOG_CATEGORY_STATIC(GeometryCollectionSolverCallbacksLogging, NoLogging, All);

int8 FGeometryCollectionSolverCallbacks::Invalid = -1;

using namespace Chaos;


FGeometryCollectionSolverCallbacks::FGeometryCollectionSolverCallbacks()
	: FSolverCallbacks()
	, InitializedState(false)
	, LocalToMassArray(new TManagedArray<FTransform>())
	, CollisionMaskArray(new TManagedArray<int32>())
	, CollisionStructureIDArray(new TManagedArray<int32>())
	, DynamicStateArray(new TManagedArray<int32>())
	, RigidBodyIdArray(new TManagedArray<int32>())
	, SolverClusterIDArray(new TManagedArray<int32>())
	, SimulatableParticlesArray(new TManagedArray<bool>())
	, VolumeArray(new TManagedArray<float>())
	, StayDynamicFieldIndex(Invalid)
	, BaseParticleIndex(INDEX_NONE)
	, NumParticles(0)
	, ProxySimDuration(0.0f)
{
}

void FGeometryCollectionSolverCallbacks::Initialize()
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::InitializeSimulationData()"));

	check(Parameters.DynamicCollection);

	if (Parameters.bClearCache)
	{
		if (ResetAnimationCacheCallback)
		{
			ResetAnimationCacheCallback();
		}
	}

	if (Parameters.FieldSystem != nullptr)
	{
		StayDynamicFieldIndex = Parameters.FieldSystem->TerminalIndex("StayDynamic");
	}

	CreateDynamicAttributes();

	IdentifySimulatableElements();

	InitializeCollisionStructures();

	ProxySimDuration = 0.0f;

	InitializedState = false;
}

void FGeometryCollectionSolverCallbacks::CreateDynamicAttributes()
{
	FGeometryCollection * DynamicCollection = Parameters.DynamicCollection;

	GeometryCollection::AddGeometryProperties(DynamicCollection);
	DynamicCollection->AddAttribute<int32>("RigidBodyID", FTransformCollection::TransformGroup, RigidBodyIdArray);
	DynamicCollection->AddAttribute<int32>("SolverClusterID", FTransformCollection::TransformGroup, SolverClusterIDArray);
	DynamicCollection->AddAttribute<FVector>("LocalToMass", FTransformCollection::TransformGroup, LocalToMassArray);
	DynamicCollection->AddAttribute<int32>("DynamicState", FTransformCollection::TransformGroup, DynamicStateArray);
	DynamicCollection->AddAttribute<bool>("SimulatableParticles", FTransformCollection::TransformGroup, SimulatableParticlesArray);
	DynamicCollection->AddAttribute<int32>("CollisionStructureID", FTransformCollection::TransformGroup, CollisionStructureIDArray);
	DynamicCollection->AddAttribute<int32>("CollisionMask", FGeometryCollection::VerticesGroup, CollisionMaskArray);
	DynamicCollection->AddAttribute<float>("Volume", FGeometryCollection::GeometryGroup, VolumeArray);

	TManagedArray<int32>& RigidBodyId = *RigidBodyIdArray;
	TManagedArray<int32>& SolverClusterID = *SolverClusterIDArray;
	TManagedArray<int32>& DynamicState = *DynamicStateArray;
	TManagedArray<bool>& SimulatableParticles = *SimulatableParticlesArray;
	TManagedArray<int32>& CollisionMask = *CollisionMaskArray;
	TManagedArray<FTransform>& LocalOffset = *LocalToMassArray;


	for (int32 Index = 0; Index < RigidBodyId.Num(); Index++)
	{
		RigidBodyId[Index] = Invalid;
		SolverClusterID[Index] = Invalid;
		DynamicState[Index] = (int32)Parameters.ObjectType;
		SimulatableParticles[Index] = false;
		LocalOffset[Index] = FTransform(FQuat::Identity, FVector(0) );
		LocalOffset[Index].NormalizeRotation();
	}

	for (int Index = 0; Index < CollisionMask.Num(); Index++)
	{
		CollisionMask[Index] = 1;
	}
}



void FGeometryCollectionSolverCallbacks::IdentifySimulatableElements()
{
	FGeometryCollection * DynamicCollection = Parameters.DynamicCollection;

	FVector Scale = Parameters.WorldTransform.GetScale3D();
	ensure(Scale.X == 1.0&&Scale.Y == 1.0&&Scale.Z == 1.0);

	// Determine which collection particles to simulate
	TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchy = *DynamicCollection->BoneHierarchy;
	TManagedArray<FBox>& BoundingBox = *DynamicCollection->BoundingBox;
	TManagedArray<int32>& VertexCount = *DynamicCollection->VertexCount;
	TManagedArray<bool>& SimulatableParticles = *SimulatableParticlesArray;
	TManagedArray<int32>& TransformIndex = *DynamicCollection->TransformIndex;
	int32 NumTransforms = BoneHierarchy.Num();

	const int32 NumTransformMappings = TransformIndex.Num();

	// @todo(better) : This should be flagged elsewhere, during prep for simulation instead.
	// Do not simulate hidden geometry
	TArray<bool> HiddenObject;
	HiddenObject.Init(true, NumTransforms);
	TManagedArray<bool>& Visible = *DynamicCollection->Visible;
	TManagedArray<int32>& BoneMap = *DynamicCollection->BoneMap;

	TManagedArray<FIntVector>& Indices = *DynamicCollection->Indices;
	int32 PrevObject = -1;
	for (int32 i = 0; i < Indices.Num(); i++)
	{
		if (Visible[i])
		{
			int32 ObjIdx = BoneMap[Indices[i][0]];
			HiddenObject[ObjIdx] = false;
 			ensureMsgf(ObjIdx >= PrevObject, TEXT("Objects are not contiguous. This breaks assumptions later in the pipeline"));
			PrevObject = ObjIdx;
		}
	}

	for (int i = 0; i < NumTransformMappings; i++)
	{
		int32 Tdx = TransformIndex[i];
		checkSlow(0<=Tdx&& Tdx<NumTransforms);
		if (BoneHierarchy[Tdx].IsGeometry() && VertexCount[i] 
			&& 0.f < BoundingBox[i].GetSize().SizeSquared() && !HiddenObject[Tdx])
		{
			SimulatableParticles[Tdx] = true;
		}
	}

}

TTriangleMesh<float>* CreateTriangleMesh(int32 FaceCount, int32 VertexOffset, int32 StartIndex, const TManagedArray<FVector>& Vertex, const TManagedArray<bool>& Visible, const TManagedArray<FIntVector>& Indices, TSet<int32>& VertsAdded)
{
	TArray<Chaos::TVector<int32, 3>> Faces;
	{
		Faces.Reserve(FaceCount);
		for (int j = 0; j < FaceCount; j++)
		{
			if (!Visible[j + StartIndex])
			{
				continue;
			}

			// @todo: This should never happen but seems to so we need to make sure these faces are not counted
			if (Indices[j + StartIndex].X == Indices[j + StartIndex].Y || Indices[j + StartIndex].Z == Indices[j + StartIndex].Y || Indices[j + StartIndex].X == Indices[j + StartIndex].Z)
			{
				continue;
			}

			//make sure triangle is not degenerate (above only checks indices, we need to check for co-linear etc...)
			const TVector<float, 3>& X = Vertex[Indices[j + StartIndex].X];
			const TVector<float, 3>& Y = Vertex[Indices[j + StartIndex].Y];
			const TVector<float, 3>& Z = Vertex[Indices[j + StartIndex].Z];
			const TVector<float, 3> Cross = TVector<float, 3>::CrossProduct(Z - X, Y - X);
			if (Cross.SizeSquared() >= 1e-4)
			{
				Faces.Add(Chaos::TVector<int32, 3>(Indices[j + StartIndex].X, Indices[j + StartIndex].Y, Indices[j + StartIndex].Z));
				for (int Axis = 0; Axis < 3; ++Axis)
				{
					VertsAdded.Add(Indices[j + StartIndex][Axis]);
				}
			}
		}
	}

	return new TTriangleMesh<float>(MoveTemp(Faces));
}


void FGeometryCollectionSolverCallbacks::InitializeCollisionStructures()
{
	FGeometryCollection * DynamicCollection = Parameters.DynamicCollection;
	const TManagedArray<bool>& SimulatableParticles = *SimulatableParticlesArray;
	const TManagedArray<bool>& Visible = *DynamicCollection->Visible;

	// TransformGroup
	TManagedArray<int32> & BoneMap = *DynamicCollection->BoneMap;
	TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchy = *DynamicCollection->BoneHierarchy;
	TManagedArray<FTransform>& LocalToMass = *LocalToMassArray;
	// VerticesGroup
	const TManagedArray<FVector>& Vertex = *DynamicCollection->Vertex;
	TManagedArray<int32>& CollisionMask = *CollisionMaskArray;
	// GeometryGroup
	const TManagedArray<FBox>& BoundingBox = *DynamicCollection->BoundingBox;	//todo(ocohen): this whole thing should use a function to avoid accidentally writing to rest collection
	const TManagedArray<float>& InnerRadius = *DynamicCollection->InnerRadius;
	const TManagedArray<int32>& VertexCount = *DynamicCollection->VertexCount;
	const TManagedArray<int32>& VertexStart = *DynamicCollection->VertexStart;
	const TManagedArray<int32>& FaceCount = *DynamicCollection->FaceCount;
	const TManagedArray<int32>& FaceStart = *DynamicCollection->FaceStart;
	TManagedArray<float>& Volume = *VolumeArray;
	const TManagedArray<int32>& CollisionStructureID = *CollisionStructureIDArray;
	const TManagedArray<int32>& TransformIndex = *DynamicCollection->TransformIndex;
	const TManagedArray<FIntVector>& Indices = *DynamicCollection->Indices;

	TArray<FTransform> Transform;
	GeometryCollectionAlgo::GlobalMatrices(DynamicCollection, Transform);
	check(DynamicCollection->Transform->Num() == Transform.Num());

	// @todo(ContiguousFaces) : Enable these and remove all code here that reconstructs faces and indices.
	// ensure(DynamicCollection->HasContiguousFaces());
	// ensure(DynamicCollection->HasContiguousVertices());

	// @todo: Need a better way to specify volume if we are going to use it
	float TotalVolume = 0.f;
	for (int32 GeometryIndex = 0; GeometryIndex < TransformIndex.Num(); GeometryIndex++)
	{
		Volume[GeometryIndex] = FCollisionStructureManager::CalculateVolume(BoundingBox[GeometryIndex], InnerRadius[GeometryIndex], Parameters.ImplicitType);
		TotalVolume += Volume[GeometryIndex];
	}
	ensureMsgf(TotalVolume != 0.f, TEXT("Volume check error."));
	
	TParticles<float, 3> AllParticles;
	AllParticles.AddParticles(Vertex.Num());	//todo(ocohen): avoid this copy
	for (int32 Idx = 0; Idx < Vertex.Num(); ++Idx)
	{
		AllParticles.X(Idx) = Vertex[Idx];
	}

	const int32 NumGeometries = DynamicCollection->NumElements(FGeometryCollection::GeometryGroup);

	for (int32 GeometryIndex = 0; GeometryIndex < NumGeometries; GeometryIndex++)
	{
		int32 TransformGroupIndex = TransformIndex[GeometryIndex];
		if (SimulatableParticles[TransformGroupIndex])
		{
			TSet<int32> VertsAdded;
			TTriangleMesh<float>* TriMesh = CreateTriangleMesh(FaceCount[GeometryIndex], VertexStart[GeometryIndex], FaceStart[GeometryIndex], Vertex, Visible, Indices, VertsAdded);
			TMassProperties<float, 3> MassProperties = CalculateMassProperties(AllParticles, *TriMesh, Parameters.Mass);
			if (MassProperties.Volume)
			{
				LocalToMass[TransformGroupIndex] = FTransform(MassProperties.RotationOfMass, MassProperties.CenterOfMass);
			}
			else
			{
				LocalToMass[TransformGroupIndex] = FTransform(Chaos::TRotation<float, 3>(FQuat(0, 0, 0, 1)), BoundingBox[GeometryIndex].GetCenter());
				FVector Size = BoundingBox[GeometryIndex].GetSize();
				FVector SideSquared(Size.X * Size.X, Size.Y * Size.Y, Size.Z * Size.Z);
				MassProperties.InertiaTensor = Chaos::PMatrix<float, 3, 3>((SideSquared.Y + SideSquared.Z) / 12.f, (SideSquared.X + SideSquared.Z) / 12.f, (SideSquared.X + SideSquared.Y) / 12.f);
				MassProperties.Volume = FCollisionStructureManager::CalculateVolume(BoundingBox[GeometryIndex], InnerRadius[GeometryIndex], EImplicitTypeEnum::Chaos_Implicit_Cube);
			}

			// Update vertex buffer to be in mass space so that at runtime geometry aligns properly.
			FBox InstanceBoundingBox(EForceInit::ForceInitToZero);
			for (int32 VertIdx = VertexStart[GeometryIndex]; VertIdx < VertexStart[GeometryIndex] + VertexCount[GeometryIndex]; ++VertIdx)
			{
				if (VertsAdded.Contains(VertIdx))	//only consider verts from the trimesh
				{
					AllParticles.X(VertIdx) = LocalToMass[TransformGroupIndex].InverseTransformPosition(AllParticles.X(VertIdx));
					InstanceBoundingBox += AllParticles.X(VertIdx);	//build bounding box for visible verts in mass space
				}
			}

			Chaos::TVector<float, 3> DiagonalInertia(MassProperties.InertiaTensor.M[0][0], MassProperties.InertiaTensor.M[1][1], MassProperties.InertiaTensor.M[2][2]);
			FCollisionStructureManager::FElement Element = {
				FCollisionStructureManager::NewSimplicial(AllParticles, BoneMap, CollisionMask, Parameters.CollisionType, *TriMesh, Parameters.CollisionParticlesFraction),
				FCollisionStructureManager::NewImplicit(AllParticles, *TriMesh, InstanceBoundingBox, InnerRadius[GeometryIndex], Parameters.MinLevelSetResolution, Parameters.MaxLevelSetResolution ,Parameters.CollisionType, Parameters.ImplicitType),
				DiagonalInertia,
				TriMesh,
				MassProperties.Volume,
				//todo: mass / volume is wrong, but for demo we already tuned it this way. See COM on clusters when fixing
				Parameters.MassAsDensity ? (Parameters.Mass / MassProperties.Volume) : (Parameters.Mass * MassProperties.Volume / TotalVolume)
			};

			CollisionStructures.Map.Add(TransformGroupIndex, Element);
		}
	}
}


bool FGeometryCollectionSolverCallbacks::IsSimulating() const
{
	return Parameters.Simulating;
}


void FGeometryCollectionSolverCallbacks::Reset()
{
	InitializedState = false;
}

void FGeometryCollectionSolverCallbacks::CreateRigidBodyCallback(FSolverCallbacks::FParticlesType& Particles)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::CreateRigidBodyCallback()"));

	FGeometryCollection * DynamicCollection = Parameters.DynamicCollection;
	check(DynamicCollection);

	if (!InitializedState)
	{
		InitializedState = true;
		Chaos::PBDRigidsSolver* RigidSolver = GetSolver();
		check(RigidSolver);

		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;
		TManagedArray<FTransform> & LocalToMass = *LocalToMassArray;
		TManagedArray<int32>& VertexCount = *DynamicCollection->VertexCount;
		TManagedArray<FBox>& BoundingBox = *DynamicCollection->BoundingBox;
		TManagedArray<bool>& SimulatableParticles = *SimulatableParticlesArray;
		TManagedArray<int32>& TransformIndex = *DynamicCollection->TransformIndex;

		TManagedArray<int32> & BoneMap = *DynamicCollection->BoneMap;
		TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchy = *DynamicCollection->BoneHierarchy;
		TManagedArray<FVector> & Vertex = *DynamicCollection->Vertex;

		TArray<FTransform> Transform;
		GeometryCollectionAlgo::GlobalMatrices(DynamicCollection, Transform);
		check(DynamicCollection->Transform->Num() == Transform.Num());

		// count particles to add
		int NumSimulatedParticles = 0;
		for (int32 Index = 0; Index < SimulatableParticles.Num(); Index++)
		{
			if (SimulatableParticles[Index] == true)
			{
				NumSimulatedParticles++;
			}
		}

		// Add entries into simulation array
		int NumRigids = Particles.Size();
		BaseParticleIndex = NumRigids;
		NumParticles = NumSimulatedParticles;
		Particles.AddParticles(NumSimulatedParticles);
		for (int32 Index = 0, NextId=0; Index < SimulatableParticles.Num(); Index++)
		{
			if (SimulatableParticles[Index] == true)
			{
				RigidBodyId[Index] = NumRigids + NextId++;
			}
		}


		FVector InitialLinearVelocity(0.f), InitialAngularVelocity(0.f);
		if (Parameters.InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
		{
			InitialLinearVelocity = Parameters.InitialLinearVelocity;
			InitialAngularVelocity = Parameters.InitialAngularVelocity;
		}

		// Add the rigid bodies
		//for (int32 Index = 0; Index < TransformIndex.Num(); Index++)
		const int32 NumGeometries = DynamicCollection->NumElements(FGeometryCollection::GeometryGroup);
		ParallelFor(NumGeometries, [&](int32 GeometryIndex)
		{
			const int32 TransformGroupIndex = TransformIndex[GeometryIndex];
			if (SimulatableParticles[TransformGroupIndex] == true)
			{
				int32 RigidBodyIndex = RigidBodyId[TransformGroupIndex];

				FTransform WorldTransform = LocalToMass[TransformGroupIndex] * Transform[TransformGroupIndex] * Parameters.WorldTransform;

				Particles.X(RigidBodyIndex) = WorldTransform.GetTranslation();
				Particles.V(RigidBodyIndex) = Chaos::TVector<float, 3>(InitialLinearVelocity);
				Particles.R(RigidBodyIndex) = WorldTransform.GetRotation().GetNormalized();
				Particles.W(RigidBodyIndex) = Chaos::TVector<float, 3>(InitialAngularVelocity);
				Particles.P(RigidBodyIndex) = Particles.X(RigidBodyIndex);
				Particles.Q(RigidBodyIndex) = Particles.R(RigidBodyIndex);

				ensure(Parameters.MinimumMassClamp >= KINDA_SMALL_NUMBER);
				Particles.M(RigidBodyIndex) = FMath::Clamp(Parameters.Mass, Parameters.MinimumMassClamp, FLT_MAX);
				Particles.I(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);
				Particles.InvM(RigidBodyIndex) = 1.f / Particles.M(RigidBodyIndex);
				Particles.InvI(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

				if (const FCollisionStructureManager::FElement* Element = CollisionStructures.Map.Find(TransformGroupIndex))
				{
					Particles.M(RigidBodyIndex) = FMath::Clamp(Element->Mass, Parameters.MinimumMassClamp, FLT_MAX);
					Particles.InvM(RigidBodyIndex) = 1.f / Particles.M(RigidBodyIndex);
					Particles.I(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(Element->InertiaTensor[0], Element->InertiaTensor[1], Element->InertiaTensor[2]);
					Particles.InvI(RigidBodyIndex) = Particles.I(RigidBodyIndex).Inverse();
					//FVector Tensor = Element.UnitMassTensor*Particles.M(RigidBodyIndex);
					//Particles.I(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(Tensor.X, 0.f, 0.f, 0.f, Tensor.Y, 0.f, 0.f, 0.f, Tensor.Z);
					//Particles.InvI(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(1.f / Tensor.X, 0.f, 0.f, 0.f, 1.f / Tensor.Y, 0.f, 0.f, 0.f, 1.f / Tensor.Z);

					// @important (The solver can not free this memory)
					Particles.Geometry(RigidBodyIndex) = Element->Implicit;

					if(FCollisionStructureManager::FSimplicial* Simplicial = Element->Simplicial)
					{
						if (Simplicial->Num() == 0)
						{
							//UE_LOG(LogChaos, Warning, TEXT("Empty Simplicial for Index:%d"), Index);
							//TODO: figure out why this happens at all
							//For now make a single point simplicial so we don't fall through the world
							Simplicial->Add(Chaos::TVector<float, 3>(0));
						}
						// @todo(AnalyticSimulation) : Ability to simulate without any particles. 
						//      We should be able to avoid this for non-clusters, but at the lower level
						//      we assume that if one of the objects has level set particles, they both 
						//      do. Needed for cluster - non cluster collision

						// @important(brice) : Can we avoid this copy?
						Particles.CollisionParticlesInitIfNeeded(RigidBodyIndex);
						ensure(Particles.CollisionParticles(RigidBodyIndex)->Size() == 0);
						Particles.CollisionParticles(RigidBodyIndex)->AddParticles(Simplicial->Num());
						for (int VertexIndex = 0; VertexIndex < Simplicial->Num(); VertexIndex++)
						{
							Particles.CollisionParticles(RigidBodyIndex)->X(VertexIndex) = (*Simplicial)[VertexIndex];
						}
						if (Particles.CollisionParticles(RigidBodyIndex)->Size())
						{
							Particles.CollisionParticles(RigidBodyIndex)->UpdateAccelerationStructures();
						}
					}
				}

				//
				//  Manage Object State
				//

				// Only sleep if we're not replaying a simulation
				// #BG TODO If this becomes an issue, recorded tracks should track awake state as well as transforms
				if (Parameters.ObjectType == EObjectTypeEnum::Chaos_Object_Sleeping)
				{
					Particles.SetSleeping(RigidBodyIndex, true);
				}
			}

		});

		// #BG Temporary - don't cluster when playing back. Needs to be changed when kinematics are per-proxy to support
		// kinematic to dynamic transition for clusters.
		if(Parameters.EnableClustering)// && Parameters.CacheType != EGeometryCollectionCacheType::Play)
		{
			// build clusters.
			const int32 NumTransforms = Parameters.DynamicCollection->NumElements(FGeometryCollection::TransformGroup);
			for(int TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; TransformGroupIndex++)
			{
				// Clustering starts at the roots and recursively descends to the 
				// hierarchy to build the nested cluster bodies.
				if(BoneHierarchy[TransformGroupIndex].Parent == FGeometryCollectionBoneNode::InvalidBone && BoneHierarchy[TransformGroupIndex].Children.Num() > 0)
				{
					InitializeClustering(TransformGroupIndex, Particles);
				}
			}
		}

		const bool bKinematicMassOverride = Parameters.ObjectType == EObjectTypeEnum::Chaos_Object_Kinematic;
		if(bKinematicMassOverride)
		{
			// All created particles need to be set to kinematic
			const int32 CurrentNumParticles = Particles.Size();
			for(int32 Index = BaseParticleIndex; Index < CurrentNumParticles; ++Index)
			{
				Particles.InvM(Index) = 0.0f;
				Particles.InvI(Index) = Chaos::PMatrix<float, 3, 3>(0);
			}
		}
		GetSolver()->InitializeFromParticleData();

		// If we're recording and want to start immediately caching then we should cache the rest state
		if(Parameters.IsCacheRecording() && Parameters.CacheBeginTime == 0.0f)
		{
			if(UpdateRecordedStateCallback)
			{
				UpdateRecordedStateCallback(0.0f, RigidBodyId, BoneHierarchy, Particles, GetSolver()->GetCollisionRule());
			}
		}
	}
}

/**/
void FGeometryCollectionSolverCallbacks::BindParticleCallbackMapping(const int32 & CallbackIndex, FSolverCallbacks::IntArray & ParticleCallbackMap)
{
	if (InitializedState)
	{
		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;
		for (int32 Index = 0; Index < RigidBodyId.Num(); Index++)
		{
			if (RigidBodyId[Index] != Invalid)
				ParticleCallbackMap[RigidBodyId[Index]] = CallbackIndex;
		}
	}
}

void FGeometryCollectionSolverCallbacks::InitializeClustering(uint32 ParentIndex, FSolverCallbacks::FParticlesType& Particles)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::InitializeClustering()"));
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::InitializeClustering()"));

	FGeometryCollection * Collection = Parameters.DynamicCollection;
	check(Collection);

	TManagedArray<FGeometryCollectionBoneNode> & Bone = *Collection->BoneHierarchy;
	if (Bone[ParentIndex].Children.Num())
	{
		TManagedArray<int32> & RigidBodyID = *RigidBodyIdArray;

		// gather cluster arrays based on root transforms
		TArray<uint32> RigidChildren, CollectionChildren;
		for (const auto ChildIndex : Bone[ParentIndex].Children)
		{
			if (Bone[ChildIndex].Children.Num())
			{
				InitializeClustering(ChildIndex, Particles);
			}
			if (RigidBodyID[ChildIndex] != Invalid)
			{
				RigidChildren.Add(RigidBodyID[ChildIndex]);
				CollectionChildren.Add(ChildIndex);
			}
		}
		if (RigidChildren.Num())
		{
			BuildClusters(ParentIndex, CollectionChildren, RigidChildren);
		}

	}
}


void FGeometryCollectionSolverCallbacks::BuildClusters(uint32 CollectionClusterIndex, const TArray<uint32>& CollectionChildIDs, const TArray<uint32>& ChildIDs)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("FChaosSolver::BuildClusters()"));
	check(CollectionChildIDs.Num() == ChildIDs.Num());
	check(CollectionClusterIndex != Invalid);
	check(ChildIDs.Num() != 0);

	FGeometryCollection * Collection = Parameters.DynamicCollection;
	check(Collection);

	const Chaos::TPBDRigidParticles<float, 3>& Particles = GetSolver()->GetRigidParticles();
	TManagedArray<int32> & RigidBodyID = *RigidBodyIdArray;
	TManagedArray<int32>& SolverClusterID = *SolverClusterIDArray;
	TManagedArray<FGeometryCollectionBoneNode> & Bone = *Collection->BoneHierarchy;
	TManagedArray<FTransform> & Transform = *Collection->Transform;

	int NewSolverClusterID = GetSolver()->CreateClusterParticle(ChildIDs);

	// two-way mapping
	RigidBodyID[CollectionClusterIndex] = NewSolverClusterID;

	FTransform ClusterTransform(Particles.R(NewSolverClusterID), Particles.X(NewSolverClusterID));
	if (Bone[CollectionClusterIndex].Parent == Invalid)
	{
		Transform[CollectionClusterIndex] = ClusterTransform;
	}

	int32 NumThresholds = Parameters.DamageThreshold.Num();
	int32 Level = FMath::Clamp(Bone[CollectionClusterIndex].Level, 0, INT_MAX);
	float Default = NumThresholds > 0 ? Parameters.DamageThreshold[NumThresholds - 1] : 0;
	float Damage = Level < NumThresholds ? Parameters.DamageThreshold[Level] : Default;
	if (Level >= Parameters.MaxClusterLevel) Damage = FLT_MAX;

	GetSolver()->SetClusterStrain(NewSolverClusterID, Damage);
	for (int32 idx=0;idx<ChildIDs.Num();idx++)
	{
		GetSolver()->SetClusterStrain(ChildIDs[idx], Damage);

		int32 TransformGroupIndex = CollectionChildIDs[idx];
		SolverClusterID[TransformGroupIndex] = NewSolverClusterID;

		FTransform ConstituentTransform(Particles.R(ChildIDs[idx]), Particles.X(ChildIDs[idx]));

		if (Bone[TransformGroupIndex].Children.Num()) // clustered local transform
		{
			Transform[TransformGroupIndex] = ConstituentTransform.GetRelativeTransform(ClusterTransform);
		}
		else // rigid local transform
		{
			FTransform RestTransform = GeometryCollectionAlgo::GlobalMatrix(Parameters.RestCollection, TransformGroupIndex) * Parameters.WorldTransform;
			Transform[TransformGroupIndex] = RestTransform.GetRelativeTransform(ClusterTransform);
		}
		Transform[TransformGroupIndex].NormalizeRotation();
	}

	GetSolver()->InitializeFromParticleData();
}


void FGeometryCollectionSolverCallbacks::ParameterUpdateCallback(FSolverCallbacks::FParticlesType& Particles, const float Time)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::ParameterUpdateCallback()"));

	FGeometryCollection * Collection = Parameters.DynamicCollection;
	check(Collection);

	if (Collection->Transform->Num())
	{
		TManagedArray<int32>& RigidBodyId = *RigidBodyIdArray;
		TManagedArray<int32> & DynamicState = *DynamicStateArray;

		if (Parameters.FieldSystem != nullptr && StayDynamicFieldIndex != Invalid)
		{
			TArrayView<int32> IndexView(&(RigidBodyIdArray->operator[](0)), RigidBodyIdArray->Num());
			FVector * tptr = &(Particles.X(0));
			TArrayView<FVector> TransformView(tptr, int32(Particles.Size()));

			FFieldContext Context{
				StayDynamicFieldIndex,
				IndexView,
				TransformView,
				Parameters.FieldSystem
			};
			TArrayView<int32> DynamicStateView(&(DynamicState[0]), DynamicState.Num());

			if (Parameters.FieldSystem->GetNode(StayDynamicFieldIndex)->Type() == FFieldNode<int32>::StaticType())
			{
				Parameters.FieldSystem->Evaluate(Context, DynamicStateView);
			}
			else if (Parameters.FieldSystem->GetNode(StayDynamicFieldIndex)->Type() == FFieldNode<float>::StaticType())
			{
				TArray<float> FloatBuffer;
				FloatBuffer.SetNumUninitialized(DynamicState.Num());
				TArrayView<float> FloatBufferView(&FloatBuffer[0], DynamicState.Num());
				Parameters.FieldSystem->Evaluate<float>(Context, FloatBufferView);
				for (int i = 0; i < DynamicState.Num();i++)
				{
					DynamicStateView[i] = (int32)FloatBufferView[i];
				}
			}
			else
			{
				ensureMsgf(false, TEXT("Incorrect type specified in StayKinematic terminal.")); 
			}
		}

		bool MassChanged = false;
		for (int32 Index = 0; Index < DynamicState.Num(); Index++)
		{
			if (RigidBodyId[Index] != Invalid)
			{
				int32 RigidBodyIndex = RigidBodyId[Index];
				if (DynamicState[Index] == int32(EObjectTypeEnum::Chaos_Object_Dynamic)
					&& Particles.InvM(RigidBodyIndex) == 0.0
					&& FLT_EPSILON < Particles.M(RigidBodyIndex))
				{
					Particles.InvM(RigidBodyIndex) = 1.f / Particles.M(RigidBodyIndex);
					Particles.InvI(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(
						1.f / Particles.I(RigidBodyIndex).M[0][0], 0.f, 0.f,
						0.f, 1.f / Particles.I(RigidBodyIndex).M[1][1], 0.f,
						0.f, 0.f, 1.f / Particles.I(RigidBodyIndex).M[2][2]);
					Particles.SetSleeping(RigidBodyIndex, false);
					MassChanged = true;
				}
			}
		}

		if (Parameters.RecordedTrack)
		{
			float ReverseTime = Parameters.RecordedTrack->GetLastTime() - Time + Parameters.ReverseCacheBeginTime;
			// @todo(mlentine): We shouldn't need to do this every frame
			if (Parameters.IsCacheRecording() && Time > Parameters.ReverseCacheBeginTime && Parameters.ReverseCacheBeginTime != 0 && Parameters.RecordedTrack->IsTimeValid(ReverseTime))
			{
				for (int32 Index = 0; Index < RigidBodyId.Num(); Index++)
				{
					int32 RigidBodyIndex = RigidBodyId[Index];

					// Check index, will be invalid for cluster parents.
					if(RigidBodyIndex != INDEX_NONE)
					{
						Particles.InvM(RigidBodyIndex) = 0.f;
						Particles.InvI(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(0.f);
					}
				}
			}
		}
		/* @question : Should we tell the solver the mass has changed ? */
	}
}

void FGeometryCollectionSolverCallbacks::DisableCollisionsCallback(TSet<TTuple<int32, int32>>& CollisionPairs)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::DisableCollisionsCallback()"));
}


void FGeometryCollectionSolverCallbacks::StartFrameCallback(const float Dt, const float Time)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::StartFrameCallback()"));
	SCOPE_CYCLE_COUNTER(STAT_GeomBeginFrame);

	bool bIsReverseCachePlaying = Parameters.IsCacheRecording() && Parameters.ReverseCacheBeginTime != 0 && Time > Parameters.ReverseCacheBeginTime;
	if(Parameters.IsCachePlaying() || bIsReverseCachePlaying)
	{
		// Update the enabled/disabled state for kinematic particles for the upcoming frame
		Chaos::PBDRigidsSolver* ThisSolver = GetSolver();
		Chaos::PBDRigidsSolver::FParticlesType& Particles = ThisSolver->GetRigidParticles();
		TManagedArray<int32> & RigidBodyId = *RigidBodyIdArray;

		if (!Parameters.RecordedTrack)
		{
			if (!ensure(Parameters.CacheType == EGeometryCollectionCacheType::Record))
			{
				return;
			}
			Parameters.RecordedTrack = new FRecordedTransformTrack();
			Parameters.bOwnsTrack = true;
		}
		if (Parameters.bClearCache && bIsReverseCachePlaying)
		{
			CommitRecordedStateCallback(*const_cast<FRecordedTransformTrack*>(Parameters.RecordedTrack));
			Parameters.bClearCache = false;
		}
		
		bool bParticlesUpdated = false;

		const float ThisFrameTime = bIsReverseCachePlaying ? (Parameters.RecordedTrack->GetLastTime() - Time + Parameters.ReverseCacheBeginTime) : Time;
		if (!Parameters.RecordedTrack->IsTimeValid(ThisFrameTime))
		{
			// Invalid cache time, nothing to update
			return;
		}

		const int32 NumMappings = RigidBodyId.Num();
		Chaos::PhysicsParallelFor(NumMappings, [&](int32 InternalParticleIndex)
		//for(int32 Index = 0; Index < NumMappings; ++Index)
		{
			const int32 ExternalParticleIndex = RigidBodyId[InternalParticleIndex];

			if(ExternalParticleIndex == Invalid)
			{
				//continue;
				return;
			}

			if (Particles.InvM(ExternalParticleIndex) != 0)
			{
				return;
			}

			// We need to check a window of Now - Dt to Now and see if we ever activated in that time.
			// This is for short activations because if we miss one then the playback will be incorrect
			bool bShouldBeDisabled = !Parameters.RecordedTrack->GetWasActiveInWindow(InternalParticleIndex, ThisFrameTime, bIsReverseCachePlaying ? (ThisFrameTime - Dt) : (ThisFrameTime + Dt));

			bool& bDisabledNow = Particles.Disabled(ExternalParticleIndex);
			if(bDisabledNow != bShouldBeDisabled)
			{
				bParticlesUpdated = true;
				bDisabledNow = bShouldBeDisabled;
			}

		});

		// Do not add collisions if reverse
		if (!bIsReverseCachePlaying)
		{
			const FRecordedFrame* RecordedFrame = Parameters.RecordedTrack->FindRecordedFrame(ThisFrameTime);
			if (RecordedFrame == nullptr)
			{
				int32 Index = Parameters.RecordedTrack->FindLastKeyBefore(ThisFrameTime);
				RecordedFrame = &Parameters.RecordedTrack->Records[Index];
			}

			if (RecordedFrame)
			{
				// Build the collisionData for the ChaosNiagara DataInterface
				Chaos::PBDRigidsSolver::FCollisionData& CollisionData = ThisSolver->GetCollisionData();

				if (ThisFrameTime == 0.f)
				{
					CollisionData.TimeCreated = ThisFrameTime;
					CollisionData.NumCollisions = 0;
					CollisionData.CollisionDataArray.SetNum(ThisSolver->GetMaxCollisionDataSize());
				}
				else
				{
					if (ThisFrameTime - CollisionData.TimeCreated > ThisSolver->GetCollisionDataTimeWindow())
					{
						CollisionData.TimeCreated = ThisFrameTime;
						CollisionData.NumCollisions = 0;
						CollisionData.CollisionDataArray.Empty(ThisSolver->GetMaxCollisionDataSize());
						CollisionData.CollisionDataArray.SetNum(ThisSolver->GetMaxCollisionDataSize());
					}
				}

				int32 NumCollisions = RecordedFrame->Collisions.Num();
				for (int32 IdxCollision = 0; IdxCollision < RecordedFrame->Collisions.Num(); ++IdxCollision)
				{
					Chaos::TCollisionData<float, 3> CollisionDataItem{
						RecordedFrame->Collisions[IdxCollision].Time,
						RecordedFrame->Collisions[IdxCollision].Location,
						RecordedFrame->Collisions[IdxCollision].AccumulatedImpulse,
						RecordedFrame->Collisions[IdxCollision].Normal,
						RecordedFrame->Collisions[IdxCollision].Velocity1, RecordedFrame->Collisions[IdxCollision].Velocity2,
						RecordedFrame->Collisions[IdxCollision].Mass1, RecordedFrame->Collisions[IdxCollision].Mass2,
						RecordedFrame->Collisions[IdxCollision].ParticleIndex, RecordedFrame->Collisions[IdxCollision].LevelsetIndex
					};

					int32 Idx = CollisionData.NumCollisions % ThisSolver->GetMaxCollisionDataSize();
					CollisionData.CollisionDataArray[Idx] = CollisionDataItem;
					CollisionData.NumCollisions++;
				}

				// Build the trailingData for the ChaosNiagara DataInterface
				Chaos::PBDRigidsSolver::FTrailingData& TrailingData = ThisSolver->GetTrailingData();

				if (ThisFrameTime == 0.f)
				{
					TrailingData.TimeLastUpdated = 0.f;
					TrailingData.TrailingDataSet.Empty(ThisSolver->GetMaxTrailingDataSize());
				}
				else
				{
					if (ThisFrameTime - TrailingData.TimeLastUpdated > ThisSolver->GetTrailingDataTimeWindow())
					{
						TrailingData.TimeLastUpdated = ThisFrameTime;

						TrailingData.TrailingDataSet.Empty(ThisSolver->GetMaxTrailingDataSize());
						for (int32 IdxTrailing = 0; IdxTrailing < RecordedFrame->Trailings.Num(); ++IdxTrailing)
						{
							Chaos::TTrailingData<float, 3> TrailingDataItem{
								RecordedFrame->Trailings[IdxTrailing].TimeTrailingStarted,
								RecordedFrame->Trailings[IdxTrailing].Location,
								RecordedFrame->Trailings[IdxTrailing].ExtentMin,
								RecordedFrame->Trailings[IdxTrailing].ExtentMax,
								RecordedFrame->Trailings[IdxTrailing].Velocity,
								RecordedFrame->Trailings[IdxTrailing].AngularVelocity,
								RecordedFrame->Trailings[IdxTrailing].Mass,
								RecordedFrame->Trailings[IdxTrailing].ParticleIndex
							};
							TrailingData.TrailingDataSet.Add(TrailingDataItem);
						}
					}
					else
					{
						return;
					}
				}
			}
		}

		if (bParticlesUpdated)
		{
			ThisSolver->InitializeFromParticleData();
		}
	}
}

void FGeometryCollectionSolverCallbacks::EndFrameCallback(const float EndFrame)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::EndFrameCallback()"));

	FGeometryCollection * Collection = Parameters.DynamicCollection;
	check(Collection);

	ProxySimDuration += EndFrame;

	if (Collection->HasAttribute("RigidBodyID", FGeometryCollection::TransformGroup))
	{
		//
		//  Update transforms for the simulated transforms
		//

		TManagedArray<int32> & RigidBodyID = *RigidBodyIdArray;
		TManagedArray<int32>& CollectionClusterID = *SolverClusterIDArray;
		TManagedArray<FTransform>& Transform = *Collection->Transform;
		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *Collection->BoneHierarchy;
		TManagedArray<FTransform>& LocalToMass = *LocalToMassArray;
		TManagedArray<int32> & DynamicState = *DynamicStateArray;
		TManagedArray<FGeometryCollectionBoneNode> & Bone = *Collection->BoneHierarchy;

		Chaos::TPBDRigidParticles<float, 3>& Particles = GetSolver()->GetRigidParticles();
		Chaos::TPBDCollisionConstraint<float, 3>& CollisionRule = GetSolver()->GetCollisionRule();
		const Chaos::TArrayCollectionArray<Chaos::ClusterId>& ClusterID = GetSolver()->ClusterIds();
		const TArrayCollectionArray<Chaos::TRigidTransform<float, 3>>& ClusterChildToParentMap = GetSolver()->ClusterChildToParentMap();
		const TArrayCollectionArray<bool>& InternalCluster = GetSolver()->ClusterInternalCluster();
		
		//Particles X and R are aligned with center of mass and inertia principal axes.
		//Renderer doesn't know about this and simply does ActorToWorld * GeomToActor * LocalSpaceVerts
		//In proper math multiplication order:
		//ParticleToWorld = ActorToWorld * GeomToActor * LocalToMass
		//GeomToWorld = ActorToWorld * GeomToActor
		//=> GeomToWorld = ParticleToWorld * LocalToMass.Inv()
		//=> GeomToActor = ActorToWorld.Inv() * ParticleToWorld * LocalToMass.Inv()
		int32 TransformSize = Collection->NumElements(FGeometryCollection::TransformGroup);
		const FTransform& ActorToWorld = Parameters.WorldTransform;

		ParallelFor(TransformSize, [&](int32 TransformGroupIndex)
		{
			int32 RigidBodyIndex = RigidBodyID[TransformGroupIndex];
			if (RigidBodyIndex != Invalid)
			{
				// Update the transform and parent hierarchy of the active rigid bodies. Active bodies can be either
				// rigid geometry defined from the leaf nodes of the collection, or cluster bodies that drive an entire
				// branch of the hierarchy within the GeometryCollection.
				// - Active bodies are directly driven from the global position of the corresponding
				//   rigid bodies within the solver ( cases where RigidBodyID[TransformGroupIndex] is not disabled ). 
				// - Deactivated bodies are driven from the transforms of there active parents. However the solver can
				//   take ownership of the parents during the simulation, so it might be necessary to force deactivated
				//   bodies out of the collections hierarchy during the simulation.  
				if (!Particles.Disabled(RigidBodyID[TransformGroupIndex]))
				{
					// Update the transform of the active body. The active body can be either a single rigid
					// or a collection of rigidly attached geometries (Clustering). The cluster is represented as a
					// single transform in the GeometryCollection, and all children are stored in the local space
					// of the parent cluster.
					// ... When setting cluster transforms it is expected that the LocalToMass is the identity.
					//     Cluster initialization will set the vertices in the MassSpace of the rigid body.
					// ... When setting individual rigid bodies that are not clustered, the LocalToMass will be 
					//     non-Identity, and will reflect the difference between the geometric center of the geometry
					//     and that corresponding rigid bodies center of mass. 
					const FTransform ParticleToWorld(Particles.R(RigidBodyIndex), Particles.X(RigidBodyIndex));
					// GeomToActor = ActorToWorld.Inv() * ParticleToWorld * LocalToMass.Inv();
					Transform[TransformGroupIndex] = LocalToMass[TransformGroupIndex].GetRelativeTransformReverse(ParticleToWorld).GetRelativeTransform(ActorToWorld);
					Transform[TransformGroupIndex].NormalizeRotation();

					// dynamic state is also updated by the solver during field interaction. 
					if (!Particles.Sleeping(RigidBodyIndex))
					{
						DynamicState[TransformGroupIndex] = Particles.InvM(RigidBodyIndex) == 0 ? (int)EObjectTypeEnum::Chaos_Object_Kinematic : (int)EObjectTypeEnum::Chaos_Object_Dynamic;
					}

					// Force all enabled rigid bodies out of the transform hierarchy
					if (Hierarchy[TransformGroupIndex].Parent != Invalid)
					{
						int32 ParentIndex = Hierarchy[TransformGroupIndex].Parent;
						Hierarchy[ParentIndex].Children.Remove(TransformGroupIndex);
						Hierarchy[TransformGroupIndex].Parent = Invalid;
					}

					// When a leaf node rigid body is removed from a cluster the rigid
					// body will become active and needs its clusterID updated. This just
					// syncs the clusterID all the time. 
					CollectionClusterID[TransformGroupIndex] = ClusterID[RigidBodyIndex].Id;
				}
				else if (Particles.Disabled(RigidBodyIndex))
				{
					// The rigid body parent cluster has changed within the solver, and its
					// parent body is not tracked within the geometry collection. So we need to
					// pull the rigid bodies out of the transform hierarchy, and just drive
					// the positions directly from the solvers cluster particle. 
					if (CollectionClusterID[TransformGroupIndex] != ClusterID[RigidBodyIndex].Id)
					{
						// Force all driven rigid bodies out of the transform hierarchy
						if (Hierarchy[TransformGroupIndex].Parent != Invalid)
						{
							int32 ParentIndex = Hierarchy[TransformGroupIndex].Parent;
							Hierarchy[ParentIndex].Children.Remove(TransformGroupIndex);
							Hierarchy[TransformGroupIndex].Parent = Invalid;
						}
						CollectionClusterID[TransformGroupIndex] = ClusterID[RigidBodyIndex].Id;
					}

					// Disabled rigid bodies that have valid cluster parents, and have been re-indexed by the
					// solver ( As in, They were re-clustered outside of the geometry collection), These clusters 
					// will need to be rendered based on the clusters position. 
					int32 ClusterParentIndex = CollectionClusterID[TransformGroupIndex];
					if (ClusterParentIndex != Invalid)
					{
						if (InternalCluster[ClusterParentIndex])
						{
							const FTransform ActorToClusterChild = ClusterChildToParentMap[RigidBodyIndex]*
								FTransform(Particles.R(ClusterParentIndex), Particles.X(ClusterParentIndex));
							// GeomToActor = ActorToWorld.Inv() * ActorToClusterChild;
							Transform[TransformGroupIndex] = ActorToClusterChild.GetRelativeTransform(ActorToWorld);
							Transform[TransformGroupIndex].NormalizeRotation();
						}
					}
				}
			}
		});

		//
		//  Set rest cache on simulated object.
		//
		if(Parameters.IsCacheRecording())
		{
			if(UpdateRecordedStateCallback)
			{
				UpdateRecordedStateCallback(ProxySimDuration, *RigidBodyIdArray, Hierarchy, Particles, CollisionRule);
			}
		}
	}
}

// We assume this is running on the physics thread: should add checks for that
void FGeometryCollectionSolverCallbacks::UpdateKinematicBodiesCallback(const FSolverCallbacks::FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::UpdateKinematicBodiesCallback()"));

	SCOPE_CYCLE_COUNTER(STAT_KinematicUpdate);
	FGeometryCollection * Collection = Parameters.DynamicCollection;
	TManagedArray<int32>& RigidBodyId = *RigidBodyIdArray;
	check(Collection);

	bool bIsCachePlaying = Parameters.IsCachePlaying() && Parameters.RecordedTrack;
	bool bIsReverseCachePlaying = Parameters.IsCacheRecording() && Parameters.ReverseCacheBeginTime != 0 && Parameters.ReverseCacheBeginTime < Time;
	if (!bIsCachePlaying && !bIsReverseCachePlaying)
	{
		return;
	}

	bool bFirst = !Proxy.Ids.Num();
	if (bFirst)
	{
		Proxy.Position.Reset(RigidBodyId.Num());
		Proxy.Rotation.Reset(RigidBodyId.Num());
		Proxy.NextPosition.Reset(RigidBodyId.Num());
		Proxy.NextRotation.Reset(RigidBodyId.Num());

		Proxy.Position.AddUninitialized(RigidBodyId.Num());
		Proxy.Rotation.AddUninitialized(RigidBodyId.Num());
		Proxy.NextPosition.AddUninitialized(RigidBodyId.Num());
		Proxy.NextRotation.AddUninitialized(RigidBodyId.Num());

		for (int32 i = 0; i < RigidBodyId.Num(); ++i)
		{
			Proxy.Ids.Add(RigidBodyId[i]);

			// Initialise to rest state
			const int32 RbId = Proxy.Ids.Last();
			Proxy.Position[i] = RbId != INDEX_NONE ? Particles.X(RbId) : FVector::ZeroVector;
			Proxy.Rotation[i] = RbId != INDEX_NONE ? Particles.R(RbId) : FQuat::Identity;
			Proxy.NextPosition[i] = Proxy.Position[i];
			Proxy.NextRotation[i] = Proxy.Rotation[i];
		}
	}

	if (bIsCachePlaying && !bIsReverseCachePlaying && (Time < Parameters.CacheBeginTime || !Parameters.RecordedTrack->IsTimeValid(Time)))
	{
		return;
	}
	
	float ReverseTime = Parameters.RecordedTrack->GetLastTime() - Time + Parameters.ReverseCacheBeginTime;
	if (bIsReverseCachePlaying && !Parameters.RecordedTrack->IsTimeValid(ReverseTime))
	{	
		return;
	}

	const FRecordedFrame* FirstFrame = nullptr;
	const FRecordedFrame* SecondFrame = nullptr;
	float PlaybackTime = bIsReverseCachePlaying ? ReverseTime : Time;
	Parameters.RecordedTrack->GetFramesForTime(PlaybackTime, FirstFrame, SecondFrame);

	if(FirstFrame && !SecondFrame)
	{
		// Only one frame to take information from (simpler case)
		const int32 NumActives = FirstFrame->TransformIndices.Num();

		// Actives
		Chaos::PhysicsParallelFor(NumActives, [&](int32 Index)
		{
			const int32 InternalIndex = FirstFrame->TransformIndices[Index];
			const int32 ExternalIndex = RigidBodyId[InternalIndex];

			if(ExternalIndex != INDEX_NONE && Particles.InvM(ExternalIndex) == 0.0f && !Particles.Disabled(ExternalIndex))
			{
				const FTransform& ParticleTransform = FirstFrame->Transforms[Index];
				Proxy.Position[InternalIndex] = Particles.X(ExternalIndex);
				Proxy.Rotation[InternalIndex] = Particles.R(ExternalIndex);
				Proxy.NextPosition[InternalIndex] = ParticleTransform.GetTranslation();
				Proxy.NextRotation[InternalIndex] = ParticleTransform.GetRotation();
			}
		});
	}
	else if(FirstFrame)
	{
		// Both frames valid, second frame has all the indices we need
		const int32 NumActives = SecondFrame->TransformIndices.Num();

		const float Alpha = (PlaybackTime - FirstFrame->Timestamp) / (SecondFrame->Timestamp - FirstFrame->Timestamp);
		check(0 <= Alpha && Alpha <= 1.0f);

		Chaos::PhysicsParallelFor(NumActives, [&](int32 Index)
		{
			const int32 InternalIndex = SecondFrame->TransformIndices[Index];
			const int32 PreviousIndexSlot = Index < SecondFrame->PreviousTransformIndices.Num() ? SecondFrame->PreviousTransformIndices[Index] : INDEX_NONE;
			
			const int32 ExternalIndex = RigidBodyId[InternalIndex];

			if(ExternalIndex != INDEX_NONE && Particles.InvM(ExternalIndex) == 0.0f && !Particles.Disabled(ExternalIndex))
			{
				if(PreviousIndexSlot != INDEX_NONE)
				{
					Proxy.Position[InternalIndex] = Proxy.NextPosition[InternalIndex];
					Proxy.Rotation[InternalIndex] = Proxy.NextRotation[InternalIndex];

					FTransform BlendedTM;
					BlendedTM.Blend(FirstFrame->Transforms[PreviousIndexSlot], SecondFrame->Transforms[Index], Alpha);

					Proxy.NextPosition[InternalIndex] = BlendedTM.GetTranslation();
					Proxy.NextRotation[InternalIndex] = BlendedTM.GetRotation();
				}
				else
				{
					// NewActive case
					Proxy.Position[InternalIndex] = Proxy.NextPosition[InternalIndex];
					Proxy.Rotation[InternalIndex] = Proxy.NextRotation[InternalIndex];

					FTransform BlendedTM;
					BlendedTM.Blend(FTransform(Particles.R(ExternalIndex), Particles.X(ExternalIndex), FVector::OneVector), SecondFrame->Transforms[Index], Alpha);

					Proxy.NextPosition[InternalIndex] = BlendedTM.GetTranslation();
					Proxy.NextRotation[InternalIndex] = BlendedTM.GetRotation();
				}
			}
		});

		// #BGallagher Handle new inactives. If it's a cluster parent and it's fully disabled we'll need to decluster it here.
	}
}

void FGeometryCollectionSolverCallbacks::AddConstraintCallback(FSolverCallbacks::FParticlesType& Particles, const float Time, const int32 Island)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::AddConstraintCallback()"));

}


void FGeometryCollectionSolverCallbacks::AddForceCallback(FSolverCallbacks::FParticlesType& Particles, const float Dt, const int32 Index)
{
	UE_LOG(GeometryCollectionSolverCallbacksLogging, Verbose, TEXT("GeometryCollectionSolverCallbacks::AddForceCallback()"));
	// gravity forces managed directly on the solver for now
}
#else
FGeometryCollectionSolverCallbacks::FGeometryCollectionSolverCallbacks() :
	RigidBodyIdArray(new TManagedArray<int32>())
{}
#endif
