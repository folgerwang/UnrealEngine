// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef PHYSICS_INTERFACE_PHYSX
	#define PHYSICS_INTERFACE_PHYSX 0
#endif

#ifndef WITH_APEIRON
	#define WITH_APEIRON 0
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

#elif WITH_APEIRON

	class FPhysInterface_Apeiron;
	class FPhysicsActorReference_Apeiron;
	class FPhysicsConstraintReference_Apeiron;
	class FPhysicsAggregateReference_Apeiron;
	class FPhysicsShapeReference_Apeiron;
	
	typedef FPhysicsActorReference_Apeiron		FPhysicsActorHandle;
	typedef FPhysicsConstraintReference_Apeiron	FPhysicsConstraintHandle;
	typedef FPhysInterface_Apeiron    		    FPhysicsInterface;
	typedef FPhysInterface_Apeiron			    FPhysScene;
	typedef FPhysicsAggregateReference_Apeiron	FPhysicsAggregateHandle;
	typedef FPhysInterface_Apeiron				FPhysicsCommand;
	typedef FPhysicsShapeReference_Apeiron		FPhysicsShapeHandle;
	typedef FPhysicsShapeReference_Apeiron	    FPhysicsGeometryCollection;
	typedef void*                           	FPhysicsMaterialHandle;

#else

static_assert(false, "A physics engine interface must be defined to build");

#endif
