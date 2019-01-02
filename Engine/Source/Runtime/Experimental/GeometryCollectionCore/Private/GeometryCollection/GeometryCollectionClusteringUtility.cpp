// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Containers/Set.h"
#include "Async/ParallelFor.h"

void FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(FGeometryCollection* GeometryCollection, const int32 InsertAtIndex, const TArray<int32>& SelectedBones, bool CalcNewLocalTransform)
{
	check(GeometryCollection);


	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;


	// insert a new node between the selected bones and their shared parent
	int NewBoneIndex = GeometryCollection->AddElements(1, FGeometryCollection::TransformGroup);

	// New Bone Setup takes level/parent from the first of the Selected Bones
	int32 SourceBoneIndex = InsertAtIndex;
	int32 OriginalParentIndex = Hierarchy[SourceBoneIndex].Parent;
	BoneNames[NewBoneIndex] = BoneNames[SourceBoneIndex];
	Hierarchy[NewBoneIndex].Level = Hierarchy[SourceBoneIndex].Level;
	Hierarchy[NewBoneIndex].Parent = Hierarchy[SourceBoneIndex].Parent;
	Hierarchy[NewBoneIndex].Children = TSet<int32>(SelectedBones);
	Hierarchy[NewBoneIndex].ClearFlags(FGeometryCollectionBoneNode::ENodeFlags::FS_Geometry);

	Transforms[NewBoneIndex] = FTransform::Identity;

	if (GeometryCollection->HasAttribute("ExplodedVector", FGeometryCollection::TransformGroup) &&
		GeometryCollection->HasAttribute("ExplodedTransform", FGeometryCollection::TransformGroup))
	{
		TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
		TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

		TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
		TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;

		ExplodedTransforms[NewBoneIndex] = Transforms[NewBoneIndex];
		ResetSliderTransforms(ExplodedTransforms, Transforms);

		// Selected Bone Setup
		FVector SumOfOffsets(0, 0, 0);
		for (int32 SelectedBoneIndex : SelectedBones)
		{
			ExplodedTransforms[SelectedBoneIndex] = Transforms[SelectedBoneIndex];
			SumOfOffsets += ExplodedVectors[SelectedBoneIndex];
		}

		// This bones offset is the average of all the selected bones
		ExplodedVectors[NewBoneIndex] = SumOfOffsets / SelectedBones.Num();
	}

	// re-parent all the geometry nodes under the new shared bone
	GeometryCollectionAlgo::ParentTransforms(GeometryCollection, NewBoneIndex, SelectedBones);


	RecursivelyUpdateHierarchyLevelOfChildren(Hierarchy, NewBoneIndex);

	// Parent Bone Fixup of Children - add the new node under the first bone selected
	// #todo: might want to add it to the one closest to the root in the hierarchy
	if (OriginalParentIndex != FGeometryCollectionBoneNode::InvalidBone)
	{
		Hierarchy[OriginalParentIndex].Children.Add(NewBoneIndex);
	}

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

	//
	// determine original parents of moved nodes so we can update their childrens names
	//
	TArray<int32> ParentsToUpdateNames;
	for (int32 SourceElement : SelectedBones)
	{
		ParentsToUpdateNames.AddUnique(Hierarchy[SourceElement].Parent);
	}
	for (int32 NodeIndex : ParentsToUpdateNames)
	{
		RecursivelyUpdateChildBoneNames(NodeIndex, Hierarchy, BoneNames);
	}

	ValidateResults(GeometryCollection);
}

void FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(FGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;

	TArray<int32> ChildBones;
	for (int ChildIndex = 0; ChildIndex < Hierarchy.Num(); ChildIndex++)
	{
		ChildBones.Push(ChildIndex);
	}

	// insert a new Root node
	int RootNoneIndex = GeometryCollection->AddElements(1, FGeometryCollection::TransformGroup);

	// New Bone Setup takes level/parent from the first of the Selected Bones
	BoneNames[RootNoneIndex] = "ClusterBone";
	Hierarchy[RootNoneIndex].Level = 0;
	Hierarchy[RootNoneIndex].Parent = FGeometryCollectionBoneNode::InvalidBone;
	Hierarchy[RootNoneIndex].Children = TSet<int32>(ChildBones);
	Hierarchy[RootNoneIndex].StatusFlags = 0; // Not a Geometry Node
	check(Hierarchy[RootNoneIndex].IsTransform());

	if (GeometryCollection->HasAttribute("ExplodedVector", FGeometryCollection::TransformGroup) &&
		GeometryCollection->HasAttribute("ExplodedTransform", FGeometryCollection::TransformGroup) )
	{

		TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
		TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

		TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
		TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;

		FVector SumOfOffsets(0, 0, 0);
		for (int32 ChildBoneIndex : ChildBones)
		{
			check(Hierarchy[ChildBoneIndex].IsGeometry());
			ExplodedVectors[ChildBoneIndex] = Transforms[ChildBoneIndex].GetLocation();
			ExplodedTransforms[ChildBoneIndex] = Transforms[ChildBoneIndex];
			SumOfOffsets += ExplodedVectors[ChildBoneIndex];
		}
		ExplodedTransforms[RootNoneIndex] = Transforms[RootNoneIndex];
		// This bones offset is the average of all the selected bones
		ExplodedVectors[RootNoneIndex] = SumOfOffsets / ChildBones.Num();
	}

	// Selected Bone Setup
	for (int32 ChildBoneIndex : ChildBones)
	{
		Hierarchy[ChildBoneIndex].Level = 1;
		Hierarchy[ChildBoneIndex].Parent = RootNoneIndex;
		Hierarchy[ChildBoneIndex].SetFlags(FGeometryCollectionBoneNode::FS_Geometry | FGeometryCollectionBoneNode::FS_Clustered);
		check(Hierarchy[ChildBoneIndex].IsGeometry());

	}

	Transforms[RootNoneIndex] = FTransform::Identity;


	RecursivelyUpdateChildBoneNames(RootNoneIndex, Hierarchy, BoneNames);

	ValidateResults(GeometryCollection);
}


void FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingRoot(FGeometryCollection* GeometryCollection, TArray<int32>& SourceElements)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

	TArray<int32> RootBonesOut;
	GetRootBones(GeometryCollection, RootBonesOut);
	check(RootBonesOut.Num() == 1); // only expecting a single root node
	int32 RootBoneElement = RootBonesOut[0];
	check(Hierarchy[RootBoneElement].Level == 0);
	check(Hierarchy[RootBoneElement].Parent == FGeometryCollectionBoneNode::InvalidBone);

	ResetSliderTransforms(ExplodedTransforms, Transforms);

	// re-parent all the geometry nodes under the root node
	GeometryCollectionAlgo::ParentTransforms(GeometryCollection, RootBoneElement, SourceElements);

	// update source levels and transforms in our custom attributes
	for (int32 Element : SourceElements)
	{
		Hierarchy[Element].Level = 1;
		ExplodedTransforms[Element] = Transforms[Element];
		ExplodedVectors[Element] = Transforms[Element].GetLocation();
	}

	// delete all the redundant transform nodes that we no longer use
	TArray<int32> NodesToDelete;
	for (int Element = 0; Element < Hierarchy.Num(); Element++)
	{
		const FGeometryCollectionBoneNode& Node = Hierarchy[Element];
		if (Element != RootBoneElement && Node.IsTransform())
		{
			NodesToDelete.Add(Element);
		}
	}

	if (NodesToDelete.Num() > 0)
	{
		DeleteNodesInHierarchy(GeometryCollection, NodesToDelete);
	}

	RecursivelyUpdateChildBoneNames(RootBoneElement, Hierarchy, BoneNames);

	ValidateResults(GeometryCollection);
}

void FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingNode(FGeometryCollection* GeometryCollection, const TArray<int32>& SourceElements)
{
	int32 MergeNode = PickBestNodeToMergeTo(GeometryCollection, SourceElements);
	ClusterBonesUnderExistingNode(GeometryCollection, MergeNode, SourceElements);

}

void FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingNode(FGeometryCollection* GeometryCollection, int32 MergeNode, const TArray<int32>& SourceElementsIn)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);

	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;

	// remove Merge Node if it's in the list - happens due to the way selection works
	TArray<int32> SourceElements;
	for (int32 Element : SourceElementsIn)
	{
		if (Element != MergeNode)
		{
			SourceElements.Push(Element);
		}
	}

	if (MergeNode != FGeometryCollectionBoneNode::InvalidBone)
	{
		bool IllegalOperation = false;
		for (int32 SourceElement : SourceElements)
		{
			if (NodeExistsOnThisBranch(GeometryCollection, MergeNode, SourceElement))
			{
				IllegalOperation = true;
				break;
			}
		}

		if (!IllegalOperation)
		{
			TArray<int32> ParentsToUpdateNames;
			// determine original parents of moved nodes so we can update their childrens names
			for (int32 SourceElement : SourceElementsIn)
			{
				ParentsToUpdateNames.AddUnique(Hierarchy[SourceElement].Parent);
			}

			ResetSliderTransforms(ExplodedTransforms, Transforms);

			// re-parent all the geometry nodes under existing merge node
			GeometryCollectionAlgo::ParentTransforms(GeometryCollection, MergeNode, SourceElements);

			// update source levels and transforms in our custom attributes
			for (int32 Element : SourceElements)
			{
				ExplodedTransforms[Element] = Transforms[Element];
				ExplodedVectors[Element] = Transforms[Element].GetLocation();
			}

			RecursivelyUpdateHierarchyLevelOfChildren(Hierarchy, MergeNode);

			RecursivelyUpdateChildBoneNames(MergeNode, Hierarchy, BoneNames);

			for (int32 NodeIndex : ParentsToUpdateNames)
			{
				RecursivelyUpdateChildBoneNames(NodeIndex, Hierarchy, BoneNames);
			}
		}
	}

	// add common root node if multiple roots found
	if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(GeometryCollection))
	{
		FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(GeometryCollection);
	}

	ValidateResults(GeometryCollection);
}

void FGeometryCollectionClusteringUtility::ClusterBonesByContext(FGeometryCollection* GeometryCollection, int32 MergeNode, const TArray<int32>& SourceElementsIn)
{
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	if (Hierarchy[MergeNode].IsTransform())
	{
		ClusterBonesUnderExistingNode(GeometryCollection, MergeNode, SourceElementsIn);
	}
	else
	{
		TArray<int32> SourceElements = SourceElementsIn;
		SourceElements.Push(MergeNode);
		ClusterBonesUnderNewNode(GeometryCollection, MergeNode, SourceElements, true);
	}
}

void FGeometryCollectionClusteringUtility::CollapseHierarchyOneLevel(FGeometryCollection* GeometryCollection, TArray<int32>& SourceElements)
{
	check(GeometryCollection);
	bool CalcNewLocalTransform = true;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);

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
					Hierarchy[ChildElement].ClearFlags(FGeometryCollectionBoneNode::FS_Clustered);
				}
			}
		}
	}

	DeleteNodesInHierarchy(GeometryCollection, SourceElements);
	TArray<int32> Roots;
	GetRootBones(GeometryCollection, Roots);
	RecursivelyUpdateChildBoneNames(Roots[0], Hierarchy, BoneNames);

	ValidateResults(GeometryCollection);
}


bool FGeometryCollectionClusteringUtility::NodeExistsOnThisBranch(const FGeometryCollection* GeometryCollection, int32 TestNode, int32 TreeElement)
{
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

	if (TestNode == TreeElement)
		return true;

	if (Hierarchy[TreeElement].Children.Num() > 0)
	{
		for (int32 ChildIndex : Hierarchy[TreeElement].Children)
		{
			if (NodeExistsOnThisBranch(GeometryCollection, TestNode, ChildIndex))
				return true;
		}
	}

	return false;

}

void FGeometryCollectionClusteringUtility::RenameBone(FGeometryCollection* GeometryCollection, int32 BoneIndex, const FString& NewName, bool UpdateChildren /* = true */)
{
	TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

	BoneNames[BoneIndex] = NewName;

	if (UpdateChildren)
	{
		FGeometryCollectionClusteringUtility::RecursivelyUpdateChildBoneNames(BoneIndex, Hierarchy, BoneNames, true);
	}
}

int32 FGeometryCollectionClusteringUtility::PickBestNodeToMergeTo(const FGeometryCollection* GeometryCollection, const TArray<int32>& SourceElements)
{
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

	// which of the source elements is the most significant, closest to the root that has children (is a cluster)
	int32 ElementClosestToRoot = -1;
	int32 LevelClosestToRoot = -1;

	for (int32 Element : SourceElements)
	{
		if (Hierarchy[Element].Children.Num() > 0 && (Hierarchy[Element].Level < LevelClosestToRoot || LevelClosestToRoot == -1))
		{
			LevelClosestToRoot = Hierarchy[Element].Level;
			ElementClosestToRoot = Element;
		}
	}

	return ElementClosestToRoot;
}

void FGeometryCollectionClusteringUtility::ResetSliderTransforms(TManagedArray<FTransform>& ExplodedTransforms, TManagedArray<FTransform>& Transforms)
{
	for (int Element = 0; Element < Transforms.Num(); Element++)
	{
		Transforms[Element] = ExplodedTransforms[Element];
	}
}

void FGeometryCollectionClusteringUtility::DeleteNodesInHierarchy(FGeometryCollection* GeometryCollection, TArray<int32>& NodesToDelete)
{
	check(GeometryCollection);
	check(NodesToDelete.Num() > 0);
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	TManagedArray<FTransform>& Transforms = *GeometryCollection->Transform;
	TManagedArray<int32>& BoneMap = *GeometryCollection->BoneMap;

	TSharedRef<TManagedArray<FVector> > ExplodedVectorsArray = GeometryCollection->GetAttribute<FVector>("ExplodedVector", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > ExplodedTransformsArray = GeometryCollection->GetAttribute<FTransform>("ExplodedTransform", FGeometryCollection::TransformGroup);
	TManagedArray<FVector>& ExplodedVectors = *ExplodedVectorsArray;
	TManagedArray<FTransform>& ExplodedTransforms = *ExplodedTransformsArray;

	// need to start deleting from the bottom, don't want to swap something to the top that is also to be deleted
	NodesToDelete.Sort(); 
	int32 OriginalSize = Hierarchy.Num();
	int32 DeletedNumber = 0;
	for (int32 Index = NodesToDelete.Num()-1; Index >= 0; Index--)
	{
		int32 Element = NodesToDelete[Index];
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
			// add child at new swapped position 
			Hierarchy[ParentElement].Children.Add(Element);
		}

		if (SwapElement != Element)
		{
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

	}

	if (DeletedNumber > 0)
	{
		// now resize the Transform group
		int NewSize = OriginalSize - DeletedNumber;

		TArray<int32> ElementList;
		for (int32 i = NewSize; i < OriginalSize; i++)
		{
			ElementList.Push(i);
		}

		GeometryCollection->RemoveElements(FGeometryCollection::TransformGroup, ElementList);
	}

}

bool FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(FGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
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


void FGeometryCollectionClusteringUtility::GetRootBones(const FGeometryCollection* GeometryCollection, TArray<int32>& RootBonesOut)
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

bool FGeometryCollectionClusteringUtility::IsARootBone(const FGeometryCollection* GeometryCollection, int32 InBone)
{
	check(GeometryCollection);
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

	return (Hierarchy[InBone].Parent == FGeometryCollectionBoneNode::InvalidBone);
}

void FGeometryCollectionClusteringUtility::GetClusteredBonesWithCommonParent(const FGeometryCollection* GeometryCollection, int32 SourceBone, TArray<int32>& BonesOut)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
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

void FGeometryCollectionClusteringUtility::GetChildBonesFromLevel(const FGeometryCollection* GeometryCollection, int32 SourceBone, int32 Level, TArray<int32>& BonesOut)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
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

int32 FGeometryCollectionClusteringUtility::GetParentOfBoneAtSpecifiedLevel(const FGeometryCollection* GeometryCollection, int32 SourceBone, int32 Level)
{
	check(GeometryCollection);
	const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
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

void FGeometryCollectionClusteringUtility::RecursivelyUpdateChildBoneNames(int32 BoneIndex, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, TManagedArray<FString>& BoneNames, bool OverrideBoneNames /*= false*/)
{
	check(BoneIndex < Hierarchy.Num());

	if (Hierarchy[BoneIndex].Children.Num() > 0)
	{
		const FString& ParentName = BoneNames[BoneIndex];
		int DisplayIndex = 1;
		for (int32 ChildIndex : Hierarchy[BoneIndex].Children)
		{
			TCHAR ChunkNumberStr[5] = { 0 };
			FString NewName;
			int32 FoundIndex=0;
			FCString::Sprintf(ChunkNumberStr, TEXT("_%03d"), DisplayIndex++);

			// enable this if we don't want to override the child names with parent names
			bool HasExistingName = BoneNames[ChildIndex].FindChar('_', FoundIndex);

			if (!OverrideBoneNames && HasExistingName && FoundIndex > 0)
			{
				FString CurrentName = BoneNames[ChildIndex].Left(FoundIndex);

				int32 FoundNumberIndex = 0;
				bool ParentHasNumbers = ParentName.FindChar('_', FoundNumberIndex);
				if (ParentHasNumbers && FoundNumberIndex > 0)
				{
					FString ParentNumbers = ParentName.Right(ParentName.Len() - FoundNumberIndex);
					NewName = CurrentName + ParentNumbers + ChunkNumberStr;
				}
				else
				{
					NewName = CurrentName + ChunkNumberStr;
				}
			}
			else
			{
				NewName = ParentName + ChunkNumberStr;
			}
			BoneNames[ChildIndex] = NewName;
			RecursivelyUpdateChildBoneNames(ChildIndex, Hierarchy, BoneNames, OverrideBoneNames);
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

void FGeometryCollectionClusteringUtility::CollapseLevelHierarchy(int8 Level, FGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);
	TSharedRef<TManagedArray<FGeometryCollectionBoneNode> >  HierarchyArray = GeometryCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
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

		if (Elements.Num() > 0)
		{
			ClusterBonesUnderExistingRoot(GeometryCollection, Elements);
		}
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
		if (Elements.Num() > 0)
		{
			CollapseHierarchyOneLevel(GeometryCollection, Elements);
		}
	}

}

void FGeometryCollectionClusteringUtility::CollapseSelectedHierarchy(int8 Level, const TArray<int32>& SelectedBones, FGeometryCollection* GeometryCollection)
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

void FGeometryCollectionClusteringUtility::ValidateResults(FGeometryCollection* GeometryCollection)
{
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	const TManagedArray<FString>& BoneNames = *GeometryCollection->BoneName;

	// there should only ever be one root node
	int NumRootNodes = 0;
	for (int i = 0; i < Hierarchy.Num(); i++)
	{
		if (Hierarchy[i].Parent == FGeometryCollectionBoneNode::InvalidBone)
		{
			NumRootNodes++;
		}
	}
	check(NumRootNodes == 1);

	// only leaf nodes should be marked as geometry nodes and all others are marked as transform nodes
	for (int BoneIndex = 0; BoneIndex < Hierarchy.Num(); BoneIndex++)
	{
		check((Hierarchy[BoneIndex].Children.Num() > 0) == Hierarchy[BoneIndex].IsTransform());
	}

}

void FGeometryCollectionClusteringUtility::ContextBasedClusterSelection(
	FGeometryCollection* GeometryCollection,
	int ViewLevel,
	const TArray<int32>& SelectedComponentBonesIn,
	TArray<int32>& SelectedComponentBonesOut,
	TArray<int32>& HighlightedComponentBonesOut)
{
	HighlightedComponentBonesOut.Empty();
	SelectedComponentBonesOut.Empty();

	for (int32 BoneIndex : SelectedComponentBonesIn)
	{
		TArray <int32> SelectionHighlightedBones;
		if (ViewLevel == -1)
		{
			SelectionHighlightedBones.AddUnique(BoneIndex);
			SelectedComponentBonesOut.AddUnique(BoneIndex);
		}
		else
		{
			// select all children under bone as selected hierarchy level
			int32 ParentBoneIndex = GetParentOfBoneAtSpecifiedLevel(GeometryCollection, BoneIndex, ViewLevel);
			if (ParentBoneIndex != FGeometryCollectionBoneNode::InvalidBone)
			{
				SelectedComponentBonesOut.AddUnique(ParentBoneIndex);
			}
			else
			{
				SelectedComponentBonesOut.AddUnique(BoneIndex);
			}

			for (int32 Bone : SelectedComponentBonesOut)
			{
				GetChildBonesFromLevel(GeometryCollection, Bone, ViewLevel, SelectionHighlightedBones);
			}
		}

		HighlightedComponentBonesOut.Append(SelectionHighlightedBones);
	}

}

void FGeometryCollectionClusteringUtility::GetLeafBones(FGeometryCollection* GeometryCollection, int BoneIndex, TArray<int32>& LeafBonesOut)
{
	const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
	if (Hierarchy[BoneIndex].Children.Num() > 0)
	{
		for (int32 ChildElement : Hierarchy[BoneIndex].Children)
		{
			GetLeafBones(GeometryCollection, ChildElement, LeafBonesOut);
		}
	}
	else
	{
		LeafBonesOut.Push(BoneIndex);
	}

}

