// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleClustering.h"
#include "GeometryCollection/GeometryCollectionExampleUtility.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"

#include "GeometryCollection/TransformCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "Field/FieldSystemCoreAlgo.h"
#include "Field/FieldSystemSimulationCoreCallbacks.h"

DEFINE_LOG_CATEGORY_STATIC(GCTCL_Log, Verbose, All);


namespace GeometryCollectionExample
{
	
	template<class T>
	bool RigidBodies_ClusterTest_SingleLevelNonBreaking(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(0, -10, 10)),FVector(1.0));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(0, 10, 10)), FVector(1.0)));
		R.ExpectTrue(RestCollection->Transform->Num() == 2);


		FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(RestCollection.Get());
		R.ExpectTrue(RestCollection->Transform->Num() == 3);
		RestCollection->Transform->operator[](2) = FTransform(FQuat::MakeFromEuler(FVector(90.f, 0, 0.)), FVector(0, 0, 40));

		//GeometryCollectionAlgo::PrintParentHierarchy(RestCollection.Get());

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
		Parameters.DamageThreshold = { 1000.f };
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float StartingRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size(), CurrentRigidDistance = 0.f;



		for (int Frame = 0; Frame < 10; Frame++)
		{
			//UE_LOG(GCTCL_Log, Verbose, TEXT("Frame[%d]"), Frame);

			Solver.AdvanceSolverBy(1 / 24.);
			const Chaos::TPBDRigidParticles<float, 3>& Particles = SolverCallbacks.GetSolver()->GetRigidParticles();
			CurrentRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size();

			//for (int rdx = 0; rdx < Transform.Num(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ... Position[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, Transform[rdx].GetTranslation().X, Transform[rdx].GetTranslation().Y, Transform[rdx].GetTranslation().Z);
			//}
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//}
			//UE_LOG(GCTCL_Log, Verbose, TEXT("StartingRigidDistance : %3.5f"), StartingRigidDistance);
			//UE_LOG(GCTCL_Log, Verbose, TEXT("DeltaRigidDistance : %3.5f"), CurrentRigidDistance - StartingRigidDistance);

			R.ExpectTrue(Particles.Disabled(0) == false);
			R.ExpectTrue(Particles.Disabled(1) == true);
			R.ExpectTrue(Particles.Disabled(2) == true);
			R.ExpectTrue(Particles.Disabled(3) == false);

			R.ExpectTrue(FMath::Abs(CurrentRigidDistance - StartingRigidDistance) < 1e-4);
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_ClusterTest_SingleLevelNonBreaking<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodies_ClusterTest_DeactivateClusterParticle(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(20.f)),FVector(1.0));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(30.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(40.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(50.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(50.f)), FVector(1.0)));

		RestCollection->AddElements(4, FGeometryCollection::TransformGroup);
		// @todo(ClusteringUtils) This is a bad assumption, the state flags should be initialized to zero.
		(*RestCollection->BoneHierarchy)[5].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[6].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[7].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[8].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;

		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 5, { 4,3 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 6, { 5,2 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 7, { 6,1 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 8, { 7,0 });

		(*RestCollection->BoneHierarchy)[0].Level = 4;
		(*RestCollection->BoneHierarchy)[1].Level = 3;
		(*RestCollection->BoneHierarchy)[2].Level = 2;
		(*RestCollection->BoneHierarchy)[3].Level = 1;
		(*RestCollection->BoneHierarchy)[4].Level = 0;
		(*RestCollection->BoneHierarchy)[5].Level = 3;
		(*RestCollection->BoneHierarchy)[6].Level = 2;
		(*RestCollection->BoneHierarchy)[7].Level = 1;
		(*RestCollection->BoneHierarchy)[8].Level = 0;

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.ObjectType = EObjectTypeEnum::Chaos_Object_Kinematic;
		Parameters.MaxClusterLevel = 1;
		Parameters.DamageThreshold = { 50.0, 50.0, 50.0, FLT_MAX };
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float StartingRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size(), CurrentRigidDistance = 0.f;

		TArray<bool> Conditions = { false,false };

		for (int Frame = 0; Frame < 4; Frame++)
		{
			Solver.AdvanceSolverBy(1 / 24.);
			const Chaos::TPBDRigidParticles<float, 3>& Particles = SolverCallbacks.GetSolver()->GetRigidParticles();

			if (Frame == 2)
			{
				Solver.DeactivateClusterParticle({ 9 });
			}

			//UE_LOG(GCTCL_Log, Verbose, TEXT("FRAME : %d"), Frame);
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...    InvM[%d] : %f"), rdx, Particles.InvM(rdx));
			//}

			if (Conditions[0] == false && Frame == 1)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == true &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == true &&
					Particles.Disabled(8) == true &&
					Particles.Disabled(9) == false)
				{
					Conditions[0] = true;
					R.ExpectTrue(Particles.InvM(9) == 0.f); // kinematic cluster
					R.ExpectTrue(Particles.InvM(8) == 0.f); // disabled child
					R.ExpectTrue(Particles.InvM(1) == 0.f); // disabled child
				}
			}
			else if (Conditions[0] == true && Conditions[1] == false && Frame == 2)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == false &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == true &&
					Particles.Disabled(8) == false &&
					Particles.Disabled(9) == true)
				{
					Conditions[1] = true;
					R.ExpectTrue(Particles.InvM(9) == 0.f); // disabled cluster body
					R.ExpectTrue(Particles.InvM(1) != 0.f); // enabled child
					R.ExpectTrue(Particles.InvM(8) != 0.f); // enabled child
				}
			}
		}
		for (int i = 0; i < Conditions.Num(); i++)
		{
			R.ExpectTrue(Conditions[i]);
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_ClusterTest_DeactivateClusterParticle<float>(ExampleResponse&& R);



	template<class T>
	bool RigidBodies_ClusterTest_SingleLevelBreaking(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(0, -10, 10)),FVector(1.0));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(0, 10, 10)), FVector(1.0)));
		R.ExpectTrue(RestCollection->Transform->Num() == 2);


		FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(RestCollection.Get());
		R.ExpectTrue(RestCollection->Transform->Num() == 3);
		RestCollection->Transform->operator[](2) = FTransform(FQuat::MakeFromEuler(FVector(90.f, 0, 0.)), FVector(0, 0, 40));

		//GeometryCollectionAlgo::PrintParentHierarchy(RestCollection.Get());

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.DamageThreshold = { 0.1f };
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float StartingRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size(), CurrentRigidDistance = 0.f;



		for (int Frame = 0; Frame < 10; Frame++)
		{
			//UE_LOG(GCTCL_Log, Verbose, TEXT("Frame[%d]"), Frame);

			Solver.AdvanceSolverBy(1 / 24.);
			const Chaos::TPBDRigidParticles<float, 3>& Particles = SolverCallbacks.GetSolver()->GetRigidParticles();
			CurrentRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size();

			//for (int rdx = 0; rdx < Transform.Num(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ... Position[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, Transform[rdx].GetTranslation().X, Transform[rdx].GetTranslation().Y, Transform[rdx].GetTranslation().Z);
			//}
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//}
			//UE_LOG(GCTCL_Log, Verbose, TEXT("StartingRigidDistance : %3.5f"), StartingRigidDistance);
			//UE_LOG(GCTCL_Log, Verbose, TEXT("DeltaRigidDistance : %3.5f"), CurrentRigidDistance - StartingRigidDistance);

			if (Frame < 5)
			{
				R.ExpectTrue(Particles.Disabled(0) == false);
				R.ExpectTrue(Particles.Disabled(1) == true);
				R.ExpectTrue(Particles.Disabled(2) == true);
				R.ExpectTrue(Particles.Disabled(3) == false);
			}
			else
			{
				R.ExpectTrue(Particles.Disabled(0) == false);
				R.ExpectTrue(Particles.Disabled(1) == false);
				R.ExpectTrue(Particles.Disabled(2) == false);
				R.ExpectTrue(Particles.Disabled(3) == true);
			}


			if (Frame <= 5)
			{
				R.ExpectTrue(FMath::Abs(CurrentRigidDistance - StartingRigidDistance) < 1e-4);
			}
			else
			{
				R.ExpectTrue(FMath::Abs(CurrentRigidDistance - StartingRigidDistance) > 1e-4);

			}
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_ClusterTest_SingleLevelBreaking<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodies_ClusterTest_NestedCluster(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(0, -10, 10)),FVector(1.0));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0, 0, 0.)), FVector(0, 10, 10)), FVector(1.0)));
		R.ExpectTrue(RestCollection->Transform->Num() == 2);

		FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(RestCollection.Get());
		R.ExpectTrue(RestCollection->Transform->Num() == 3);
		RestCollection->Transform->operator[](2) = FTransform(FQuat::MakeFromEuler(FVector(90.f, 0, 0.)), FVector(0, 0, 40));

		FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(RestCollection.Get(), 3, { 2 }, true);
		R.ExpectTrue(RestCollection->Transform->Num() == 4);
		RestCollection->Transform->operator[](3) = FTransform(FQuat::MakeFromEuler(FVector(0.f, 0, 0.)), FVector(0, 0, 10));

		//GeometryCollectionAlgo::PrintParentHierarchy(RestCollection.Get());

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.DamageThreshold = { 0.1f };
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float StartingRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size(), CurrentRigidDistance = 0.f;

		TArray<bool> Conditions = {false,false,false};

		for (int Frame = 0; Frame < 20; Frame++)
		{
			Solver.AdvanceSolverBy(1 / 24.);

			const Chaos::TPBDRigidParticles<float, 3>& Particles = SolverCallbacks.GetSolver()->GetRigidParticles();
			CurrentRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size();

			//UE_LOG(GCTCL_Log, Verbose, TEXT("FRAME : %d"), Frame);
			//for (int rdx = 0; rdx < Transform.Num(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ... Position[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, Transform[rdx].GetTranslation().X, Transform[rdx].GetTranslation().Y, Transform[rdx].GetTranslation().Z);
			//}
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//}
			//UE_LOG(GCTCL_Log, Verbose, TEXT("StartingRigidDistance : %3.5f"), StartingRigidDistance);
			//UE_LOG(GCTCL_Log, Verbose, TEXT("DeltaRigidDistance : %3.5f"), CurrentRigidDistance - StartingRigidDistance);

			if (Conditions[0]==false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == true &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == false) 
				{
					Conditions[0] = true;
				}
			}
			else if (Conditions[0]==true && Conditions[1] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == true &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == false &&
					Particles.Disabled(4) == true)
				{
					Conditions[1] = true;
				}
			}
			else if (Conditions[1] == true && Conditions[2] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == false &&
					Particles.Disabled(2) == false &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true)
				{
					Conditions[2] = true;
				}
			}
		}
		for (int i = 0; i < Conditions.Num(); i++)
		{
			R.ExpectTrue(Conditions[i]);
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_ClusterTest_NestedCluster<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodies_ClusterTest_NestedCluster_MultiStrain(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(20.f)),FVector(1.0));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(30.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(40.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(50.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(50.f)), FVector(1.0)));

		RestCollection->AddElements(4, FGeometryCollection::TransformGroup);
		// @todo(ClusteringUtils) This is a bad assumption, the state flags should be initialized to zero.
		(*RestCollection->BoneHierarchy)[5].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[6].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[7].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[8].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;

		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 5, { 4,3 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 6, { 5,2 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 7, { 6,1 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 8, { 7,0 });

		(*RestCollection->BoneHierarchy)[0].Level = 4;
		(*RestCollection->BoneHierarchy)[1].Level = 3;
		(*RestCollection->BoneHierarchy)[2].Level = 2;
		(*RestCollection->BoneHierarchy)[3].Level = 1;
		(*RestCollection->BoneHierarchy)[4].Level = 0;
		(*RestCollection->BoneHierarchy)[5].Level = 3;
		(*RestCollection->BoneHierarchy)[6].Level = 2;
		(*RestCollection->BoneHierarchy)[7].Level = 1;
		(*RestCollection->BoneHierarchy)[8].Level = 0;

		// @todo(brice->Bill.Henderson) Why did this not work? I needed to build my own parenting and level initilization. 
		//FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(RestCollection.Get());
		//FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(RestCollection.Get(), 4, { 0, 1 }, true);
		//FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(RestCollection.Get(), 4, { 2, 3 }, true);

		GeometryCollectionAlgo::PrintParentHierarchy(RestCollection.Get());

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.DamageThreshold = { 50.0, 50.0, 50.0, FLT_MAX };
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float StartingRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size(), CurrentRigidDistance = 0.f;

		TArray<bool> Conditions = { false,false,false };

		for (int Frame = 0; Frame < 20; Frame++)
		{
			Solver.AdvanceSolverBy(1 / 24.);

			const Chaos::TPBDRigidParticles<float, 3>& Particles = SolverCallbacks.GetSolver()->GetRigidParticles();
			CurrentRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size();

			//UE_LOG(GCTCL_Log, Verbose, TEXT("FRAME : %d"), Frame);
			//for (int rdx = 0; rdx < Transform.Num(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ... Position[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, Transform[rdx].GetTranslation().X, Transform[rdx].GetTranslation().Y, Transform[rdx].GetTranslation().Z);
			//}
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//}
			//UE_LOG(GCTCL_Log, Verbose, TEXT("StartingRigidDistance : %3.5f"), StartingRigidDistance);
			//UE_LOG(GCTCL_Log, Verbose, TEXT("DeltaRigidDistance : %3.5f"), CurrentRigidDistance - StartingRigidDistance);
			

			if (Conditions[0] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == true &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == true &&
					Particles.Disabled(8) == true &&
					Particles.Disabled(9) == false)
				{
					Conditions[0] = true;
				}
			}
			else if (Conditions[0] == true && Conditions[1] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == false &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == true &&
					Particles.Disabled(8) == false &&
					Particles.Disabled(9) == true)
				{
					Conditions[1] = true;
				}
			}
			else if (Conditions[1] == true && Conditions[2] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == false &&
					Particles.Disabled(2) == false &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == false &&
					Particles.Disabled(8) == true &&
					Particles.Disabled(9) == true)
				{
					Conditions[2] = true;
				}
			}
		}
		for (int i = 0; i < Conditions.Num(); i++)
		{
			R.ExpectTrue(Conditions[i]);
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_ClusterTest_NestedCluster_MultiStrain<float>(ExampleResponse&& R);



	template<class T>
	bool RigidBodies_ClusterTest_NestedCluster_Halt(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(20.f)),FVector(1.0));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(30.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(40.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(50.f)), FVector(1.0)));
		RestCollection->AppendGeometry(*GeometryCollection::MakeCubeElement(FTransform(FQuat::MakeFromEuler(FVector(0.f)), FVector(50.f)), FVector(1.0)));

		RestCollection->AddElements(4, FGeometryCollection::TransformGroup);
		// @todo(ClusteringUtils) This is a bad assumption, the state flags should be initialized to zero.
		(*RestCollection->BoneHierarchy)[5].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[6].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[7].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;
		(*RestCollection->BoneHierarchy)[8].StatusFlags = FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered;

		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 5, { 4,3 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 6, { 5,2 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 7, { 6,1 });
		GeometryCollectionAlgo::ParentTransforms(RestCollection.Get(), 8, { 7,0 });

		(*RestCollection->BoneHierarchy)[0].Level = 4;
		(*RestCollection->BoneHierarchy)[1].Level = 3;
		(*RestCollection->BoneHierarchy)[2].Level = 2;
		(*RestCollection->BoneHierarchy)[3].Level = 1;
		(*RestCollection->BoneHierarchy)[4].Level = 0;
		(*RestCollection->BoneHierarchy)[5].Level = 3;
		(*RestCollection->BoneHierarchy)[6].Level = 2;
		(*RestCollection->BoneHierarchy)[7].Level = 1;
		(*RestCollection->BoneHierarchy)[8].Level = 0;

		// @todo(brice->Bill.Henderson) Why did this not work? I needed to build my own parenting and level initilization. 
		//FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(RestCollection.Get());
		//FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(RestCollection.Get(), 4, { 0, 1 }, true);
		//FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(RestCollection.Get(), 4, { 2, 3 }, true);

		GeometryCollectionAlgo::PrintParentHierarchy(RestCollection.Get());

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.MaxClusterLevel = 1;
		Parameters.DamageThreshold = { 50.0, 50.0, 50.0, FLT_MAX };
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float StartingRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size(), CurrentRigidDistance = 0.f;

		TArray<bool> Conditions = { false,false };

		for (int Frame = 0; Frame < 10; Frame++)
		{
			Solver.AdvanceSolverBy(1 / 24.);

			const Chaos::TPBDRigidParticles<float, 3>& Particles = SolverCallbacks.GetSolver()->GetRigidParticles();
			CurrentRigidDistance = (Transform[1].GetTranslation() - Transform[0].GetTranslation()).Size();

			//UE_LOG(GCTCL_Log, Verbose, TEXT("FRAME : %d"), Frame);
			//for (int rdx = 0; rdx < Transform.Num(); rdx++)
			//{
			//UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ... Position[%d] : (%3.5f,%3.5f,%3.5f)"), rdx, Transform[rdx].GetTranslation().X, Transform[rdx].GetTranslation().Y, Transform[rdx].GetTranslation().Z);
			//}
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//}
			//UE_LOG(GCTCL_Log, Verbose, TEXT("StartingRigidDistance : %3.5f"), StartingRigidDistance);
			//UE_LOG(GCTCL_Log, Verbose, TEXT("DeltaRigidDistance : %3.5f"), CurrentRigidDistance - StartingRigidDistance);
			
			
			if (Conditions[0] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == true &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == true &&
					Particles.Disabled(8) == true &&
					Particles.Disabled(9) == false)
				{
					Conditions[0] = true;
				}
			}
			else if (Conditions[0] == true && Conditions[1] == false)
			{
				if (Particles.Disabled(0) == false &&
					Particles.Disabled(1) == false &&
					Particles.Disabled(2) == true &&
					Particles.Disabled(3) == true &&
					Particles.Disabled(4) == true &&
					Particles.Disabled(5) == true &&
					Particles.Disabled(6) == true &&
					Particles.Disabled(7) == true &&
					Particles.Disabled(8) == false &&
					Particles.Disabled(9) == true)
				{
					Conditions[1] = true;
				}
			}
		}
		for (int i = 0; i < Conditions.Num(); i++)
		{
			R.ExpectTrue(Conditions[i]);
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_ClusterTest_NestedCluster_Halt<float>(ExampleResponse&& R);
	
}
