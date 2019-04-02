// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "GeometryCollection/GeometryCollectionPhysicsFieldCommands.h"

#if INCLUDE_CHAOS

#include "PBDRigidsSolver.h"
#include "Containers/ArrayView.h"
#include "GeometryCollection/ManagedArray.h"
#include "Field/FieldSystemTypes.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"

DEFINE_LOG_CATEGORY_STATIC(FSCA_Log, Log, All);

namespace PhysicsFieldCommand
{
	void ApplyStayDynamicField(
		const FFieldSystemCommand & Command
		, TSharedPtr<TManagedArray<int32> > & DynamicStateArray
		, TArray<int32> * RigidBodyIdArray
		, FSolverCallbacks::FParticlesType& Particles
		, const FFieldSystem* FieldSystem, int32 StayDynamicFieldIndex)
	{
		TManagedArray<int32> & DynamicState = *DynamicStateArray;

		// Move to a Terminal Evaluator in Field. 
		if (FieldSystem != nullptr && StayDynamicFieldIndex != FFieldNodeBase::Invalid)
		{

			int32 * bptr = &(RigidBodyIdArray->operator[](0));
			int32 bsz = RigidBodyIdArray->Num();
			TArrayView<int32> IndexView(bptr,bsz);
			FVector * tptr = &(Particles.X(0));
			TArrayView<FVector> TransformView(tptr, int32(Particles.Size()));

			FFieldContext Context{
				StayDynamicFieldIndex,
				IndexView,
				TransformView,
				FieldSystem
			};
			TArrayView<int32> DynamicStateView(&(DynamicState[0]), DynamicState.Num());

			if (FieldSystem->GetNode(StayDynamicFieldIndex)->Type() == FFieldNode<int32>::StaticType())
			{
				FieldSystem->Evaluate(Context, DynamicStateView);
			}
			else if (FieldSystem->GetNode(StayDynamicFieldIndex)->Type() == FFieldNode<float>::StaticType())
			{
				TArray<float> FloatBuffer;
				FloatBuffer.SetNumUninitialized(DynamicState.Num());
				TArrayView<float> FloatBufferView(&FloatBuffer[0], DynamicState.Num());
				FieldSystem->Evaluate<float>(Context, FloatBufferView);
				for (int i = 0; i < DynamicState.Num(); i++)
				{
					DynamicStateView[i] = (int32)FloatBufferView[i];
				}
			}
			else
			{
				ensureMsgf(false, TEXT("Incorrect type specified in StayKinematic terminal."));
			}
		}
	}
}
#endif