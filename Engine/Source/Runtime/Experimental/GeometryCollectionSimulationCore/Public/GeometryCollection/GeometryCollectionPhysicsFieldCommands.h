// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#if INCLUDE_CHAOS

#include "PBDRigidsSolver.h"
#include "GeometryCollection/ManagedArray.h"
#include "Field/FieldSystemTypes.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"

namespace PhysicsFieldCommand
{

	void ApplyStayDynamicField(
		const FFieldSystemCommand & Command
		, TSharedPtr<TManagedArray<int32> > & DynamicStateArray
		, const TArray<int32> * RigidBodyIdArray
		, FSolverCallbacks::FParticlesType& Particles
		, const FFieldSystem* FieldSystem, int32 StayDynamicFieldIndex);

}

#endif