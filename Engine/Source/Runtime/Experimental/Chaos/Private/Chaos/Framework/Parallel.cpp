// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/Framework/Parallel.h"
#include "Async/ParallelFor.h"

using namespace Chaos;

static bool GNoParallelFor = false;

void Chaos::PhysicsParallelFor(int32 InNum, TFunctionRef<void(int32)> InCallable, bool bForceSingleThreaded)
{
	// Passthrough for now, except with global flag to disable parallel
	::ParallelFor(InNum, InCallable, GNoParallelFor || bForceSingleThreaded);
}