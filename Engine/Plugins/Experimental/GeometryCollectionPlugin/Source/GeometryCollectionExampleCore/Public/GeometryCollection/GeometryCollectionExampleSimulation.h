// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollection/GeometryCollectionExample.h"
#include "GeometryCollection/GeometryCollectionExampleResponse.h"

namespace GeometryCollectionExample
{	
	
	template<class T>
	bool RigidBodiesFallingUnderGravity(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesCollidingWithSolverFloor(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesSingleSphereCollidingWithSolverFloor(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesSingleSphereIntersectingWithSolverFloor(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesKinematic(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesKinematicFieldActivation(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesSleepingActivation(ExampleResponse&& R);

	template<class T>
	bool RigidBodiesInitialLinearVelocity(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_Field_StayDynamic(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_Field_LinearForce(ExampleResponse&& R);
	
}
