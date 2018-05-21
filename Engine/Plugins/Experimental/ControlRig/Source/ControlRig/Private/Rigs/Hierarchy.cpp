// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Hierarchy.h"
#include "ControlRig.h"
#include "HelperUtil.h"

////////////////////////////////////////////////////////////////////////////////
// FRigHierarchy
////////////////////////////////////////////////////////////////////////////////

void FRigHierarchy::Sort()
{
	TMap<int32, TArray<int32>> HierarchyTree;

	TArray<int32> SortedArray;

	// first figure out children and find roots
	for (int32 Index = 0; Index < Joints.Num(); ++Index)
	{
		int32 ParentIndex = GetIndexSlow(Joints[Index].ParentName);
		if (ParentIndex != INDEX_NONE)
		{
			TArray<int32>& ChildIndices = HierarchyTree.FindOrAdd(ParentIndex);
			ChildIndices.Add(Index);
		}
		else
		{
			// as as a root
			HierarchyTree.Add(Index);
			// add them to the list first
			SortedArray.Add(Index);
		}
	}

	// now go through map and add to sorted array
	for (int32 SortIndex = 0; SortIndex < SortedArray.Num(); ++SortIndex)
	{
		// add children of sorted array
		TArray<int32>* ChildIndices = HierarchyTree.Find(SortedArray[SortIndex]);
		if (ChildIndices)
		{
			// append children now
			SortedArray.Append(*ChildIndices);
			// now sorted array will grow
			// this is same as BFS as it starts from all roots, and going down
		}
	}

	check(SortedArray.Num() == Joints.Num());

	// create new list with sorted
	TArray<FRigJoint> NewSortedList;
	NewSortedList.AddDefaulted(Joints.Num());
	for (int32 NewIndex = 0; NewIndex < SortedArray.Num(); ++NewIndex)
	{
		NewSortedList[NewIndex] = Joints[SortedArray[NewIndex]];
	}

	Joints = MoveTemp(NewSortedList);

	// now fix up parent Index
	for (int32 JointIndex = 0; JointIndex < Joints.Num(); ++JointIndex)
	{
		Joints[JointIndex].ParentIndex = GetIndexSlow(Joints[JointIndex].ParentName);
		// parent index always should be less than this index, even if invalid
		check(Joints[JointIndex].ParentIndex < JointIndex);
	}
}

void FRigHierarchy::RefreshMapping()
{
	Sort();

	NameToIndexMapping.Empty();
	for (int32 Index = 0; Index < Joints.Num(); ++Index)
	{
		NameToIndexMapping.Add(Joints[Index].Name, Index);
	}
}

void FRigHierarchy::Initialize()
{
	RefreshMapping();

	// update parent index
	for (int32 Index = 0; Index < Joints.Num(); ++Index)
	{
		Joints[Index].ParentIndex = GetIndex(Joints[Index].ParentName);
	}

	// initialize transform
	for (int32 Index = 0; Index < Joints.Num(); ++Index)
	{
		Joints[Index].GlobalTransform = Joints[Index].InitialTransform;
		RecalculateLocalTransform(Joints[Index]);

		// update children
		GetChildren(Index, Joints[Index].Dependents, false);
	}
}

void FRigHierarchy::Reset()
{
	// initialize transform
	for (int32 Index = 0; Index < Joints.Num(); ++Index)
	{
		Joints[Index].GlobalTransform = Joints[Index].InitialTransform;
		RecalculateLocalTransform(Joints[Index]);
	}
}

void FRigHierarchy::PropagateTransform(int32 JointIndex)
{
	const TArray<int32> Dependents = Joints[JointIndex].Dependents;
	for (int32 DependentIndex = 0; DependentIndex<Dependents.Num(); ++DependentIndex)
	{
		int32 Index = Dependents[DependentIndex];
		RecalculateGlobalTransform(Joints[Index]);
		PropagateTransform(Index);
	}
}
////////////////////////////////////////////////////////////////////////////////
// FRigHierarchyRef
////////////////////////////////////////////////////////////////////////////////

bool FRigHierarchyRef::CreateHierarchy(const FName& RootName, const FRigHierarchyRef& SourceHierarchyRef)
{
	return CreateHierarchy(RootName, SourceHierarchyRef.Get());
}

bool FRigHierarchyRef::CreateHierarchy(const FName& RootName, const FRigHierarchy* SourceHierarchy)
{
	if (Container)
	{
		FRigHierarchy* Found = Container->Find(Name);
		if (Found)
		{
			return false;
		}
		if (Name == NAME_None)
		{
			const FName NewName = (RootName != NAME_None)? RootName : FName(TEXT("NewName"));
			// find new unique name
			Name = UtilityHelpers::CreateUniqueName(NewName, [this](const FName& CurName) { return Container->Find(CurName) == nullptr; });
		}

		const FRigHierarchy* SourceToCopy = (SourceHierarchy) ? SourceHierarchy : &Container->BaseHierarchy;
		FRigHierarchy NewHierarchy;
		// whenever array reallocates, this will has to be fixed
		// default hierarchy is based on base
		if (RootName == NAME_None)
		{
			NewHierarchy = *SourceToCopy;
		}
		else
		{
			// add root, and all children
			int32 JointIndex = SourceToCopy->GetIndex(RootName);
			if (JointIndex != INDEX_NONE)
			{
				// add root first
				NewHierarchy.AddJoint(RootName, NAME_None, SourceToCopy->Joints[JointIndex].InitialTransform);

				// add all children
				TArray<int32> ChildIndices;
				SourceToCopy->GetChildren(RootName, ChildIndices, true);
				for (int32 ChildIndex = 0; ChildIndex < ChildIndices.Num(); ++ChildIndex)
				{
					const FRigJoint& ChildJoint = SourceToCopy->Joints[ChildIndices[ChildIndex]];
					NewHierarchy.AddJoint(ChildJoint.Name, ChildJoint.ParentName, ChildJoint.InitialTransform);
				}
			}
			else
			{
				// index not found, add warning
				// if somebody typed name, and didn't get to create, something went wrong
				return false;
			}
		}

		uint32& CurrentIndex = Container->MapContainer.Add(Name);
		CurrentIndex = Container->Hierarchies.Add(NewHierarchy);
		return (CurrentIndex != INDEX_NONE);
	}

	return false;
}

bool FRigHierarchyRef::MergeHierarchy(const FRigHierarchy* InSourceHierarchy)
{
	// copy from source to this
	FRigHierarchy* MyHierarchy = Get();
	if (InSourceHierarchy && MyHierarchy)
	{
		for (int32 SourceJointIndex = 0; SourceJointIndex < InSourceHierarchy->GetNum(); ++SourceJointIndex)
		{
			// first find same name, and apply to the this
			const FRigJoint& SourceJoint = InSourceHierarchy->Joints[SourceJointIndex];
			const int32 TargetIndex = MyHierarchy->GetIndex(SourceJoint.Name);
			if (TargetIndex != INDEX_NONE)
			{
				// copy source Joint
				// if parent changed, it will derive that data
				MyHierarchy->Joints[TargetIndex] = SourceJoint;
			}
			else// if we don't find, that means it's new hierarchy
			{
				// parent should add first, so this should work
				MyHierarchy->AddJoint(SourceJoint.Name, SourceJoint.ParentName, SourceJoint.InitialTransform, SourceJoint.LocalTransform, SourceJoint.GlobalTransform);
			}
		}

		return true;
	}

	return false;
}

bool FRigHierarchyRef::MergeHierarchy(const FRigHierarchyRef& SourceHierarchyRef)
{
	return MergeHierarchy(SourceHierarchyRef.Get());
}

