// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDCollisionConstraint.h"

namespace Chaos
{
	void CHAOS_API ComputeHashTable(const TArray<Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint>& ConstraintsArray,
									const FBox& BoundingBox, TMultiMap<int32, int32>& HashTableMap, const float SpatialHashRadius);
}
