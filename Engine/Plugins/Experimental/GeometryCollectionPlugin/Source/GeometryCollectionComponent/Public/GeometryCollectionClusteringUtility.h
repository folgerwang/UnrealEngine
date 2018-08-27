// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ManagedArrayCollection.h"

class UGeometryCollection;

class GEOMETRYCOLLECTIONCOMPONENT_API FGeometryCollectionClusteringUtility
{
public:
	/**
	* Creates a cluster in the node hierarchy by re-parenting the Selected Bones off a new node in the hierarchy
	* It makes more sense to think that the Selected Bones are all at the same level in the hierarchy however
	* it will re-parent multiple levels at the InsertAtIndex location bone
	*
	* e.g. if you have a flat chunk hierarchy after performing Voronoi fracturing
	*   L0         Root
	*               |
	*          ----------
	*          |  |  |  |
	*   L1     A  B  C  D
	*
	* Cluster A & B at insertion point A, results in
	*   L0         Root
	*               |
	*          ----------
	*          |     |  |
	*   L1     E     C  D
	*          |
	*         ----
	*         |  |
	*   L2    A  B
	*
	* Node E has no geometry of its own, only a transform by which to control A & B as a single unit
	*/
	static void ClusterBonesUnderNewNode(UGeometryCollection* GeometryCollection, int32 InsertAtIndex, const TArray<int32>& SelectedBones, bool CalcNewLocalTransform);

	/** Cluster all existing bones under a new root node, so there is now only one root node and a completely flat hierarchy underneath it */
	static void ClusterAllBonesUnderNewRoot(UGeometryCollection* GeometryCollection);

	static void DeleteNodesInHierarchy(UGeometryCollection* GeometryCollection, TArray<int32>& NodesToDelete);

	/** Returns true if bone hierarchy contains more than one root node */
	static bool ContainsMultipleRootBones(UGeometryCollection* GeometryCollection);

	/** Finds the root bone in the hierarchy, the one with an invalid parent bone index */
	static void GetRootBones(const UGeometryCollection* GeometryCollection, TArray<int32>& RootBonesOut);
	
	/** Finds all Bones in same cluster as the one specified */
	static void GetClusteredBonesWithCommonParent(const UGeometryCollection* GeometryCollection, int32 SourceBone, TArray<int32>& BonesOut);
	
	/** Get list of child bones down from the source bone below the specified hierarchy level */
	static void GetChildBonesFromLevel(const UGeometryCollection* GeometryCollection, int32 SourceBone, int32 Level, TArray<int32>& BonesOut);

	/** Recursively Add all children to output bone list from source bone down to the leaf nodes */
	static void RecursiveAddAllChildren(const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, int32 SourceBone, TArray<int32>& BonesOut);

	/** Search hierarchy for the parent of the specified bone, where the parent exists at the given level in the hierarchy */
	static int32 GetParentOfBoneAtSpecifiedLevel(const UGeometryCollection* GeometryCollection, int32 SourceBone, int32 Level);

	/**
	* Maintains the bone naming convention of
	*  Root "Name"
	*  Level 1 "Name_001", "Name_0002", ...
	*  Level 2 children of "Name_0001" are "Name_0001_0001", "Name_0001_0002",.. etc
	* from the given bone index down through the hierarchy to the leaf nodes
	*/
	static void RecursivelyUpdateChildBoneNames(int32 BoneIndex, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, TManagedArray<FString>& BoneNames);
	
	/** Recursively update the hierarchy level of all the children below this bone */
	static void RecursivelyUpdateHierarchyLevelOfChildren(TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, int32 ParentElement);
	static void CollapseLevelHierarchy(int8 Level, UGeometryCollection* GeometryCollection);
	static void CollapseSelectedHierarchy(int8 Level, const TArray<int32>& SelectedBones, UGeometryCollection* GeometryCollection);
	static void ClusterBonesUnderExistingRoot(UGeometryCollection* GeometryCollection, TArray<int32>& SourceElements);
	static void CollapseHierarchyOneLevel(UGeometryCollection* GeometryCollection, TArray<int32>& SourceElements);
};
