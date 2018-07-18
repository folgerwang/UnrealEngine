// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hierarchy.generated.h"

class UControlRig;

USTRUCT(BlueprintType)
struct FRigJoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = FRigHierarchy)
	FName Name;

	UPROPERTY(VisibleAnywhere, Category = FRigHierarchy)
	FName ParentName;

	UPROPERTY(transient)
	int32 ParentIndex;

	/* Initial global transform that is saved in this rig */
	UPROPERTY(EditAnywhere, Category = FRigHierarchy)
	FTransform InitialTransform;

	UPROPERTY(transient, VisibleAnywhere, Category = FRigHierarchy)
	FTransform GlobalTransform;

	UPROPERTY(transient, VisibleAnywhere, Category = FRigHierarchy)
	FTransform LocalTransform;

	/** dependent list - direct dependent for child or anything that needs to update due to this */
	UPROPERTY(transient)
	TArray<int32> Dependents;
};

USTRUCT()
struct CONTROLRIG_API FRigHierarchy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = FRigHierarchy)
	TArray<FRigJoint> Joints;

	// can serialize fine? 
	UPROPERTY()
	TMap<FName, int32> NameToIndexMapping;

	void AddJoint(const FName& NewJointName, const FName& Parent, const FTransform& InitTransform)
	{
		int32 ParentIndex = GetIndex(Parent);
		bool bHasParent = (ParentIndex != INDEX_NONE);

		FRigJoint NewJoint;
		NewJoint.Name = NewJointName;
		NewJoint.ParentIndex = ParentIndex;
		NewJoint.ParentName = bHasParent? Parent : NAME_None;
		NewJoint.InitialTransform = InitTransform;
		NewJoint.GlobalTransform = InitTransform;
		RecalculateLocalTransform(NewJoint);

		Joints.Add(NewJoint);
		RefreshMapping();
	}

	void AddJoint(const FName& NewJointName, const FName& Parent, const FTransform& InitTransform, const FTransform& LocalTransform, const FTransform& GlobalTransform)
	{
		AddJoint(NewJointName, Parent, InitTransform);

		int32 NewIndex = GetIndex(NewJointName);
		Joints[NewIndex].LocalTransform = LocalTransform;
		Joints[NewIndex].GlobalTransform = GlobalTransform;
	}

	void Reparent(const FName& InJoint, const FName& NewParent)
	{
		int32 Index = GetIndex(InJoint);
		// can't parent to itself
		if (Index != INDEX_NONE && InJoint != NewParent)
		{
			// should allow reparent to none (no parent)
			// if invalid, we consider to be none
			int32 ParentIndex = GetIndex(NewParent);
			bool bHasParent = (ParentIndex != INDEX_NONE);
			FRigJoint& CurJoint = Joints[Index];
			CurJoint.ParentIndex = ParentIndex;
			CurJoint.ParentName = (bHasParent)? NewParent : NAME_None;
			RecalculateLocalTransform(CurJoint);

			// we want to make sure parent is before the child
			RefreshMapping();
		}
	}

	void DeleteJoint(const FName& JointToDelete, bool bIncludeChildren)
	{
		TArray<int32> Children;
		if (GetChildren(JointToDelete, Children, true) > 0)
		{
			// sort by child index
			Children.Sort([](const int32& A, const int32& B) { return A < B; });

			// want to delete from end to the first 
			for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
			{
				Joints.RemoveAt(Children[ChildIndex]);
			}
		}

		// in theory, since I'm not adding new element here
		// it is safe to do search using FindIndex, but it may cause
		// difficulty in the future, so I'm changing to FindIndexSlow
		// note that we're removing units here, but the index is removed from later
		// to begin, so in theory it should be fine
		int32 IndexToDelete = GetIndex(JointToDelete);
		Joints.RemoveAt(IndexToDelete);

		RefreshMapping();
	}

	FName GetParentName(const FName& InJoint) const
	{
		int32 Index = GetIndex(InJoint);
		if (Index != INDEX_NONE)
		{
			return Joints[Index].ParentName;
		}

		return NAME_None;
	}

	int32 GetParentIndex(const int32 JointIndex) const
	{
		if (JointIndex != INDEX_NONE)
		{
			return Joints[JointIndex].ParentIndex;
		}

		return INDEX_NONE;
	}
	// list of names of children - this is not cheap, and is supposed to be used only for one time set up
	int32 GetChildren(const FName& InJoint, TArray<int32>& OutChildren, bool bRecursively) const
	{
		return GetChildren(GetIndex(InJoint), OutChildren, bRecursively);
	}

	int32 GetChildren(const int32 InJointIndex, TArray<int32>& OutChildren, bool bRecursively) const
	{
		OutChildren.Reset();

		if (InJointIndex != INDEX_NONE)
		{
			GetChildrenRecursive(InJointIndex, OutChildren, bRecursively);
		}

		return OutChildren.Num();
	}

	FName GetName(int32 Index) const
	{
		if (Joints.IsValidIndex(Index))
		{
			return Joints[Index].Name;
		}

		return NAME_None;
	}

	int32 GetIndex(const FName& Joint) const
	{
		// ensure if it does not match
		//ensureAlways(Joints.Num() == NameToIndexMapping.Num());

		const int32* Index = NameToIndexMapping.Find(Joint);
		if (Index)
		{
			return *Index;
		}

		return INDEX_NONE;
	}

//#if WITH_EDITOR
	// @FIXMELINA: figure out how to remove this outside of editor
	// ignore mapping, run slow search
	// this is useful in editor while editing
	// we don't want to build mapping data every time
	int32 GetIndexSlow(const FName& Joint) const
	{
		for (int32 JointId = 0; JointId < Joints.Num(); ++JointId)
		{
			if (Joints[JointId].Name == Joint)
			{
				return JointId;
			}
		}

		return INDEX_NONE;
	}
//#endif // WITH_EDITOR
	void SetGlobalTransform(const FName& Joint, const FTransform& InTransform, bool bPropagateTransform = true)
	{
		SetGlobalTransform(GetIndex(Joint), InTransform, bPropagateTransform);
	}

	void SetGlobalTransform(int32 Index, const FTransform& InTransform, bool bPropagateTransform = true)
	{
		if (Joints.IsValidIndex(Index))
		{
			FRigJoint& Joint = Joints[Index];
			Joint.GlobalTransform = InTransform;
			Joint.GlobalTransform.NormalizeRotation();
			RecalculateLocalTransform(Joint);

			if (bPropagateTransform)
			{
				PropagateTransform(Index);
			}
		}
	}

	FTransform GetGlobalTransform(const FName& Joint) const
	{
		return GetGlobalTransform(GetIndex(Joint));
	}

	FTransform GetGlobalTransform(int32 Index) const
	{
		if (Joints.IsValidIndex(Index))
		{
			return Joints[Index].GlobalTransform;
		}

		return FTransform::Identity;
	}

	void SetLocalTransform(const FName& Joint, const FTransform& InTransform, bool bPropagateTransform = true)
	{
		SetLocalTransform(GetIndex(Joint), InTransform, bPropagateTransform);
	}

	void SetLocalTransform(int32 Index, const FTransform& InTransform, bool bPropagateTransform = true)
	{
		if (Joints.IsValidIndex(Index))
		{
			FRigJoint& Joint = Joints[Index];
			Joint.LocalTransform = InTransform;
			RecalculateGlobalTransform(Joint);

			if (bPropagateTransform)
			{
				PropagateTransform(Index);
			}
		}
	}

	FTransform GetLocalTransform(const FName& Joint) const
	{
		return GetLocalTransform(GetIndex(Joint));
	}

	FTransform GetLocalTransform(int32 Index) const
	{
		if (Joints.IsValidIndex(Index))
		{
			return Joints[Index].LocalTransform;
		}

		return FTransform::Identity;
	}

	void SetInitialTransform(const FName& Joint, const FTransform& InTransform)
	{
		SetInitialTransform(GetIndex(Joint), InTransform);
	}

	void SetInitialTransform(int32 Index, const FTransform& InTransform)
	{
		if (Joints.IsValidIndex(Index))
		{
			FRigJoint& Joint = Joints[Index];
			Joint.InitialTransform = InTransform;
			Joint.InitialTransform.NormalizeRotation();
			RecalculateLocalTransform(Joint);
		}
	}

	FTransform GetInitialTransform(const FName& Joint) const
	{
		return GetInitialTransform(GetIndex(Joint));
	}

	FTransform GetInitialTransform(int32 Index) const
	{
		if (Joints.IsValidIndex(Index))
		{
			return Joints[Index].InitialTransform;
		}

		return FTransform::Identity;
	}
	// @todo: move to private
	void RecalculateLocalTransform(FRigJoint& InOutJoint)
	{
		bool bHasParent = InOutJoint.ParentIndex != INDEX_NONE;
		InOutJoint.LocalTransform = (bHasParent) ? InOutJoint.GlobalTransform.GetRelativeTransform(Joints[InOutJoint.ParentIndex].GlobalTransform) : InOutJoint.GlobalTransform;
	}

	// @todo: move to private
	void RecalculateGlobalTransform(FRigJoint& InOutJoint)
	{
		bool bHasParent = InOutJoint.ParentIndex != INDEX_NONE;
		InOutJoint.GlobalTransform = (bHasParent) ? InOutJoint.LocalTransform * Joints[InOutJoint.ParentIndex].GlobalTransform : InOutJoint.LocalTransform;
	}

	void Rename(const FName& OldName, const FName& NewName)
	{
		if (OldName != NewName)
		{
			const int32 Found = GetIndex(OldName);
			if (Found != INDEX_NONE)
			{
				Joints[Found].Name = NewName;

				// go through find all children and rename them
				for (int32 Index = 0; Index < Joints.Num(); ++Index)
				{
					if (Joints[Index].ParentName == OldName)
					{
						Joints[Index].ParentName = NewName;
					}
				}

				RefreshMapping();
			}
		}
	}

	void Initialize();
	void Reset();

	int32 GetNum() const
	{
		return Joints.Num();
	}
private:
	void RefreshMapping();
	void Sort();

	// list of names of children - this is not cheap, and is supposed to be used only for one time set up
	int32 GetChildrenRecursive(const int32 InJointIndex, TArray<int32>& OutChildren, bool bRecursively) const
	{
		const int32 StartChildIndex = OutChildren.Num();

		// all children should be later than parent
		for (int32 ChildIndex = InJointIndex + 1; ChildIndex < Joints.Num(); ++ChildIndex)
		{
			if (Joints[ChildIndex].ParentIndex == InJointIndex)
			{
				OutChildren.AddUnique(ChildIndex);
			}
		}

		if (bRecursively)
		{
			// since we keep appending inside of functions, we make sure not to go over original list
			const int32 EndChildIndex = OutChildren.Num() - 1;
			for (int32 ChildIndex = StartChildIndex; ChildIndex <= EndChildIndex; ++ChildIndex)
			{
				GetChildrenRecursive(OutChildren[ChildIndex], OutChildren, bRecursively);
			}
		}

		return OutChildren.Num();
	}

	void PropagateTransform(int32 JointIndex);
};

USTRUCT()
struct FRigHierarchyContainer
{
	GENERATED_BODY()

	// index to hierarchy
	TMap<FName, uint32> MapContainer;

	// list of hierarchies
	TArray<FRigHierarchy> Hierarchies;

	// base hierarchy
	// this should serialize
	UPROPERTY()
	FRigHierarchy BaseHierarchy;

	FRigHierarchy* Find(const FName& InName)
	{
		uint32* IndexPtr = MapContainer.Find(InName);
		if (IndexPtr && Hierarchies.IsValidIndex(*IndexPtr))
		{
			return &Hierarchies[*IndexPtr];
		}

		return nullptr;
	}

	void Reset()
	{
		BaseHierarchy.Reset();

		// @todo reset whole hierarhcy
	}
};

USTRUCT(BlueprintType)
struct FRigHierarchyRef
{
	GENERATED_BODY()

	FRigHierarchyRef()
		: Container(nullptr)
		, bUseBaseHierarchy(true)
	{

	}

	FRigHierarchy* Get() 
	{
		return GetInternal();
	}

	const FRigHierarchy* Get() const
	{
		return (const FRigHierarchy*)GetInternal();
	}

	FRigHierarchy* Find(const FName& InName)
	{
		if(Container)
		{
			return Container->Find(InName);
		}
		return nullptr;
	}

	// create name if the name isn't set, for now it only accepts root
	bool CreateHierarchy(const FName& RootName, const FRigHierarchyRef& SourceHierarchyRef);
	bool MergeHierarchy(const FRigHierarchyRef& SourceHierarchyRef);

private:
	struct FRigHierarchyContainer* Container;

	// @todo: this only works with merge right now. Should fix for all cases
	UPROPERTY(EditAnywhere, Category = FRigHierarchyRef)
	bool bUseBaseHierarchy; 

	/** Name of Hierarchy */
	UPROPERTY(EditAnywhere, Category = FRigHierarchyRef, meta=(EditCondition = "!bUseBaseHierarchy"))
	FName Name;

	// utility functions
	bool CreateHierarchy(const FName& RootName, const FRigHierarchy* SourceHierarchy);
	bool MergeHierarchy(const FRigHierarchy* InSourceHierarchy);

	FRigHierarchy* GetInternal() const
	{
		if (Container)
		{
			if (bUseBaseHierarchy)
			{
				return &Container->BaseHierarchy;
			}

			return Container->Find(Name);
		}
		return nullptr;
	}
	friend class UControlRig;
};