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

#include "Physics/PhysicsInterfacePhysX.h"
#include "Physics/PhysScene_PhysX.h"
#include "Physics/Experimental/PhysScene_ImmediatePhysX.h"
#include "Physics/Experimental/PhysicsInterfaceImmediatePhysX.h"
	
	#if WITH_IMMEDIATE_PHYSX
	
		typedef FPhysicsActorReference_ImmediatePhysX		FPhysicsActorHandle;
		typedef FPhysicsConstraintReference_ImmediatePhysX	FPhysicsConstraintHandle;
		typedef FPhysicsInterface_ImmediatePhysX			FPhysicsInterface;
		typedef FPhysScene_ImmediatePhysX					FPhysScene;
		typedef FPhysicsAggregateReference_ImmediatePhysX	FPhysicsAggregateHandle;
		typedef FPhysicsCommand_ImmediatePhysX				FPhysicsCommand;
		typedef FPhysicsShapeReference_ImmediatePhysX	    FPhysicsShapeHandle;
		typedef FPhysicsGeometryCollection_ImmediatePhysX   FPhysicsGeometryCollection;
		typedef FPhysicsMaterialReference_ImmediatePhysX    FPhysicsMaterialHandle;
	
	#else // PhysX base interface as a fallback

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
	
	#include "Physics/Experimental/PhysicsInterfaceLLImmediate.h"

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

	#include "Physics/Experimental/PhysInterface_Apeiron.h"

	typedef FPhysicsActorReference_Apeiron	    FPhysicsActorHandle;
	typedef FPhysicsConstraintReference_Apeiron FPhysicsConstraintHandle;
	typedef FPhysInterface_Apeiron    		    FPhysicsInterface;
	typedef FPhysInterface_Apeiron			    FPhysScene;
	typedef FPhysicsAggregateReference_Apeiron  FPhysicsAggregateHandle;
	typedef FPhysicsShapeReference_Apeiron		FPhysicsShapeHandle;
	typedef FPhysicsShapeReference_Apeiron	    FPhysicsGeometryCollection;
	typedef void*                           	FPhysicsMaterialHandle;

#else

	static_assert(false, "A physics engine interface must be defined to build");

#endif
