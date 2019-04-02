// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef PHYSICS_INTERFACE_PHYSX
	#define PHYSICS_INTERFACE_PHYSX 0
#endif

#ifndef WITH_CHAOS
	#define WITH_CHAOS 0
#endif

#ifndef WITH_IMMEDIATE_PHYSX
	#define WITH_IMMEDIATE_PHYSX 0
#endif

#ifndef PHYSICS_INTERFACE_LLIMMEDIATE
	#define PHYSICS_INTERFACE_LLIMMEDIATE 0
#endif

#if PHYSICS_INTERFACE_PHYSX

	#if WITH_IMMEDIATE_PHYSX
	
		struct FPhysicsActorReference_ImmediatePhysX;
		struct FPhysicsConstraintReference_ImmediatePhysX;
		struct FPhysicsInterface_ImmediatePhysX;
		class FPhysScene_ImmediatePhysX;
		struct FPhysicsAggregateReference_ImmediatePhysX;
		struct FPhysicsCommand_ImmediatePhysX;
		struct FPhysicsShapeReference_ImmediatePhysX;
		struct FPhysicsMaterialReference_ImmediatePhysX;
		struct FPhysicsGeometryCollection_ImmediatePhysX;
		
		typedef FPhysicsActorReference_ImmediatePhysX		FPhysicsActorHandle;
		typedef FPhysicsConstraintReference_ImmediatePhysX	FPhysicsConstraintHandle;
		typedef FPhysicsInterface_ImmediatePhysX			FPhysicsInterface;
		typedef FPhysScene_ImmediatePhysX					FPhysScene;
		typedef FPhysicsAggregateReference_ImmediatePhysX	FPhysicsAggregateHandle;
		typedef FPhysicsCommand_ImmediatePhysX				FPhysicsCommand;
		typedef FPhysicsShapeReference_ImmediatePhysX		FPhysicsShapeHandle;
		typedef FPhysicsGeometryCollection_ImmediatePhysX   FPhysicsGeometryCollection;
		typedef FPhysicsMaterialReference_ImmediatePhysX    FPhysicsMaterialHandle;
	
	#else
	
		struct FPhysicsActorHandle_PhysX;
		struct FPhysicsConstraintHandle_PhysX;
		struct FPhysicsInterface_PhysX;
		class FPhysScene_PhysX;
		struct FPhysicsAggregateHandle_PhysX;
		struct FPhysicsCommand_PhysX;
		struct FPhysicsShapeHandle_PhysX;
		struct FPhysicsGeometryCollection_PhysX;
		struct FPhysicsMaterialHandle_PhysX;
		
		typedef FPhysicsActorHandle_PhysX			FPhysicsActorHandle;
		typedef FPhysicsConstraintHandle_PhysX		FPhysicsConstraintHandle;
		typedef FPhysicsInterface_PhysX				FPhysicsInterface;
		typedef FPhysScene_PhysX					FPhysScene;
		typedef FPhysicsAggregateHandle_PhysX		FPhysicsAggregateHandle;
		typedef FPhysicsCommand_PhysX				FPhysicsCommand;
		typedef FPhysicsShapeHandle_PhysX			FPhysicsShapeHandle;
		typedef FPhysicsGeometryCollection_PhysX	FPhysicsGeometryCollection;
		typedef FPhysicsMaterialHandle_PhysX		FPhysicsMaterialHandle;

		namespace physx
		{
			struct PxLocationHit;
			struct PxSweepHit;
			struct PxRaycastHit;
			struct PxOverlapHit;
			struct PxQueryHit;

			class PxTransform;
			class PxShape;
			class PxGeometry;
			class PxCapsuleGeometry;
			class PxMaterial;
			class PxRigidActor;

			template<typename T>
			struct PxHitBuffer;
		}

		template<typename T>
		class FSingleHitBuffer;

		using FHitLocation = physx::PxLocationHit;
		using FHitSweep = physx::PxSweepHit;
		using FHitRaycast = physx::PxRaycastHit;
		using FHitOverlap = physx::PxOverlapHit;
		using FPhysicsQueryHit = physx::PxQueryHit;

		using FPhysicsTransform = physx::PxTransform;

		using FPhysicsShape = physx::PxShape;
		using FPhysicsGeometry = physx::PxGeometry;
		using FPhysicsCapsuleGeometry = physx::PxCapsuleGeometry;
		using FPhysicsMaterial = physx::PxMaterial;
		using FPhysicsActor = physx::PxRigidActor;
		
		using FPhysicsSweepBuffer = FSingleHitBuffer<physx::PxSweepHit>;
		using FPhysicsRaycastBuffer = FSingleHitBuffer<physx::PxRaycastHit>;
	
	#endif

#elif PHYSICS_INTERFACE_LLIMMEDIATE

	struct FPhysicsActorHandle_LLImmediate;
	struct FPhysicsAggregateHandle_LLImmediate;
	struct FPhysicsConstraintHandle_LLImmediate;
	struct FPhysicsShapeHandle_LLImmediate;
	struct FPhysicsCommand_LLImmediate;
	struct FPhysicsGeometryCollection_LLImmediate;
	struct FPhysicsMaterialHandle_LLImmediate;
	class FPhysInterface_LLImmediate;

	typedef FPhysicsActorHandle_LLImmediate           FPhysicsActorHandle;
	typedef FPhysicsConstraintHandle_LLImmediate      FPhysicsConstraintHandle;
	typedef FPhysInterface_LLImmediate                FPhysicsInterface;
	typedef FPhysInterface_LLImmediate                FPhysScene;
	typedef FPhysicsAggregateHandle_LLImmediate       FPhysicsAggregateHandle;
	typedef FPhysicsCommand_LLImmediate               FPhysicsCommand;
	typedef FPhysicsShapeHandle_LLImmediate           FPhysicsShapeHandle;
	typedef FPhysicsGeometryCollection_LLImmediate    FPhysicsGeometryCollection;
	typedef FPhysicsMaterialHandle_LLImmediate        FPhysicsMaterialHandle;

	namespace physx
	{
		//struct PxLocationHit;
		//struct PxSweepHit;
		//struct PxRaycastHit;
		//struct PxOverlapHit;
		//
		//class PxShape;
		class PxGeometry;
		class PxCapsuleGeometry;
		//class PxMaterial;
		//class PxRigidActor;
		//
		//template<typename T>
		//struct PxHitBuffer;

		//struct PxSweepBuffer;
		//struct PxRaycastBuffer;
	}

	// Temporary dummy types until SQ implemented
	struct FPhysTypeDummy {};
	struct FPhysActorDummy {};
	template<typename T>
	struct FCallbackDummy;

	using FHitLocation = FPhysTypeDummy;
	using FHitSweep = FPhysTypeDummy;
	using FHitRaycast = FPhysTypeDummy;
	using FHitOverlap = FPhysTypeDummy;
	using FPhysicsQueryHit = FPhysTypeDummy;

	using FPhysicsTransform = FTransform;

	using FPhysicsShape = FPhysTypeDummy;
	using FPhysicsGeometry = physx::PxGeometry;
	using FPhysicsCapsuleGeometry = physx::PxCapsuleGeometry;
	using FPhysicsMaterial = FPhysTypeDummy;
	using FPhysicsActor = FPhysActorDummy;

	using FPhysicsSweepBuffer = FCallbackDummy<FHitSweep>;
	using FPhysicsRaycastBuffer = FCallbackDummy<FHitRaycast>;

#elif WITH_CHAOS

	class FPhysInterface_Chaos;
	class FPhysicsActorReference_Chaos;
	class FPhysicsConstraintReference_Chaos;
	class FPhysicsAggregateReference_Chaos;
	class FPhysicsShapeReference_Chaos;
	
	typedef FPhysicsActorReference_Chaos		FPhysicsActorHandle;
	typedef FPhysicsConstraintReference_Chaos	FPhysicsConstraintHandle;
	typedef FPhysInterface_Chaos    		    FPhysicsInterface;
	typedef FPhysInterface_Chaos			    FPhysScene;
	typedef FPhysicsAggregateReference_Chaos	FPhysicsAggregateHandle;
	typedef FPhysInterface_Chaos				FPhysicsCommand;
	typedef FPhysicsShapeReference_Chaos		FPhysicsShapeHandle;
	typedef FPhysicsShapeReference_Chaos	    FPhysicsGeometryCollection;
	typedef void*                           	FPhysicsMaterialHandle;

	namespace Chaos
	{
		template<class T, int d>
		class TImplicitObject;

		template<class T>
		class TCapsule;
	}

	// Temporary dummy types until SQ implemented
	struct FPhysTypeDummy {};
	struct FPhysActorDummy {};
	template<typename T>
	struct FCallbackDummy;

	using FHitLocation = FPhysTypeDummy;
	using FHitSweep = FPhysTypeDummy;
	using FHitRaycast = FPhysTypeDummy;
	using FHitOverlap = FPhysTypeDummy;
	using FPhysicsQueryHit = FPhysTypeDummy;

	using FPhysicsTransform = FTransform;

	using FPhysicsShape = Chaos::TImplicitObject<float, 3>;
	using FPhysicsGeometry = Chaos::TImplicitObject<float, 3>;
	using FPhysicsCapsuleGeometry = Chaos::TCapsule<float>;
	using FPhysicsMaterial = FPhysTypeDummy;
	using FPhysicsActor = FPhysActorDummy;

	using FPhysicsSweepBuffer = FCallbackDummy<FHitSweep>;
	using FPhysicsRaycastBuffer = FCallbackDummy<FHitRaycast>;

#else

static_assert(false, "A physics engine interface must be defined to build");

#endif
