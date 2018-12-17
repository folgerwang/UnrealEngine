// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FTransformCollection methods.
=============================================================================*/

#include "GeometryCollection/TransformCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

DEFINE_LOG_CATEGORY_STATIC(FTransformCollectionLogging, NoLogging, All);
const FName FTransformCollection::TransformGroup = "Transform";

FTransformCollection::FTransformCollection()
	: FManagedArrayCollection()
	, Transform(new TManagedArray<FTransform>())
	, BoneName(new TManagedArray<FString>())
	, BoneHierarchy(new TManagedArray<FGeometryCollectionBoneNode>())
	, BoneColor(new TManagedArray<FLinearColor>())
{
	Construct();
}

FTransformCollection::FTransformCollection(FTransformCollection& TransformCollectionIn)
	: FManagedArrayCollection(TransformCollectionIn)
	, Transform(TransformCollectionIn.Transform)
	, BoneName(TransformCollectionIn.BoneName)
	, BoneHierarchy(TransformCollectionIn.BoneHierarchy)
	, BoneColor(TransformCollectionIn.BoneColor)
{
}

void FTransformCollection::Construct()
{
	FManagedArrayCollection::FConstructionParameters TransformDependency(FTransformCollection::TransformGroup);

	// Hierarchy Group
	AddAttribute<FTransform>("Transform", FTransformCollection::TransformGroup, Transform);
	AddAttribute<FString>("BoneName", FTransformCollection::TransformGroup, BoneName);
	AddAttribute<FLinearColor>("BoneColor", FTransformCollection::TransformGroup, BoneColor);
	AddAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FTransformCollection::TransformGroup, BoneHierarchy, TransformDependency);
}

int32 FTransformCollection::AppendTransform(const FTransformCollection & Element)
{
	check(Element.NumElements(FTransformCollection::TransformGroup) == 1);
	const TManagedArray<FTransform>& ElementTransform = *Element.Transform;
	const TManagedArray<FString>& ElementBoneName = *Element.BoneName;
	const TManagedArray<FLinearColor>& ElementBoneColor = *Element.BoneColor;
	const TManagedArray<FGeometryCollectionBoneNode>& ElementBoneHierarchy = *Element.BoneHierarchy;


	// we are adding just one new piece of geometry, @todo - add general append support ?
	int ParticleIndex = AddElements(1, FTransformCollection::TransformGroup);
	TManagedArray<FTransform>& Transforms = *Transform;
	Transforms[ParticleIndex] = ElementTransform[0];
	TManagedArray<FString>& BoneNames = *BoneName;
	BoneNames[ParticleIndex] = ElementBoneName[0];
	TManagedArray<FLinearColor>& BoneColors = *BoneColor;
	BoneColors[ParticleIndex] = ElementBoneColor[0];
	TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchys = *BoneHierarchy;
	BoneHierarchys[ParticleIndex] = ElementBoneHierarchy[0];
	return NumElements(FTransformCollection::TransformGroup) - 1;
}

void FTransformCollection::RelativeTransformation( const int32& Index, const FTransform& LocalOffset)
{
	if (ensureMsgf(Index < NumElements(FTransformCollection::TransformGroup), TEXT("Index out of range.")))
	{
		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *BoneHierarchy;
		TManagedArray<FTransform>& TransformArray = *Transform;

		if (Hierarchy[Index].Children.Num())
		{
			FTransform LocalOffsetInverse = LocalOffset.Inverse();
			for (int32 Child : Hierarchy[Index].Children)
			{
				TransformArray[Child] = TransformArray[Child] * LocalOffset.Inverse();
			}
		}
		TransformArray[Index] = LocalOffset * TransformArray[Index];
	}
}

void FTransformCollection::RemoveElements(const FName & Group, const TArray<int32> & SortedDeletionList)
{
	if (SortedDeletionList.Num())
	{
		GeometryCollectionAlgo::ValidateSortedList(SortedDeletionList, NumElements(Group));
		if (Group == FTransformCollection::TransformGroup)
		{
			TArray<int32> Mask;
			int32 MaskSize = NumElements(FTransformCollection::TransformGroup);
			GeometryCollectionAlgo::BuildIncrementMask(SortedDeletionList, MaskSize, Mask);

			TManagedArray<FGeometryCollectionBoneNode>&  Bones = *BoneHierarchy;
			TManagedArray<FTransform>&  LocalTransform = *Transform;
			for (int32 sdx = 0; sdx < SortedDeletionList.Num(); sdx++)
			{
				TArray<FTransform> GlobalTransform;
				GeometryCollectionAlgo::GlobalMatrices(this, GlobalTransform);

				int32 Index = SortedDeletionList[sdx];
				ensure(0 <= Index && Index < Bones.Num());

				FGeometryCollectionBoneNode & Node = Bones[Index];
				int32 ParentID = Node.Parent;
				ensure(ParentID < Bones.Num());

				for (int32 ChildID : Node.Children)
				{
					FTransform ParentTransform = FTransform::Identity;

					Bones[ChildID].Parent = Node.Parent;
					if (ParentID >= 0)
					{
						ensure(!Bones[ParentID].Children.Find(ChildID));
						Bones[ParentID].Children.Add(ChildID);
						ParentTransform = GlobalTransform[ParentID].Inverse();
					}

					LocalTransform[ChildID] = ParentTransform * GlobalTransform[ChildID];
				}

				if (0 <= ParentID)
				{
					Bones[ParentID].Children.Remove(Index);
				}
			}
		}

		Super::RemoveElements(Group, SortedDeletionList);
	}
}


void  FTransformCollection::Initialize(FManagedArrayCollection & CollectionIn)
{
	Super::Initialize(CollectionIn);
}

void  FTransformCollection::BindSharedArrays()
{
	Super::BindSharedArrays();

	Transform = ShareAttribute<FTransform>("Transform", TransformGroup);
	BoneName = ShareAttribute<FString>("BoneName", TransformGroup);
	BoneColor = ShareAttribute<FLinearColor>("BoneColor", TransformGroup);
	BoneHierarchy = ShareAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", TransformGroup);
}

