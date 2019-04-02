// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExample.h"
#include "GeometryCollection/GeometryCollectionExampleResponse.h"

#include "GeometryCollection/GeometryCollectionExampleClustering.h"
#include "GeometryCollection/GeometryCollectionExampleCreation.h"
#include "GeometryCollection/GeometryCollectionExampleDecimation.h"
#include "GeometryCollection/GeometryCollectionExampleFields.h"
#include "GeometryCollection/GeometryCollectionExampleMatrices.h"
#include "GeometryCollection/GeometryCollectionExampleSimulation.h"
#include "GeometryCollection/GeometryCollectionExampleProximity.h"
#include "GeometryCollection/GeometryCollectionExampleClean.h"
#include "GeometryCollection/GeometryCollectionExampleSpatialHash.h"

namespace GeometryCollectionExample
{
#define RUN_EXAMPLE(X) X<float>(RESPONSE());

	template<class RESPONSE>
	void ExecuteExamples()
	{
		RUN_EXAMPLE(BasicGlobalMatrices);
		RUN_EXAMPLE(TransformMatrixElement);
		RUN_EXAMPLE(ReparentingMatrices);
		RUN_EXAMPLE(ParentTransformTest);
		RUN_EXAMPLE(ReindexMaterialsTest);
		RUN_EXAMPLE(Creation);
		RUN_EXAMPLE(ContiguousElementsTest);
		RUN_EXAMPLE(DeleteFromEnd);
		RUN_EXAMPLE(DeleteFromStart);
		RUN_EXAMPLE(DeleteFromMiddle);
		RUN_EXAMPLE(DeleteBranch);
		RUN_EXAMPLE(DeleteRootLeafMiddle);
		RUN_EXAMPLE(DeleteEverything);
		RUN_EXAMPLE(Fields_RadialIntMask);
		RUN_EXAMPLE(Fields_RadialFalloff);
		RUN_EXAMPLE(Fields_UniformVector);
		RUN_EXAMPLE(Fields_RaidalVector);
		RUN_EXAMPLE(Fields_SumVectorFullMult);
		RUN_EXAMPLE(Fields_SumVectorFullDiv);
		RUN_EXAMPLE(Fields_SumVectorFullAdd);
		RUN_EXAMPLE(Fields_SumVectorFullSub);
		RUN_EXAMPLE(Fields_SumVectorLeftSide);
		RUN_EXAMPLE(Fields_SumVectorRightSide);
		RUN_EXAMPLE(Fields_SumScalar);
		RUN_EXAMPLE(Fields_SumScalarRightSide);
		RUN_EXAMPLE(Fields_SumScalarLeftSide);
		RUN_EXAMPLE(Fields_ContextOverrides);
		RUN_EXAMPLE(Fields_DefaultRadialFalloff);
		RUN_EXAMPLE(RigidBodiesFallingUnderGravity);
		RUN_EXAMPLE(RigidBodiesCollidingWithSolverFloor);
		RUN_EXAMPLE(RigidBodiesSingleSphereIntersectingWithSolverFloor);
		RUN_EXAMPLE(RigidBodiesSingleSphereCollidingWithSolverFloor);
		RUN_EXAMPLE(RigidBodiesKinematic);
		RUN_EXAMPLE(RigidBodiesKinematicFieldActivation);
		RUN_EXAMPLE(RigidBodiesSleepingActivation);
		RUN_EXAMPLE(RigidBodiesInitialLinearVelocity);
		RUN_EXAMPLE(RigidBodies_ClusterTest_SingleLevelNonBreaking);
		RUN_EXAMPLE(RigidBodies_ClusterTest_DeactivateClusterParticle);
		RUN_EXAMPLE(RigidBodies_ClusterTest_SingleLevelBreaking);
		RUN_EXAMPLE(RigidBodies_ClusterTest_NestedCluster);
		RUN_EXAMPLE(RigidBodies_ClusterTest_NestedCluster_MultiStrain);
		RUN_EXAMPLE(RigidBodies_ClusterTest_NestedCluster_Halt);
		RUN_EXAMPLE(RigidBodies_Field_StayDynamic);
		RUN_EXAMPLE(RigidBodies_Field_LinearForce);
		RUN_EXAMPLE(BuildProximity);
		RUN_EXAMPLE(GeometryDeleteFromStart);
		RUN_EXAMPLE(GeometryDeleteFromEnd);
		RUN_EXAMPLE(GeometryDeleteFromMiddle);
		RUN_EXAMPLE(GeometryDeleteMultipleFromMiddle);
		RUN_EXAMPLE(GeometryDeleteRandom);
		RUN_EXAMPLE(GeometryDeleteRandom2);
		RUN_EXAMPLE(GeometryDeleteAll);
		RUN_EXAMPLE(TestFracturedGeometry);
		RUN_EXAMPLE(TestDeleteCoincidentVertices);
		RUN_EXAMPLE(TestDeleteCoincidentVertices2);
		RUN_EXAMPLE(TestDeleteZeroAreaFaces);
		RUN_EXAMPLE(TestDeleteHiddenFaces);
		RUN_EXAMPLE(GetClosestPointsTest1);
		RUN_EXAMPLE(GetClosestPointsTest2);
		RUN_EXAMPLE(GetClosestPointsTest3);
		RUN_EXAMPLE(GetClosestPointTest);
		RUN_EXAMPLE(HashTableUpdateTest);
		RUN_EXAMPLE(HashTablePressureTest);
		RUN_EXAMPLE(TestGeometryDecimation);
		
	}
	template void ExecuteExamples<ExampleResponse>();
}
