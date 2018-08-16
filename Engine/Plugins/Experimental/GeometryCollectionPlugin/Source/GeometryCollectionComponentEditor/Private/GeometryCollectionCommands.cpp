// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionCommands.h"

#include "Engine/Selection.h"
#include "GeometryCollection.h"
#include "GeometryCollectionActor.h"
#include "GeometryCollectionAlgo.h"
#include "GeometryCollectionComponent.h"
#include "GeometryCollectionClusteringUtility.h"
#include "Logging/LogMacros.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionCommandsLogging, Log, All);

void FGeometryCollectionCommands::ToString(UWorld * World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				const UGeometryCollection* RestCollection = Actor->GetGeometryCollectionComponent()->GetRestCollection();
				GeometryCollectionAlgo::PrintParentHierarchy(RestCollection);
			}
		}
	}
}

int32 FGeometryCollectionCommands::EnsureSingleRoot(UGeometryCollection* RestCollection)
{
	TManagedArray<FTransform>& Transform = *RestCollection->Transform;
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *RestCollection->BoneHierarchy;
	int32 NumElements = RestCollection->NumElements(UGeometryCollection::TransformGroup);
	if (GeometryCollectionAlgo::HasMultipleRoots(RestCollection))
	{
		TArray<int32> RootIndices;
		for (int32 Index = 0; Index < NumElements; Index++)
		{
			if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
			{
				RootIndices.Add(Index);
			}
		}
		int32 RootIndex = RestCollection->AddElements(1, UGeometryCollection::TransformGroup);
		Transform[RootIndex].SetTranslation(GeometryCollectionAlgo::AveragePosition(RestCollection,RootIndices));
		GeometryCollectionAlgo::ParentTransforms(RestCollection, RootIndex, RootIndices);
		return RootIndex;
	}
	else
	{
		for (int32 Index = 0; Index < NumElements; Index++)
		{
			if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
			{
				return Index;
			}
		}
	}
	check(false);
	return -1;
}



void SplitAcrossYZPlaneRecursive(uint32 RootIndex, const FTransform & ParentTransform, UGeometryCollection* Collection)
{
	TSet<uint32> RootIndices;
	TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *Collection->BoneHierarchy;
	TManagedArray<FTransform>& Transform = *Collection->Transform;

	TArray<int32> SelectedBonesA, SelectedBonesB;
	for (auto& ChildIndex : Hierarchy[RootIndex].Children)
	{
		if (Hierarchy[ChildIndex].Children.Num())
		{
			SplitAcrossYZPlaneRecursive(ChildIndex, ParentTransform, Collection);
		}

		FVector Translation = (Transform[ChildIndex]*ParentTransform).GetTranslation();
		UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("... (%3.5f,%3.5f,%3.5f)"), Translation.X, Translation.Y, Translation.Z);

		if (Translation.X > 0.f)
		{
			SelectedBonesA.Add(ChildIndex);
		}
		else
		{
			SelectedBonesB.Add(ChildIndex);
		}
	}

	if (SelectedBonesB.Num() && SelectedBonesA.Num())
	{
		int32 BoneAIndex = Collection->AddElements(1, UGeometryCollection::TransformGroup);
		GeometryCollectionAlgo::ParentTransform(Collection, RootIndex, BoneAIndex);
		Transform[BoneAIndex].SetTranslation(GeometryCollectionAlgo::AveragePosition(Collection, SelectedBonesA));
		GeometryCollectionAlgo::ParentTransforms(Collection, BoneAIndex, SelectedBonesA);

		int32 BoneBIndex = Collection->AddElements(1, UGeometryCollection::TransformGroup);
		GeometryCollectionAlgo::ParentTransform(Collection, RootIndex, BoneBIndex);
		Transform[BoneBIndex].SetTranslation(GeometryCollectionAlgo::AveragePosition(Collection, SelectedBonesB));
		GeometryCollectionAlgo::ParentTransforms(Collection, BoneBIndex, SelectedBonesB);
	}
}


void FGeometryCollectionCommands::SplitAcrossYZPlane(UWorld * World)
{
	UE_LOG(UGeometryCollectionCommandsLogging, Log, TEXT("FGeometryCollectionCommands::SplitAcrossXZPlane"));
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			if (AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(*Iter))
			{
				FGeometryCollectionEdit RestCollectionEdit = Actor->GetGeometryCollectionComponent()->EditRestCollection();
				UGeometryCollection* RestCollection = RestCollectionEdit.GetRestCollection();

				FGeometryCollectionCommands::EnsureSingleRoot(RestCollection);

				TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *RestCollection->BoneHierarchy;
				for (int32 Index = 0; Index < Hierarchy.Num(); Index++)
				{
					if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
					{
						SplitAcrossYZPlaneRecursive(Index, Actor->GetTransform(), RestCollection);
					}
				}
			}
		}
	}
}

