// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/RigidParticles.h"

using namespace Chaos;

//Note this has to be in the cpp to avoid allocating/freeing across DLLs
template <typename T, int d>
void TRigidParticles<T,d>::CollisionParticlesInitIfNeeded(const int32 Index)
{
	if (MCollisionParticles[Index] == nullptr)
	{
		MCollisionParticles[Index] = MakeUnique<TBVHParticles<T, d>>();
	}
}

template class Chaos::TRigidParticles<float, 3>;