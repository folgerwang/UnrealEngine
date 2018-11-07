// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: UGeometryCollection methods.
=============================================================================*/

#include "GeometryCollectionAlgo.h"
#include "GeometryCollectionBoneNode.h"
#include "Async/ParallelFor.h"

DEFINE_LOG_CATEGORY_STATIC(GeometryCollectionAlgoLog, Log, All);

namespace GeometryCollectionAlgo
{

	void PrintParentHierarchyRecursive(int32 Index
		, const TManagedArray<FTransform>& Transform
		, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy
		, const TManagedArray<FString>& BoneName
		, int8 Tab = 0
	)
	{
		check(Index >= 0);
		check(Index < Transform.Num());
		FString Buffer;
		Buffer += FString::Printf(TEXT("(%+6.2f,%+6.2f,%+6.2f)"), Transform[Index].GetTranslation().X, 
			Transform[Index].GetTranslation().Y, Transform[Index].GetTranslation().Z);
		for (int Tdx = 0; Tdx < Tab; Tdx++)
			Buffer += " ";
		Buffer += FString::Printf(TEXT("[%d] Name : '%s'  %s"), Index, *BoneName[Index], *Hierarchy[Index].ToString());
		UE_LOG(GeometryCollectionAlgoLog, Log, TEXT("%s"), *Buffer);

		for (auto& ChildIndex : Hierarchy[Index].Children)
		{
			PrintParentHierarchyRecursive(ChildIndex, Transform, Hierarchy, BoneName, Tab + 3);
		}
	}


	void PrintParentHierarchy(const UGeometryCollection * Collection)
	{
		check(Collection);

		const TSharedRef< TManagedArray<FTransform> > TransformArray = Collection->GetAttribute<FTransform>("Transform", UGeometryCollection::TransformGroup);
		const TManagedArray<FTransform>& Transform = *TransformArray;

		const TSharedRef<TManagedArray<FString> > BoneNamesArray = Collection->GetAttribute<FString>("BoneName", UGeometryCollection::TransformGroup);
		const TManagedArray<FString>& BoneName = *BoneNamesArray;

		const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> > HierarchyArray = Collection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", UGeometryCollection::TransformGroup);
		const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

		int32 NumParticles = Collection->NumElements(UGeometryCollection::TransformGroup);
		for (int32 Index = 0; Index < NumParticles; Index++)
		{
			if (Hierarchy[Index].Parent == FGeometryCollectionBoneNode::InvalidBone)
			{
				PrintParentHierarchyRecursive(Index, Transform, Hierarchy, BoneName);
			}
		}
	}

	TSharedRef<TArray<int32> > ContiguousArray(int32 Length)
	{
		TArray<int32> * Array = new TArray<int32>();
		Array->SetNumUninitialized(Length);
		ParallelFor(Length, [&](int32 Idx)
		{
			Array->operator[](Idx) = Idx;
		});
		return TSharedRef<TArray<int32> >(Array);
	}


	FVector AveragePosition(UGeometryCollection* Collection, const TArray<int32>& Indices)
	{
		TManagedArray<FTransform>& Transform = *Collection->Transform;
		int32 NumIndices = Indices.Num();

		FVector Translation(0);
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			Translation += Transform[ Indices[Index] ].GetTranslation();
		}
		if (NumIndices > 1)
		{
			Translation /= NumIndices;
		}
		return Translation;
	}

	bool HasMultipleRoots(UGeometryCollection * Collection)
	{
		int32 ParentCount = 0;
		TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchy = *Collection->BoneHierarchy;
		for (int32 i = 0; i < BoneHierarchy.Num(); i++)
		{
			if (BoneHierarchy[i].Parent == FGeometryCollectionBoneNode::InvalidBone) ParentCount++;
			if (ParentCount > 1) return true;
		}
		return false;
	}

	void ParentTransform(UGeometryCollection* GeometryCollection, const int32 InsertAtIndex, const int32 ChildIndex)
	{
		TArray<int32> SelectedBones;
		SelectedBones.Add(ChildIndex);
		ParentTransforms(GeometryCollection, InsertAtIndex, SelectedBones);
	}

	void ParentTransforms(UGeometryCollection* GeometryCollection, const int32 InsertAtIndex, const TArray<int32>& SelectedBones)
	{
		check(GeometryCollection!=nullptr);

		TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;
		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
		if (ensure(-1 <= InsertAtIndex&&InsertAtIndex < Hierarchy.Num()))
		{
			// pre calculate global positions
			TArray<FTransform> GlobalTransform;
			GeometryCollectionAlgo::GlobalMatrices(GeometryCollection, GlobalTransform);

			// append children 
			for (int32 Index = 0; Index < SelectedBones.Num(); Index++) {
				int32 BoneIndex = SelectedBones[Index];
				if (ensure(0 <= BoneIndex&&BoneIndex < Hierarchy.Num()))
				{
					// remove entry in previous parent
					int32 ParentIndex = Hierarchy[BoneIndex].Parent;
					if (ParentIndex != FGeometryCollectionBoneNode::InvalidBone)
					{
						if (ensure(0 <= ParentIndex&&ParentIndex < Hierarchy.Num()))
						{
							Hierarchy[ParentIndex].Children.Remove(BoneIndex);
						}
					}

					// set new parent
					Hierarchy[BoneIndex].Parent = InsertAtIndex;
				}
			}

			FTransform ParentInverse = FTransform::Identity;
			if (InsertAtIndex != -1)
			{
				Hierarchy[InsertAtIndex].Children.Append(SelectedBones);
				ParentInverse = GlobalTransform[InsertAtIndex].Inverse();
			}
			// move the children to the local space of the transform. 
			for (int32 Index = 0; Index < SelectedBones.Num(); Index++) 
			{
				int32 BoneIndex = SelectedBones[Index];
				Transform[BoneIndex] = ParentInverse*GlobalTransform[BoneIndex];
			}

		}
	}

	struct CacheElement
	{
		CacheElement() :Processed(false), Global() {}
		bool Processed;
		FTransform Global;
	};

	FTransform GlobalMatricesRecursive(const int32 & Index, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const TManagedArray<FTransform>& Transform, TArray<CacheElement>& AccelerationStructure)
	{
		if (AccelerationStructure[Index].Processed)
			return AccelerationStructure[Index].Global;

		FTransform Result = Transform[Index];
		if ( Hierarchy[Index].Parent != FGeometryCollectionBoneNode::InvalidBone )
			Result =Result*GlobalMatricesRecursive( Hierarchy[Index].Parent, Hierarchy, Transform, AccelerationStructure);

		AccelerationStructure[Index].Global = Result;
		AccelerationStructure[Index].Processed = true;
		return AccelerationStructure[Index].Global;
	}

	void GlobalMatrices(UGeometryCollection* GeometryCollection, const TArray<int32>& Indices, TArray<FTransform> & Transforms)
	{
		TArray<CacheElement> AccelerationStructures;
		AccelerationStructures.AddDefaulted(GeometryCollection->NumElements(UGeometryCollection::TransformGroup));

		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
		TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;

		Transforms.SetNumUninitialized(Indices.Num(), false);
		ParallelFor(Indices.Num(), [&](int32 Index)
		{
			Transforms[Index] = GlobalMatricesRecursive(Index, Hierarchy, Transform, AccelerationStructures);
		});
	}

	void GlobalMatrices(UGeometryCollection* GeometryCollection, TArray<FTransform> & Transforms)
	{
		TArray<CacheElement> AccelerationStructures;
		AccelerationStructures.AddDefaulted(GeometryCollection->NumElements(UGeometryCollection::TransformGroup));

		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;
		TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;

		Transforms.SetNumUninitialized(Transform.Num(), false);
		ParallelFor(Transform.Num(), [&](int32 Index)
		{
			Transforms[Index] = GlobalMatricesRecursive(Index, Hierarchy, Transform, AccelerationStructures);
		});
	}


	void PrepareForSimulation(UGeometryCollection* GeometryCollection)
	{

		TManagedArray<int32> & BoneMap = *GeometryCollection->BoneMap;
		TManagedArray<FVector> & Vertex = *GeometryCollection->Vertex;
		TManagedArray<FTransform> & Transform = *GeometryCollection->Transform;
		TManagedArray<FGeometryCollectionBoneNode> & BoneHierarchy = *GeometryCollection->BoneHierarchy;

		TArray<int32> SurfaceParticlesCount;
		SurfaceParticlesCount.AddZeroed(GeometryCollection->NumElements(UGeometryCollection::TransformGroup));

		TArray<FVector> CenterOfMass;
		CenterOfMass.AddZeroed(GeometryCollection->NumElements(UGeometryCollection::TransformGroup));

		for (int i = 0; i < Vertex.Num(); i++)
		{
			int32 ParticleIndex = BoneMap[i];
			SurfaceParticlesCount[ParticleIndex]++;
			CenterOfMass[ParticleIndex] += Vertex[i];
		}

		for (int i = 0; i < Transform.Num(); i++)
		{
			if (SurfaceParticlesCount[i])
			{
				CenterOfMass[i] /=  SurfaceParticlesCount[i];
				Transform[i].SetTranslation(Transform[i].GetTranslation()+CenterOfMass[i]);
			}
		}

		for (int i = 0; i < Vertex.Num(); i++)
		{
			int32 ParticleIndex = BoneMap[i];
			Vertex[i] -= CenterOfMass[ParticleIndex];
		}
	}

}