// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionClusteringUtility.h"
#include "GeometryCollection.h"
#include "Containers/Set.h"
#include "Async/ParallelFor.h"

void FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(UGeometryCollection* GeometryCollection, const int32 InsertAtIndex, const TArray<int32>& SelectedBones, bool CalcNewLocalTransform)
{
	check(GeometryCollection);

	TSharedRef<TManagedArray<FGeometryCollectionBoneNode> > HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > TransformsArray = GeometryCollection->GetAttribute<FTransform>("Transform", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FString> > BoneNamesArray = GeometryCollection->GetAttribute<FString>("BoneName", UGeometryCollection::TransformGroup);

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
	TManagedArray<FTransform>& Transforms = *TransformsArray;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FString>& BoneNames = *BoneNamesArray;
	

	// insert a new node between the selected bones and their shared parent
	int NewBoneIndex = GeometryCollection->AddElements(1, UGeometryCollection::TransformGroup);

	// New Bone Setup takes level/parent from the first of the Selected Bones
	int32 SourceBoneIndex = InsertAtIndex;
	int32 OriginalParentIndex = Hierarchy[SourceBoneIndex].Parent;
	BoneNames[NewBoneIndex] = BoneNames[SourceBoneIndex];
	Hierarchy[NewBoneIndex].Level = Hierarchy[SourceBoneIndex].Level;
	Hierarchy[NewBoneIndex].Parent = Hierarchy[SourceBoneIndex].Parent;
	Hierarchy[NewBoneIndex].Children = TSet<int32>(SelectedBones);
	Hierarchy[NewBoneIndex].ClearFlags(FGeometryCollectionBoneNode::ENodeFlags::FS_Geometry);

	// Selected Bone Setup
	FVector SumOfOffsets(0, 0, 0);
	for (int32 SelectedBoneIndex : SelectedBones)
	{
		// Parent Bone Fixup Children - remove the selected nodes
		// the selected bones might not all have the same parent so try remove selected bones from all selected bones' parents
		if (Hierarchy[SelectedBoneIndex].Parent != FGeometryCollectionBoneNode::InvalidBone)
		{
			Hierarchy[Hierarchy[SelectedBoneIndex].Parent].Children.Remove(SelectedBoneIndex);
		}

		Hierarchy[SelectedBoneIndex].Level = Hierarchy[NewBoneIndex].Level + 1;
		Hierarchy[SelectedBoneIndex].Parent = NewBoneIndex;
		Hierarchy[SelectedBoneIndex].SetFlags( FGeometryCollectionBoneNode::FS_Clustered );
		check((Hierarchy[SelectedBoneIndex].Children.Num() > 0) == Hierarchy[SelectedBoneIndex].IsTransform());

		RecursivelyUpdateHierarchyLevelOfChildren(Hierarchy, SelectedBoneIndex);

		// these are already calculated in the case of pre-fractured geometry
		if (CalcNewLocalTransform)
		{
			ExplodedVectors[SelectedBoneIndex] = Transforms[SelectedBoneIndex].GetLocation();
		}

		SumOfOffsets += ExplodedVectors[SelectedBoneIndex];
	}

	Transforms[NewBoneIndex] = FTransform::Identity;
	ExplodedTransforms[NewBoneIndex] = Transforms[NewBoneIndex];

	// Parent Bone Fixup of Children - add the new node under the first bone selected
	// #todo: might want to add it to the one closest to the root in the hierarchy
	if (OriginalParentIndex != FGeometryCollectionBoneNode::InvalidBone)
	{
		Hierarchy[OriginalParentIndex].Children.Add(NewBoneIndex);
	}

	// This bones offset is the average of all the selected bones
	ExplodedVectors[NewBoneIndex] = SumOfOffsets / SelectedBones.Num();

	// update all the bone names from here on down the tree to the leaves
	if (Hierarchy[NewBoneIndex].Parent != FGeometryCollectionBoneNode::InvalidBone)
	{
		RecursivelyUpdateChildBoneNames(Hierarchy[NewBoneIndex].Parent, Hierarchy, BoneNames);
	}
	else
	{
		// #todo: how should we get the appropriate actor's name or invent a name here?
		BoneNames[NewBoneIndex] = "ClusterBone";
		RecursivelyUpdateChildBoneNames(NewBoneIndex, Hierarchy, BoneNames);
	}

}

void FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(UGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TSharedRef<TManagedArray<FGeometryCollectionBoneNode> > HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > TransformsArray = GeometryCollection->GetAttribute<FTransform>("Transform", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FString> > BoneNamesArray = GeometryCollection->GetAttribute<FString>("BoneName", UGeometryCollection::TransformGroup);

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
	TManagedArray<FTransform>& Transforms = *TransformsArray;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FString>& BoneNames = *BoneNamesArray;

	TArray<int32> ChildBones;
	for (int ChildIndex = 0; ChildIndex < Hierarchy.Num(); ChildIndex++)
	{
		ChildBones.Push(ChildIndex);
	}

	// insert a new Root node
	int RootNoneIndex = GeometryCollection->AddElements(1, UGeometryCollection::TransformGroup);

	// New Bone Setup takes level/parent from the first of the Selected Bones
	BoneNames[RootNoneIndex] = "ClusterBone";
	Hierarchy[RootNoneIndex].Level = 0;
	Hierarchy[RootNoneIndex].Parent = FGeometryCollectionBoneNode::InvalidBone;
	Hierarchy[RootNoneIndex].Children = TSet<int32>(ChildBones);
	Hierarchy[RootNoneIndex].StatusFlags = 0; // Not a Geometry Node
	check(Hierarchy[RootNoneIndex].IsTransform());

	// Selected Bone Setup
	FVector SumOfOffsets(0, 0, 0);
	for (int32 ChildBoneIndex : ChildBones)
	{
		Hierarchy[ChildBoneIndex].Level = 1;
		Hierarchy[ChildBoneIndex].Parent = RootNoneIndex;
		Hierarchy[ChildBoneIndex].SetFlags(FGeometryCollectionBoneNode::FS_Geometry | FGeometryCollectionBoneNode::FS_Clustered);
		check(Hierarchy[ChildBoneIndex].IsGeometry());

		ExplodedVectors[ChildBoneIndex] = Transforms[ChildBoneIndex].GetLocation();
		ExplodedTransforms[ChildBoneIndex] = Transforms[ChildBoneIndex];

		SumOfOffsets += ExplodedVectors[ChildBoneIndex];
	}

	Transforms[RootNoneIndex] = FTransform::Identity;
	ExplodedTransforms[RootNoneIndex] = Transforms[RootNoneIndex];

	// This bones offset is the average of all the selected bones
	ExplodedVectors[RootNoneIndex] = SumOfOffsets / ChildBones.Num();

	RecursivelyUpdateChildBoneNames(RootNoneIndex, Hierarchy, BoneNames);
}

void FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingRoot(UGeometryCollection* GeometryCollection, TArray<int32>& SourceElements)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", UGeometryCollection::TransformGroup);

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

	TArray<int32> RootBonesOut;
	GetRootBones(GeometryCollection, RootBonesOut);
	check(RootBonesOut.Num() == 1); // only expecting a single root node
	int32 RootBoneElement = RootBonesOut[0];

	// New Bone Setup takes level/parent from the first of the Selected Bones
	check(Hierarchy[RootBoneElement].Level == 0);
	check(Hierarchy[RootBoneElement].Parent == FGeometryCollectionBoneNode::InvalidBone);

	TArray<int32> NodesToDelete;
	for (int32 SourceElement : SourceElements)
	{
		Hierarchy[RootBoneElement].Children.Add(SourceElement);

		int32 ParentElement = Hierarchy[SourceElement].Parent;
		while(ParentElement != FGeometryCollectionBoneNode::InvalidBone && ParentElement != RootBoneElement)
		{
			NodesToDelete.AddUnique(ParentElement);
			ParentElement = Hierarchy[ParentElement].Parent;
		};

		Hierarchy[SourceElement].Level = 1;
		Hierarchy[SourceElement].Parent = RootBoneElement;
		Hierarchy[SourceElement].ClearFlags(FGeometryCollectionBoneNode::FS_Clustered); // ??
	}

	if (NodesToDelete.Num() > 0)
	{
		DeleteNodesInHierarchy(GeometryCollection, NodesToDelete);
	}

	RecursivelyUpdateChildBoneNames(RootBoneElement, Hierarchy, BoneNames);
}

void FGeometryCollectionClusteringUtility::CollapseHierarchyOneLevel(UGeometryCollection* GeometryCollection, TArray<int32>& SourceElements)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", UGeometryCollection::TransformGroup);

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

	for (int32 SourceElement : SourceElements)
	{
		int32 DeletedNode = SourceElement;
		if (DeletedNode != FGeometryCollectionBoneNode::InvalidBone)
		{
			int32 NewParentElement = Hierarchy[DeletedNode].Parent;

			if (NewParentElement != FGeometryCollectionBoneNode::InvalidBone)
			{
				for (int32 ChildElement : Hierarchy[DeletedNode].Children)
				{
					Hierarchy[NewParentElement].Children.Add(ChildElement);

					Hierarchy[ChildElement].Level -= 1;
					Hierarchy[ChildElement].Parent = NewParentElement;
					Hierarchy[ChildElement].ClearFlags(FGeometryCollectionBoneNode::FS_Clustered); // ??
				}
			}
		}
	}

	DeleteNodesInHierarchy(GeometryCollection, SourceElements);
	TArray<int32> Roots;
	GetRootBones(GeometryCollection, Roots);
	RecursivelyUpdateChildBoneNames(Roots[0], Hierarchy, BoneNames);
}


void FGeometryCollectionClusteringUtility::DeleteNodesInHierarchy(UGeometryCollection* GeometryCollection, TArray<int32>& NodesToDelete)
{
	check(GeometryCollection);
	check(NodesToDelete.Num() > 0);
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<int32>& BoneMap = *GeometryCollection->BoneMap;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", UGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", UGeometryCollection::TransformGroup);
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;

	// bone names, exploded vectors, exploded transforms

	int32 OriginalSize = Hierarchy.Num();
	int32 DeletedNumber = 0;
	for (int32 Element : NodesToDelete)
	{
		// should never be deleting a node that contains geometry
		if (Hierarchy[Element].IsGeometry())
			continue;

		DeletedNumber++;
		int32 SwapElement = OriginalSize - DeletedNumber;
		int32 ParentElement = Hierarchy[SwapElement].Parent;

		if (ParentElement != FGeometryCollectionBoneNode::InvalidBone)
		{
			// remove parents reference to this child
			Hierarchy[ParentElement].Children.Remove(SwapElement);
		}

		// move data so deleted are at bottom of element list
		Hierarchy[Element] = Hierarchy[SwapElement];
		Transforms[Element] = Transforms[SwapElement];
		ExplodedVectors[Element] = ExplodedVectors[SwapElement];
		ExplodedTransforms[Element] = ExplodedTransforms[SwapElement];

		// sort out parent references of children that are being re-parented
		ParallelFor(Hierarchy.Num(), [&](int32 i)
		{
			if (Hierarchy[i].Parent == SwapElement)
			{
				Hierarchy[i].Parent = Element;
			}
		});

		ParallelFor(BoneMap.Num(), [&](int32 i)
		{
			if (BoneMap[i] == SwapElement)
				BoneMap[i] = Element;
		});

	}

	if (DeletedNumber > 0)
	{
		// now resize the Transform group
		int NewSize = OriginalSize - DeletedNumber;
		GeometryCollection->Resize(NewSize, UGeometryCollection::TransformGroup);
	}

}

bool FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(UGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

	// never assume the root bone is always index 0 in the particle group
	int NumRootBones = 0;
	for (int i = 0; i < Hierarchy.Num(); i++)
	{
		if (Hierarchy[i].Parent == FGeometryCollectionBoneNode::InvalidBone)
		{
			NumRootBones++;
			if (NumRootBones > 1)
			{
				return true;
			}
		}
	}
	return false;
}


void FGeometryCollectionClusteringUtility::GetRootBones(const UGeometryCollection* GeometryCollection, TArray<int32>& RootBonesOut)
{
	check(GeometryCollection);
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	
	// never assume the root bone is always index 0 in the particle group
	for (int i = 0; i < Hierarchy.Num(); i++)
	{
		if (Hierarchy[i].Parent == FGeometryCollectionBoneNode::InvalidBone)
		{
			RootBonesOut.AddUnique(i);
		}
	}
}

void FGeometryCollectionClusteringUtility::GetClusteredBonesWithCommonParent(const UGeometryCollection* GeometryCollection, int32 SourceBone, TArray<int32>& BonesOut)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

	// then see if this bone as any other bones clustered to it
	if (Hierarchy[SourceBone].StatusFlags & FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered)
	{
		int32 SourceParent = Hierarchy[SourceBone].Parent;

		for (int i = 0; i < Hierarchy.Num(); i++)
		{
			if (SourceParent == Hierarchy[i].Parent && (Hierarchy[i].StatusFlags & FGeometryCollectionBoneNode::ENodeFlags::FS_Clustered))
				BonesOut.AddUnique(i);
		}
	}

}

void FGeometryCollectionClusteringUtility::GetChildBonesFromLevel(const UGeometryCollection* GeometryCollection, int32 SourceBone, int32 Level, TArray<int32>& BonesOut)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

	if (SourceBone >= 0)
	{
		int32 SourceParent = SourceBone;
		while (Hierarchy[SourceParent].Level > Level)
		{
			if (Hierarchy[SourceParent].Parent == -1)
				break;

			SourceParent = Hierarchy[SourceParent].Parent;
		}

		RecursiveAddAllChildren(Hierarchy, SourceParent, BonesOut);
	}

}

void FGeometryCollectionClusteringUtility::RecursiveAddAllChildren(const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, int32 SourceBone, TArray<int32>& BonesOut)
{
	BonesOut.AddUnique(SourceBone);
	for (int32 Child : Hierarchy[SourceBone].Children)
	{
		RecursiveAddAllChildren(Hierarchy, Child, BonesOut);
	}

}

int32 FGeometryCollectionClusteringUtility::GetParentOfBoneAtSpecifiedLevel(const UGeometryCollection* GeometryCollection, int32 SourceBone, int32 Level)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

	if (SourceBone >= 0)
	{
		int32 SourceParent = SourceBone;
		while (Hierarchy[SourceParent].Level > Level)
		{
			if (Hierarchy[SourceParent].Parent == -1)
				break;

			SourceParent = Hierarchy[SourceParent].Parent;
		}

		return SourceParent;
	}

	return FGeometryCollectionBoneNode::InvalidBone;
}

void FGeometryCollectionClusteringUtility::RecursivelyUpdateChildBoneNames(int32 BoneIndex, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, TManagedArray<FString>& BoneNames)
{
	check(BoneIndex < Hierarchy.Num());

	if (Hierarchy[BoneIndex].Children.Num() > 0)
	{
		const FString& ParentName = BoneNames[BoneIndex];
		for (int32 ChildIndex : Hierarchy[BoneIndex].Children)
		{
			TCHAR ChunkNumberStr[5] = { 0 };
			FCString::Sprintf(ChunkNumberStr, TEXT("_%03d"), ChildIndex);
			FString NewName = ParentName + ChunkNumberStr;
			BoneNames[ChildIndex] = NewName;
			RecursivelyUpdateChildBoneNames(ChildIndex, Hierarchy, BoneNames);
		}
	}
}

void FGeometryCollectionClusteringUtility::RecursivelyUpdateHierarchyLevelOfChildren(TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, int32 ParentElement)
{
	check(ParentElement < Hierarchy.Num());

	for (int32 Element : Hierarchy[ParentElement].Children)
	{
		Hierarchy[Element].Level = Hierarchy[ParentElement].Level + 1;
		RecursivelyUpdateHierarchyLevelOfChildren(Hierarchy, Element);
	}

}

void FGeometryCollectionClusteringUtility::CollapseLevelHierarchy(int8 Level, UGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

	TArray<int32> Elements;

	if (Level == -1) // AllLevels
	{

		for (int Element = 0; Element < Hierarchy.Num(); Element++)
		{
			const FGeometryCollectionBoneNode& Node = Hierarchy[Element];
			if (Node.IsGeometry())
			{
				Elements.Add(Element);
			}
		}

		ClusterBonesUnderExistingRoot(GeometryCollection, Elements);
	}
	else
	{
		for (int Element = 0; Element < Hierarchy.Num(); Element++)
		{
			const FGeometryCollectionBoneNode& Node = Hierarchy[Element];

			// if matches selected level then re-parent this node to the root
			if (Node.Level == Level)
			{
				Elements.Add(Element);
			}
		}

		CollapseHierarchyOneLevel(GeometryCollection, Elements);
	}

}

void FGeometryCollectionClusteringUtility::CollapseSelectedHierarchy(int8 Level, const TArray<int32>& SelectedBones, UGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *(GeometryCollection->BoneHierarchy);

	// can't collapse root node away and doesn't make sense to operate when AllLevels selected
	if (Level > 0)
	{
		TArray<int32> Elements;
		for (int Element = 0; Element < SelectedBones.Num(); Element++)
		{
			FGeometryCollectionBoneNode& Node = Hierarchy[SelectedBones[Element]];

			// if matches selected level then re-parent this node to the root if it's not a leaf node
			if (Node.Level == Level && Node.Children.Num() > 0)
			{
				Elements.Add(SelectedBones[Element]);
			}
		}

		if (Elements.Num() > 0)
		{
			CollapseHierarchyOneLevel(GeometryCollection, Elements);
		}
	}

}
