// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Field/FieldSystemSimulationCoreCallbacks.h"

#if INCLUDE_CHAOS
#include "Async/ParallelFor.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "Chaos/MassProperties.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/PBDRigidParticles.h"

//
// This needs to be pushed down to a lower level and shared 
// between the GeometryCollectionSimulationCore and this module.
enum class EObjectTypeEnum : uint8
{
	Chaos_Object_Dynamic UMETA(DisplayName = "Dynamic"),
	Chaos_Object_Kinematic UMETA(DisplayName = "Kinematic"),
	Chaos_Object_Sleeping UMETA(DisplayName = "Sleeping"),
	Chaos_Object_Static UMETA(DisplayName = "Static"),
	//
	Chaos_Max                UMETA(Hidden)
};

DEFINE_LOG_CATEGORY_STATIC(FieldSystemSolverCallbacksLogging, NoLogging, All);

int32 FFieldSystemSolverCallbacks::Invalid = -1;

FFieldSystemSolverCallbacks::FFieldSystemSolverCallbacks(const FFieldSystem & System)
	: FSolverFieldCallbacks(System)
{
}

bool FFieldSystemSolverCallbacks::IsSimulating() const
{
	return true;// Parameters.bIsActive;
}



void FFieldSystemSolverCallbacks::CommandUpdateCallback(FParticlesType& Particles, \
	Chaos::TArrayCollectionArray<FVector> & LinearForce, const float Time)
{
	UE_LOG(FieldSystemSolverCallbacksLogging, Verbose, TEXT("FieldSystemSolverCallbacks::CommandUpdateCallback()"));

	if (Commands.Num())
	{
		Chaos::PBDRigidsSolver* CurrentSolver = GetSolver();
		const Chaos::TArrayCollectionArray<Chaos::ClusterId> & ClusterIDs = CurrentSolver->ClusterIds();

		// @todo: This seems like a waste if we just want to get everything
		TArray<int32> IndicesArray;
		IndicesArray.SetNum(Particles.Size());
		for (int32 i = 0; i < IndicesArray.Num(); ++i)
		{
			IndicesArray[i] = i;
		}
		TArrayView<int32> IndexView(&(IndicesArray[0]), IndicesArray.Num());

		FVector * tptr = &(Particles.X(0));
		TArrayView<FVector> TransformView(tptr, int32(Particles.Size()));

		for (int32 CommandIndex = 0; CommandIndex < Commands.Num(); CommandIndex++)
		{

			// @note(brice) - I'm not really happy with this implementation. The solver needs a 
			//                an object type array that syncs with the collections, that would
			//                avoid all the initialization here. 
			//              - Also, Fields need to know about the particles to operate on. 
			//                This implementation will cause the floor to go active.
			//              - Also, an empty index array should evaluate everything
			if (Commands[CommandIndex].Type == Field_StayDynamic)
			{
				int32 StayDynamicFieldIndex = FieldSystem.TerminalIndex(Commands[CommandIndex].Name);
				if (StayDynamicFieldIndex != INDEX_NONE)
				{
					FFieldContext Context{
						StayDynamicFieldIndex,
						IndexView, // @todo(brice) important: an empty index array should evaluate everything
						TransformView,
						&FieldSystem,
						&Commands[CommandIndex].Position,
						&Commands[CommandIndex].Direction,
						&Commands[CommandIndex].Radius,
						&Commands[CommandIndex].Magnitude
					};

					TArray<int32> DynamicState;
					DynamicState.InsertUninitialized(0, Particles.Size());
					for (int32 i = 0; i < (int32)Particles.Size(); i++)
					{
						DynamicState[i] = (Particles.InvM(i) == 0 || Particles.Sleeping(i)) ? (int)EObjectTypeEnum::Chaos_Object_Kinematic : (int)EObjectTypeEnum::Chaos_Object_Dynamic;
					}
					TArrayView<int32> DynamicStateView(&(DynamicState[0]), DynamicState.Num());

					if (FieldSystem.GetNode(StayDynamicFieldIndex)->Type() == FFieldNode<int32>::StaticType())
					{
						FieldSystem.Evaluate(Context, DynamicStateView);
					}
					else if (FieldSystem.GetNode(StayDynamicFieldIndex)->Type() == FFieldNode<float>::StaticType())
					{
						TArray<float> FloatBuffer;
						FloatBuffer.SetNumUninitialized(DynamicState.Num());
						TArrayView<float> FloatBufferView(&FloatBuffer[0], DynamicState.Num());
						FieldSystem.Evaluate<float>(Context, FloatBufferView);
						for (int i = 0; i < DynamicState.Num(); i++)
						{
							DynamicStateView[i] = (int32)FloatBufferView[i];
						}
					}
					else
					{
						ensureMsgf(false, TEXT("Incorrect type specified in StayKinematic terminal."));
					}

					TSet<int32> ClusterSet;
					bool MadeChanges = false;
					for (int32 RigidBodyIndex = 0; RigidBodyIndex < DynamicState.Num(); RigidBodyIndex++)
					{
						if (DynamicState[RigidBodyIndex] == (int)EObjectTypeEnum::Chaos_Object_Dynamic &&
							Particles.InvM(RigidBodyIndex) == 0.f)
						{
							// Walk up the cluster bodies, activating all the parents.
							if (0 <= RigidBodyIndex && RigidBodyIndex < ClusterIDs.Num())
							{
								int32 ParentID = ClusterIDs[RigidBodyIndex].Id;
								while (ParentID != -1 && !ClusterSet.Contains(ParentID))
								{
									ClusterSet.Add(ParentID);
									DynamicState[ParentID] = (int)EObjectTypeEnum::Chaos_Object_Dynamic;
									ParentID = ClusterIDs[ParentID].Id;
								}
							}
							MadeChanges = true;
						}
					}

					// process levels
					int32 MaxLevel = Commands[CommandIndex].MaxClusterLevel;
					TArray<bool> ProcessList;
					for (int32 Level = 0; Level < MaxLevel && MadeChanges; Level++)
					{
						ProcessList.Init(false, DynamicState.Num());
						for (int32 RigidBodyIndex = 0; RigidBodyIndex < DynamicState.Num(); RigidBodyIndex++)
						{
							if (!Particles.Disabled(RigidBodyIndex) && DynamicState[RigidBodyIndex] == (int)EObjectTypeEnum::Chaos_Object_Dynamic)
							{
								ProcessList[RigidBodyIndex] = true;
							}
						}

						MadeChanges = false;
						for (int32 RigidBodyIndex = 0; RigidBodyIndex < DynamicState.Num(); RigidBodyIndex++)
						{
							if (ProcessList[RigidBodyIndex])
							{
								Particles.SetSleeping(RigidBodyIndex, false);
								if (ClusterSet.Contains(RigidBodyIndex))
								{
									for (int32 ChildIndex : CurrentSolver->DeactivateClusterParticle((uint32)RigidBodyIndex))
									{
										Particles.InvM(ChildIndex) = 1.f / Particles.M(ChildIndex);
										Particles.InvI(ChildIndex) = Chaos::PMatrix<float, 3, 3>(
											1.f / Particles.I(ChildIndex).M[0][0], 0.f, 0.f,
											0.f, 1.f / Particles.I(ChildIndex).M[1][1], 0.f,
											0.f, 0.f, 1.f / Particles.I(ChildIndex).M[2][2]);
									}
									MadeChanges = true;
								}
								else if (Particles.InvM(RigidBodyIndex) == 0.0 && KINDA_SMALL_NUMBER < Particles.M(RigidBodyIndex))
								{
									Particles.InvM(RigidBodyIndex) = 1.f / Particles.M(RigidBodyIndex);
									Particles.InvI(RigidBodyIndex) = Chaos::PMatrix<float, 3, 3>(
										1.f / Particles.I(RigidBodyIndex).M[0][0], 0.f, 0.f,
										0.f, 1.f / Particles.I(RigidBodyIndex).M[1][1], 0.f,
										0.f, 0.f, 1.f / Particles.I(RigidBodyIndex).M[2][2]);
									MadeChanges = true;
								}
							}
						}
					}
				}
			}
			
			if (Commands[CommandIndex].Type == Field_LinearForce )
			{
				int32 ForceFieldIndex = FieldSystem.TerminalIndex(Commands[CommandIndex].Name);
				if (ForceFieldIndex != INDEX_NONE)
				{
					FFieldContext Context{
					ForceFieldIndex,
					IndexView,
					TransformView,
					&FieldSystem,
					&Commands[CommandIndex].Position,
					&Commands[CommandIndex].Direction,
					&Commands[CommandIndex].Radius,
					&Commands[CommandIndex].Magnitude
					};
					TArrayView<FVector> ForceView(&(LinearForce[0]), LinearForce.Num());
					if (FieldSystem.GetNode(ForceFieldIndex)->Type() == FFieldNode<FVector>::StaticType())
					{
						FieldSystem.Evaluate(Context, ForceView);
					}
				}
			}
		}

		Commands.Reset(0);
	}
}

#else
FFieldSystemSolverCallbacks::FFieldSystemSolverCallbacks()
{}
#endif
