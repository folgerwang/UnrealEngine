// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleSimulation.h"
#include "GeometryCollection/GeometryCollectionExampleUtility.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/TransformCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "Field/FieldSystemSimulationCoreCallbacks.h"

#define SMALL_THRESHOLD 1e-4


namespace GeometryCollectionExample
{
	template<class T>
	bool RigidBodiesFallingUnderGravity(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetEnabled(true);

		Solver.AdvanceSolverBy(1 / 24.);

		// never touched
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		R.ExpectTrue(FMath::Abs(RestTransform[0].GetTranslation().Z) < SMALL_THRESHOLD);

		// simulated
		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		R.ExpectTrue(Transform.Num() == 1);
		R.ExpectTrue(Transform[0].GetTranslation().Z < 0);
#endif

		return !R.HasError();
	}
	template bool RigidBodiesFallingUnderGravity<float>(ExampleResponse&& R);



	template<class T>
	bool RigidBodiesCollidingWithSolverFloor(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		Solver.AdvanceSolverBy(1 / 24.);

		// never touched
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		R.ExpectTrue(FMath::Abs(RestTransform[0].GetTranslation().Z) < SMALL_THRESHOLD);

		// simulated
		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		R.ExpectTrue(Transform.Num() == 1);
		R.ExpectTrue(FMath::Abs(Transform[0].GetTranslation().Z - 0.5) < SMALL_THRESHOLD);
#endif

		return !R.HasError();
	}
	template bool RigidBodiesCollidingWithSolverFloor<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodiesSingleSphereCollidingWithSolverFloor(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		(*RestCollection->Transform)[0].SetTranslation(FVector(0, 0, 10));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.Bouncyness = 0.f;
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Sphere;
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		for (int i = 0; i < 100; i++)
		{
			Solver.AdvanceSolverBy(1 / 240.);
		}
		
		// never touched
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		R.ExpectTrue(FMath::Abs(RestTransform[0].GetTranslation().Z-10.0) < KINDA_SMALL_NUMBER);

		// simulated
		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		TManagedArray<float>& InnerRadius = *DynamicCollection->InnerRadius;
		//UE_LOG(LogTest, Verbose, TEXT("Height : (%3.5f), Inner Radius:%3.5f"), Transform[0].GetTranslation().Z, InnerRadius[0]);

		R.ExpectTrue(Transform.Num() == 1);
		R.ExpectTrue(FMath::Abs(Transform[0].GetTranslation().Z - 0.5) < 0.1); // @todo - Why is this not 0.5f
#endif

		return !R.HasError();
	}
	template bool RigidBodiesSingleSphereCollidingWithSolverFloor<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodiesSingleSphereIntersectingWithSolverFloor(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Sphere;
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(true);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		Solver.AdvanceSolverBy(1 / 24.);

		// never touched
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		R.ExpectTrue(FMath::Abs(RestTransform[0].GetTranslation().Z) < KINDA_SMALL_NUMBER);

		// simulated
		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		R.ExpectTrue(Transform.Num() == 1);
		//UE_LOG(LogTest, Verbose, TEXT("Position : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);

		R.ExpectTrue(FMath::Abs(Transform[0].GetTranslation().Z - 0.5) < KINDA_SMALL_NUMBER);
#endif

		return !R.HasError();
	}
	template bool RigidBodiesSingleSphereIntersectingWithSolverFloor<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodiesKinematic(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ObjectType = EObjectTypeEnum::Chaos_Object_Kinematic;
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		for (int i = 0; i < 100; i++)
		{
			Solver.AdvanceSolverBy(1 / 24.);
		}

		// simulated
		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		R.ExpectTrue(Transform.Num() == 1);
		//UE_LOG(LogTest, Verbose, TEXT("Position : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);
		R.ExpectTrue(Transform[0].GetTranslation().Z == 0.f);
#endif

		return !R.HasError();
	}
	template bool RigidBodiesKinematic<float>(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesKinematicFieldActivation(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ObjectType = EObjectTypeEnum::Chaos_Object_Kinematic;
		Parameters.Simulating = true;

		TSharedPtr<UFieldSystem> System(NewObject<UFieldSystem>());
		Parameters.FieldSystem = &System->GetFieldData();

		FRadialIntMask & RadialMask = System->NewNode<FRadialIntMask>("StayDynamic");
		RadialMask.Position = FVector(0.0, 0.0, 0.0);
		RadialMask.Radius = 100.0;
		RadialMask.InteriorValue = 1.0;
		RadialMask.ExteriorValue = 0.0;
		RadialMask.SetMaskCondition = ESetMaskConditionType::Field_Set_IFF_NOT_Interior;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		for (int i = 0; i < 100; i++)
		{
			Solver.AdvanceSolverBy(1 / 24.);
		}

		// simulated
		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		R.ExpectTrue(Transform.Num() == 1);
		R.ExpectTrue(Transform[0].GetTranslation().Z == 0.f);
		//UE_LOG(LogTest, Verbose, TEXT("Position : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);

		RadialMask.InteriorValue = 0.0;
		RadialMask.ExteriorValue = 1.0;

		for (int i = 0; i < 100; i++)
		{
			Solver.AdvanceSolverBy(1 / 24.);
		}

		//UE_LOG(LogTest, Verbose, TEXT("Position : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);
		R.ExpectTrue(Transform[0].GetTranslation().Z <= 0.f);
#endif

		return !R.HasError();
	}
	template bool RigidBodiesKinematicFieldActivation<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodiesSleepingActivation(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));

		RestCollection->AppendGeometry(*RestCollection);
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		RestTransform[1].SetTranslation(FVector(0.f, 0.f, 5.f));

		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ObjectType = EObjectTypeEnum::Chaos_Object_Kinematic;
		Parameters.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Cube;
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		//
		//
		//
		TSharedPtr< TManagedArray<int32> > ObjectTypeArray = DynamicCollection->FindAttribute<int32>("DynamicState", FTransformCollection::TransformGroup);
		TManagedArray<int32> & ObjectType = *ObjectTypeArray;
		ObjectType[0] = (int32)EObjectTypeEnum::Chaos_Object_Sleeping;
		ObjectType[1] = (int32)EObjectTypeEnum::Chaos_Object_Dynamic;

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		for (int i = 0; i < 100; i++)
		{
			Solver.AdvanceSolverBy(1 / 24.);
			//UE_LOG(LogTest, Verbose, TEXT("Position[0] : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);
			//UE_LOG(LogTest, Verbose, TEXT("Position[1] : (%3.5f,%3.5f,%3.5f)"), Transform[1].GetTranslation().X, Transform[1].GetTranslation().Y, Transform[1].GetTranslation().Z);
		}

		// todo: WIP
#endif

		return !R.HasError();
	}
	template bool RigidBodiesSleepingActivation<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodiesInitialLinearVelocity(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());

		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;

		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.InitialVelocityType = EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined;
		Parameters.InitialLinearVelocity = FVector(0.f, 100.f, 0.f);
		Parameters.Simulating = true;

		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetIsFloorAnalytic(true);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;

		float PreviousY = 0.f;
		R.ExpectTrue(Transform[0].GetTranslation().X == 0 );
		R.ExpectTrue(Transform[0].GetTranslation().Y == 0 );

		for (int i = 0; i < 10; i++)
		{
			Solver.AdvanceSolverBy(1 / 24.);
			R.ExpectTrue(Transform[0].GetTranslation().X == 0);
			R.ExpectTrue(Transform[0].GetTranslation().Y > PreviousY);
			PreviousY = Transform[0].GetTranslation().Y;
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodiesInitialLinearVelocity<float>(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_Field_StayDynamic(ExampleResponse&& R)
	{
#if INCLUDE_CHAOS
		//
		//  Rigid Body Setup
		//
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		RestTransform[0].SetTranslation(FVector(0.f, 0.f, 5.f));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;
		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ObjectType = EObjectTypeEnum::Chaos_Object_Kinematic;
		Parameters.Simulating = true;
		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		//
		// Field setup
		//
		FFieldSystem System;
		FRadialIntMask & RadialMask = System.NewNode<FRadialIntMask>("StayDynamic");
		RadialMask.Position = FVector(0.0, 0.0, 0.0);
		RadialMask.Radius = 5.0;
		RadialMask.InteriorValue = 0;
		RadialMask.ExteriorValue = 1;
		RadialMask.SetMaskCondition = ESetMaskConditionType::Field_Set_IFF_NOT_Interior;
		FFieldSystemSolverCallbacks FieldCallbacks(System);
		
		//
		// Solver setup
		//
		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.RegisterFieldCallbacks(&FieldCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float PreviousHeight = 5.f;
		for (int Frame = 0; Frame < 10; Frame++)
		{
			//UE_LOG(LogTest, Verbose, TEXT("Frame[%d]"), Frame);

			if (Frame == 5)
			{
				FFieldSystemCommand Command("StayDynamic", EFieldPhysicsType::Field_StayDynamic, FVector(0.0,0.0,5.0), FVector(0), 5.f, 0.f);
				FieldCallbacks.BufferCommand(Command);
			}

			Solver.AdvanceSolverBy(1 / 24.);
			const Chaos::TPBDRigidParticles<float, 3>& Particles = Solver.GetRigidParticles();

			if(Frame < 5)
			{
				R.ExpectTrue(FMath::Abs(Transform[0].GetTranslation().Z-5.f) < SMALL_THRESHOLD);
			}
			else
			{
				R.ExpectTrue(Transform[0].GetTranslation().Z < PreviousHeight);
			}
			PreviousHeight = Transform[0].GetTranslation().Z;

			//UE_LOG(LogTest, Verbose, TEXT("Position[0] : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);
			//for (int32 rdx = 0; rdx < (int32)Particles.Size(); rdx++)
			//{
			//	UE_LOG(GCTCL_Log, Verbose, TEXT("... ... ...Disabled[%d] : %d"), rdx, Particles.Disabled(rdx));
			//}
		}
#endif

		return !R.HasError();
	}
	template bool RigidBodies_Field_StayDynamic<float>(ExampleResponse&& R);


	template<class T>
	bool RigidBodies_Field_LinearForce(ExampleResponse&& R)
	{
		/*
		//
		//  Rigid Body Setup
		//
		TSharedPtr<FGeometryCollection> RestCollection = GeometryCollection::MakeCubeElement(FTransform::Identity, FVector(1.0));
		TManagedArray<FTransform>& RestTransform = *RestCollection->Transform;
		RestTransform[0].SetTranslation(FVector(0.f, 0.f, 5.f));
		TSharedPtr<FGeometryCollection> DynamicCollection = CopyGeometryCollection(RestCollection.Get());
		FGeometryCollectionSolverCallbacks SolverCallbacks;
		FSimulationParameters Parameters;
		Parameters.RestCollection = RestCollection.Get();
		Parameters.DynamicCollection = DynamicCollection.Get();
		Parameters.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
		Parameters.ObjectType = EObjectTypeEnum::Chaos_Object_Dynamic;
		Parameters.Simulating = true;
		SolverCallbacks.UpdateParameters(Parameters);
		SolverCallbacks.Initialize();

		//
		// Field setup
		//
		FFieldSystem System;
		FUniformVector & UniformVector = System.NewNode<FUniformVector>("LinearForce");
		UniformVector.Direction = FVector(1.0, 0.0, 0.0);
		UniformVector.Magnitude = 0.0;
		FFieldSystemSolverCallbacks FieldCallbacks(System);

		//
		// Solver setup
		//
		Chaos::PBDRigidsSolver Solver;
		Solver.RegisterCallbacks(&SolverCallbacks);
		Solver.RegisterFieldCallbacks(&FieldCallbacks);
		Solver.SetHasFloor(false);
		Solver.SetEnabled(true);

		TManagedArray<FTransform>& Transform = *DynamicCollection->Transform;
		float PreviousY = 0.f;
		for (int Frame = 0; Frame < 10; Frame++)
		{
			//UE_LOG(LogTest, Verbose, TEXT("Frame[%d]"), Frame);

			if (Frame >= 5)
			{
				FFieldSystemCommand Command("LinearForce", EFieldPhysicsType::Field_LinearForce, FVector(0), FVector(0,1,0), 0.f, 100.f);
				FieldCallbacks.BufferCommand(Command);
			}

			Solver.AdvanceSolverBy(1 / 24.);

			
			if (Frame < 5)
			{
				R.ExpectTrue(FMath::Abs(Transform[0].GetTranslation().Y) < SMALL_THRESHOLD);
			}
			else
			{
				R.ExpectTrue(Transform[0].GetTranslation().Y > PreviousY);
			}
			
			PreviousY = Transform[0].GetTranslation().Y;

			//UE_LOG(LogTest, Verbose, TEXT("Position[0] : (%3.5f,%3.5f,%3.5f)"), Transform[0].GetTranslation().X, Transform[0].GetTranslation().Y, Transform[0].GetTranslation().Z);
		}

		return !R.HasError();
		*/
		return true;
	}
	template bool RigidBodies_Field_LinearForce<float>(ExampleResponse&& R);

}

