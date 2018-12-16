// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollection/GeometryCollectionExample.h"
#include "GeometryCollection/GeometryCollectionExampleResponse.h"
namespace GeometryCollectionExample
{

	template<class T>
	bool RigidBodies_ClusterTest_SingleLevelNonBreaking(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_ClusterTest_DeactivateClusterParticle(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_ClusterTest_SingleLevelBreaking(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_ClusterTest_NestedCluster(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_ClusterTest_NestedCluster_MultiStrain(ExampleResponse&& R);

	template<class T>
	bool RigidBodies_ClusterTest_NestedCluster_Halt(ExampleResponse&& R);
	
}
