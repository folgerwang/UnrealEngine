// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: UGeometryCollection methods.
=============================================================================*/

#include "Field/FieldSystemCoreAlgo.h"
#include "Field/FieldSystemNodes.h"
#include "Async/ParallelFor.h"

DEFINE_LOG_CATEGORY_STATIC(FSCA_Log, Log, All);

namespace FieldSystemAlgo
{
	void InitDefaultFieldData(FFieldSystem& System)
	{
		System.Reset();

		// RadialIntMask
		FRadialIntMask & RadialMask = System.NewNode<FRadialIntMask>("RadialIntMask");
		RadialMask.InteriorValue = 0;
		RadialMask.ExteriorValue = 1;
		RadialMask.SetMaskCondition = ESetMaskConditionType::Field_Set_IFF_NOT_Interior;

		// RadialFalloff
		FRadialFalloff & RadialFalloff = System.NewNode<FRadialFalloff>("RadialFalloff");
		RadialFalloff.Position = FVector(0.f);
		RadialFalloff.Radius = 0.f;
		RadialFalloff.Magnitude = 0.f;

		// Uniform Vector
		FUniformVector & UniformVector = System.NewNode<FUniformVector>("UniformVector");
		UniformVector.Direction = FVector(0.f);
		UniformVector.Magnitude = 0.f;

		// Radial Vector
		FRadialVector & RadialVector = System.NewNode<FRadialVector>("RadialVector");
		RadialVector.Position = FVector(0.f);
		RadialVector.Magnitude = 0.f;

		// Radial Vector Falloff
		FSumVector & SumVector = System.NewNode<FSumVector>("RadialVectorFalloff");
		SumVector.Scalar = RadialFalloff.GetTerminalID();
		SumVector.VectorLeft = RadialVector.GetTerminalID();
		SumVector.VectorRight = FFieldNodeBase::Invalid;
		SumVector.Operation = EFieldOperationType::Field_Multiply;

		// Uniform Vector Falloff
		FSumVector & UniformSumVector = System.NewNode<FSumVector>("UniformVectorFalloff");
		UniformSumVector.Scalar = RadialFalloff.GetTerminalID();
		UniformSumVector.VectorLeft = UniformVector.GetTerminalID();
		UniformSumVector.VectorRight = FFieldNodeBase::Invalid;
		UniformSumVector.Operation = EFieldOperationType::Field_Multiply;

	}
}
