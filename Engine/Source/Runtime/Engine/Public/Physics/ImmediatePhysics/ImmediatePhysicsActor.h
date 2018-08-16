// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

#include "ImmediatePhysicsShape.h"

namespace ImmediatePhysics
{

/** Holds geometry data*/
struct FActor
{
	FActor()
		: UserData(nullptr)
	{}

	TArray<FShape> Shapes;
	void* UserData;

#if WITH_PHYSX
	/** Create geometry data for the entity */
	void CreateGeometry(PxRigidActor* RigidActor, const PxTransform& ActorToBodyTM);
	bool AddShape(PxShape* InShape);
#endif

	/** Ensures all the geometry data has been properly freed */
	void TerminateGeometry();
};

}