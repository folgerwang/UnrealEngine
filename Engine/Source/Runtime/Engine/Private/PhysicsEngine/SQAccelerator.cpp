// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Physics/SQAccelerator.h"
#include "CollisionQueryFilterCallback.h"

#if PHYSICS_INTERFACE_PHYSX
#include "Collision/SceneQueryPhysXImp.h"	//todo: use nice platform wrapper
#include "PhysXInterfaceWrapper.h"
#elif PHYSICS_INTERFACE_LLIMMEDIATE
#include "Collision/Experimental/SceneQueryLLImmediateImp.h"
#endif

FSQAcceleratorEntry* FSQAccelerator::AddEntry(void* Payload)
{
	if (Nodes.Num() == 0)
	{
		Nodes.Add(new FSQNode());
	}

	FSQAcceleratorEntry* NewEntry = new FSQAcceleratorEntry(Payload);
	Nodes[0]->Entries.Add(NewEntry);
	return NewEntry;
}

#if 0
void FSQAccelerator::UpdateBounds(FSQAcceleratorEntry* Entry, const FBoxSphereBounds& NewBounds)
{
	Entry->Bounds = NewBounds;
}
#endif

void FSQAccelerator::RemoveEntry(FSQAcceleratorEntry* Entry)
{
	if (Nodes.Num())
	{
		Nodes[0]->Entries.RemoveSingleSwap(Entry);
	}
	delete Entry;
}

void FSQAccelerator::GetNodes(TArray<const FSQNode*>& NodesFound) const
{
	NodesFound.Reset(Nodes.Num());
	for (const FSQNode* Node : Nodes)
	{
		NodesFound.Add(Node);
	}
}

FSQAccelerator::~FSQAccelerator()
{
	for (FSQNode* Node : Nodes)
	{
		for (FSQAcceleratorEntry* Entry : Node->Entries)
		{
			delete Entry;
		}

		delete Node;
	}
}

template <typename HitType>
class FSQOverlapBuffer
{
public:

	FSQOverlapBuffer(int32 InMaxNumOverlaps)
		: MaxNumOverlaps(InMaxNumOverlaps)
	{
	}

	bool Insert(const HitType& Hit)
	{
		//maybe prioritize based on penetration depth?
		if (Overlapping.Num() < MaxNumOverlaps)
		{
			Overlapping.Add(Hit);
		}

		return Overlapping.Num() < MaxNumOverlaps;
	}

private:
	TArray<HitType>	Overlapping;	//todo(ocohen): use a TInlineArray + compatible bytes to avoid default constructor and allocations
	int32 MaxNumOverlaps;
};

template <typename HitType>
class FSQTraceBuffer
{
public:

	FSQTraceBuffer(float InDeltaMag, int32 InMaxNumOverlaps)
		: DeltaMag(InDeltaMag)
		, MaxNumOverlaps(InMaxNumOverlaps)
	{
	}

	bool Insert(const HitType& Hit, bool bBlocking)
	{
		if (GetDistance(Hit) < DeltaMag)
		{
			if (bBlocking)
			{
				BlockingHit = Hit;
				bHasBlocking = true;
				DeltaMag = GetDistance(Hit);
			}
			else
			{
				//todo(ocohen):use a priority queue or something more sensible than just a full sort every time. Avoid add then remove if possible
				Overlapping.Add(Hit);
				Overlapping.Sort();
				if (Overlapping.Num() > MaxNumOverlaps)
				{
					Overlapping.SetNum(MaxNumOverlaps);
				}
			}
		}

		return true;
	}

	float GetBlockingDistance() const { return DeltaMag; }
	float GetOverlapingDistance() const { return DeltaMag;  /*todo(ocohen): if we keep overlaps sorted we could make this tighter and avoid memory growth.*/ }

private:
	HitType BlockingHit;
	TArray<HitType>	Overlapping;	//todo(ocohen): use a TInlineArray + compatible bytes to avoid default constructor and allocations
	int32 MaxNumOverlaps;
	bool bHasBlocking;
	float DeltaMag;
};

void FSQAccelerator::Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
#if WITH_PHYSX
	FPhysicsRaycastInputAdapater Inputs(Start, Dir, OutputFlags);

	for (const FSQNode* Node : Nodes)
	{
		//in theory we'd do some checks against max block and max overlap distance to avoid some of these nodes
		for (const FSQAcceleratorEntry* Entry : Node->Entries)
		{
			if (FPhysicsActor* RigidActor = (FPhysicsActor*)Entry->GetPayload())
			{
				const FPhysicsTransform ActorTM = GetGlobalPose(*RigidActor);
				const uint32 NumShapes = GetNumShapes(*RigidActor);
				TArray<FPhysicsShape*> Shapes;
				Shapes.AddZeroed(NumShapes);
				GetShapes(*RigidActor, Shapes.GetData(), NumShapes);
				FHitRaycast Hit;
				for (FPhysicsShape* Shape : Shapes)
				{
					const FCollisionFilterData ShapeFilterData = GetQueryFilterData(*Shape);
					ECollisionQueryHitType FilterType = QueryFlags & EQueryFlags::PreFilter ? QueryCallback.PreFilter(QueryFilter, *Shape, *RigidActor) : ECollisionQueryHitType::Block;	//todo(ocohen): we always use preFilter, should we add a cheaper test?
					if (FilterType != ECollisionQueryHitType::None)
					{
						if (LowLevelRaycastImp(Inputs.Start, Inputs.Dir, GetCurrentBlockTraceDistance(HitBuffer), *Shape, ActorTM, Inputs.OutputFlags, Hit))
						{
							SetActor(Hit, RigidActor);
							SetShape(Hit, Shape);
							FilterType = QueryFlags & EQueryFlags::PostFilter ? QueryCallback.PostFilter(QueryFilter, Hit) : FilterType;
							if (FilterType != ECollisionQueryHitType::None)
							{
								Insert(HitBuffer, Hit, FilterType == ECollisionQueryHitType::Block || (QueryFlags & EQueryFlags::AnyHit));
							}
						}
					}
				}
			}
		}	
	}
#endif
}

void FSQAccelerator::Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
#if WITH_PHYSX
	FPhysicsSweepInputAdapater Inputs(StartTM, Dir, OutputFlags);
	for (const FSQNode* Node : Nodes)
	{
		for (const FSQAcceleratorEntry* Entry : Node->Entries)
		{
			if (FPhysicsActor* RigidActor = (FPhysicsActor*)Entry->GetPayload())
			{
				const FPhysicsTransform ActorTM = GetGlobalPose(*RigidActor);
				const uint32 NumShapes = GetNumShapes(*RigidActor);
				TArray<FPhysicsShape*> Shapes;
				Shapes.AddZeroed(NumShapes);
				GetShapes(*RigidActor, Shapes.GetData(), NumShapes);
				FHitSweep Hit;
				for (FPhysicsShape* Shape : Shapes)
				{
					const FCollisionFilterData ShapeFilterData = GetQueryFilterData(*Shape);
					ECollisionQueryHitType FilterType = QueryFlags & EQueryFlags::PreFilter ? QueryCallback.PreFilter(QueryFilter, *Shape, *RigidActor) : ECollisionQueryHitType::Block;	//todo(ocohen): we always use preFilter, should we add a cheaper test?
					if (FilterType != ECollisionQueryHitType::None)
					{
						if (LowLevelSweepImp(Inputs.StartTM, Inputs.Dir, GetCurrentBlockTraceDistance(HitBuffer), QueryGeom, *Shape, ActorTM, Inputs.OutputFlags, Hit))
						{
							SetActor(Hit, RigidActor);
							SetShape(Hit, Shape);
							FilterType = QueryFlags & EQueryFlags::PostFilter ? QueryCallback.PostFilter(QueryFilter, Hit) : FilterType;
							if (FilterType != ECollisionQueryHitType::None)
							{
								Insert(HitBuffer, Hit, FilterType == ECollisionQueryHitType::Block || (QueryFlags & EQueryFlags::AnyHit));
							}
						}
					}

				}
			}
		}
	}
#endif
}

void FSQAccelerator::Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
#if WITH_PHYSX
	FPhysicsOverlapInputAdapater Inputs(GeomPose);
	for (const FSQNode* Node : Nodes)
	{
		for (const FSQAcceleratorEntry* Entry : Node->Entries)
		{
			if (FPhysicsActor* RigidActor = (FPhysicsActor*)Entry->GetPayload())
			{
				const FPhysicsTransform ActorTM = GetGlobalPose(*RigidActor);
				const uint32 NumShapes = GetNumShapes(*RigidActor);
				TArray<FPhysicsShape*> Shapes;
				Shapes.AddZeroed(NumShapes);
				GetShapes(*RigidActor, Shapes.GetData(), NumShapes);
				FHitOverlap Overlap;
				for (FPhysicsShape* Shape : Shapes)
				{
					const FCollisionFilterData ShapeFilterData = GetQueryFilterData(*Shape);
					ECollisionQueryHitType FilterType = QueryFlags & EQueryFlags::PreFilter ? QueryCallback.PreFilter(QueryFilter, *Shape, *RigidActor) : ECollisionQueryHitType::Block;	//todo(ocohen): we always use preFilter, should we add a cheaper test?
					if (FilterType != ECollisionQueryHitType::None)
					{
						if (LowLevelOverlapImp(Inputs.GeomPose, QueryGeom, *Shape, ActorTM, Overlap))
						{
							SetActor(Overlap, RigidActor);
							SetShape(Overlap, Shape);
							FilterType = QueryFlags & EQueryFlags::PostFilter ? QueryCallback.PostFilter(QueryFilter, Overlap) : FilterType;
							if (FilterType != ECollisionQueryHitType::None)
							{
								if (!InsertOverlap(HitBuffer, Overlap))
								{
									return;
								}
							}
						}
					}
				}
			}
		}
	}
#endif
}

void FSQAcceleratorUnion::Raycast(const FVector& Start, const FVector& Dir, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
	for (const ISQAccelerator* Accelerator : Accelerators)
	{
		Accelerator->Raycast(Start, Dir, HitBuffer, OutputFlags, QueryFlags, QueryFilter, QueryCallback);
	}
}

void FSQAcceleratorUnion::Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
	for (const ISQAccelerator* Accelerator : Accelerators)
	{
		Accelerator->Sweep(QueryGeom, StartTM, Dir, HitBuffer, OutputFlags, QueryFlags, QueryFilter, QueryCallback);
	}
}

void FSQAcceleratorUnion::Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& QueryFilter, FCollisionQueryFilterCallback& QueryCallback) const
{
	for (const ISQAccelerator* Accelerator : Accelerators)
	{
		Accelerator->Overlap(QueryGeom, GeomPose, HitBuffer, QueryFlags, QueryFilter, QueryCallback);
	}
}

void FSQAcceleratorUnion::AddSQAccelerator(ISQAccelerator* InAccelerator)
{
	Accelerators.AddUnique(InAccelerator);
}

void FSQAcceleratorUnion::RemoveSQAccelerator(ISQAccelerator* AcceleratorToRemove)
{
	Accelerators.RemoveSingleSwap(AcceleratorToRemove);	//todo(ocohen): probably want to order these in some optimal way
}