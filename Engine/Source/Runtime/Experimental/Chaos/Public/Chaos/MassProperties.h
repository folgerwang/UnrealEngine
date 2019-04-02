// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Defines.h"
#include "Chaos/Particles.h"
#include "Chaos/TriangleMesh.h"
#include "Containers/ArrayView.h"


#if !COMPILE_WITHOUT_UNREAL_SUPPORT
namespace Chaos
{
	template<class T, int M, int N> class PMatrix;
	template<class T, int D> class TRotation;
	template<class T, int D> class TVector;


	template<class T, int d>
	struct TMassProperties {
		T Volume;
		TVector<T, d> CenterOfMass;
		TRotation<T, d> RotationOfMass;
		PMatrix<T, d, d> InertiaTensor;
	};


	template<class T, int d>
	TRotation<T, d> TransformToLocalSpace(PMatrix<T, d, d>& Inertia);

	
	template<class T, int d>
	TMassProperties<T, d> CalculateMassProperties(
		const TParticles<T, d> & Vertices,
		const TTriangleMesh<T>& Surface,
		const T & Mass);

}
#endif
