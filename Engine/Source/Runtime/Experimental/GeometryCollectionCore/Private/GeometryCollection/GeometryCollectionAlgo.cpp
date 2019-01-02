// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"
#include "GeometryCollection/RecordedTransformTrack.h"
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

		UE_LOG(GeometryCollectionAlgoLog, Verbose, TEXT("%s"), *Buffer);

		for (auto& ChildIndex : Hierarchy[Index].Children)
		{
			PrintParentHierarchyRecursive(ChildIndex, Transform, Hierarchy, BoneName, Tab + 3);
		}
	}


	void PrintParentHierarchy(const FGeometryCollection * Collection)
	{
		check(Collection);

		const TSharedRef< TManagedArray<FTransform> > TransformArray = Collection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
		const TManagedArray<FTransform>& Transform = *TransformArray;

		const TSharedRef<TManagedArray<FString> > BoneNamesArray = Collection->GetAttribute<FString>("BoneName", FGeometryCollection::TransformGroup);
		const TManagedArray<FString>& BoneName = *BoneNamesArray;

		const TSharedRef<TManagedArray<FGeometryCollectionBoneNode> > HierarchyArray = Collection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);
		const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *HierarchyArray;

		int32 NumParticles = Collection->NumElements(FGeometryCollection::TransformGroup);
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

	void BuildIncrementMask(const TArray<int32> & SortedDeletionList, const int32 & Size, TArray<int32> & Mask)
	{
		Mask.SetNumUninitialized(Size);
		for (int Index = 0, DelIndex = 0; Index < Size; Index++)
		{
			if (DelIndex < SortedDeletionList.Num() && Index == SortedDeletionList[DelIndex])
			{
				DelIndex++;
			}
			Mask[Index] = DelIndex;
		}
	}

	void BuildLookupMask(const TArray<int32> & SortedDeletionList, const int32 & Size, TArray<bool> & Mask)
	{
		Mask.Init(false, Size);
		for (int Index = 0; Index < SortedDeletionList.Num(); Index++)
		{
			if (SortedDeletionList[Index] < Size)
				Mask[SortedDeletionList[Index]] = true;
			else
				break;
		}
	}


	void BuildTransformGroupToGeometryGroupMap(const FGeometryCollection& GeometryCollection, TArray<int32> & TransformToGeometry)
	{
		int32 NumGeometryGroup = GeometryCollection.NumElements(FGeometryCollection::GeometryGroup);
		const TManagedArray<int32>& TransformIndex = *GeometryCollection.TransformIndex;
		TransformToGeometry.Init(FGeometryCollection::Invalid, GeometryCollection.NumElements(FGeometryCollection::TransformGroup));
		for (int32 i = 0; i < NumGeometryGroup; i++)
		{
			check(TransformIndex[i] != FGeometryCollection::Invalid);
			TransformToGeometry[TransformIndex[i]] = i;
		}
	}


	void BuildFaceGroupToGeometryGroupMap(const FGeometryCollection& GeometryCollection, const TArray<int32>& TransformToGeometryMap, TArray<int32> & FaceToGeometry)
	{
		check(TransformToGeometryMap.Num() == GeometryCollection.NumElements(FGeometryCollection::TransformGroup));
		const TManagedArray<FIntVector>& Indices = *GeometryCollection.Indices;
		const TManagedArray<int32>& BoneMap = *GeometryCollection.BoneMap;

		int32 NumTransforms = TransformToGeometryMap.Num();
		int32 NumFaces = GeometryCollection.NumElements(FGeometryCollection::FacesGroup);
		FaceToGeometry.Init(FGeometryCollection::Invalid, NumFaces);
		for (int32 i = 0; i < NumFaces; i++)
		{
			check(0 <= Indices[i][0] && Indices[i][0] < TransformToGeometryMap.Num());
			FaceToGeometry[i] = TransformToGeometryMap[Indices[i][0]];
		}
	}


	void ValidateSortedList(const TArray<int32>&SortedDeletionList, const int32 & ListSize)
	{
		int32 PreviousValue = -1;
		int32 DeletionListSize = SortedDeletionList.Num();
		if (DeletionListSize)
		{
			ensureMsgf(DeletionListSize != 0, TEXT("TManagedArray::NewCopy( DeletionList ) DeletionList empty"));
			ensureMsgf(DeletionListSize <= ListSize, TEXT("TManagedArray::NewCopy( DeletionList ) DeletionList larger than array"));
			for (int32 Index = 0; Index < SortedDeletionList.Num(); Index++)
			{
				ensureMsgf(PreviousValue < SortedDeletionList[Index], TEXT("TManagedArray::NewCopy( DeletionList ) DeletionList not sorted"));
				ensureMsgf(0 <= SortedDeletionList[Index] && SortedDeletionList[Index] < ListSize, TEXT("TManagedArray::NewCopy( DeletionList ) Index out of range"));
				PreviousValue = SortedDeletionList[Index];
			}
		}
	}


	FVector AveragePosition(FGeometryCollection* Collection, const TArray<int32>& Indices)
	{
		TManagedArray<FTransform>& Transform = *Collection->Transform;
		int32 NumIndices = Indices.Num();

		FVector Translation(0);
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			Translation += Transform[Indices[Index]].GetTranslation();
		}
		if (NumIndices > 1)
		{
			Translation /= NumIndices;
		}
		return Translation;
	}

	bool HasMultipleRoots(FGeometryCollection * Collection)
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

	bool HasCycleRec(TManagedArray<FGeometryCollectionBoneNode> & Hierarchy, int32 Node, TArray<bool> Visited)
	{
		ensure(0 <= Node && Node < Visited.Num());

		if (Visited[Node])
			return true;
		Visited[Node] = true;

		if (Hierarchy[Node].Parent != FGeometryCollection::Invalid)
		{
			return HasCycleRec(Hierarchy, Hierarchy[Node].Parent, Visited);
		}
		return false;
	}
	bool HasCycle(TManagedArray<FGeometryCollectionBoneNode> & Hierarchy, int32 Node)
	{
		TArray<bool> Visited;
		Visited.Init(false, Hierarchy.Num());
		return HasCycleRec(Hierarchy, Node, Visited);
	}
	bool HasCycle(TManagedArray<FGeometryCollectionBoneNode> & Hierarchy, const TArray<int32>& SelectedBones)
	{
		bool result = false;
		TArray<bool> Visited;
		Visited.Init(false, Hierarchy.Num());
		for (int32 Index = 0; Index < SelectedBones.Num(); Index++)
		{
			result |= HasCycleRec(Hierarchy, SelectedBones[Index], Visited);
		}
		return result;
	}

	void ParentTransform(FGeometryCollection* GeometryCollection, const int32 TransformIndex, const int32 ChildIndex)
	{
		TArray<int32> SelectedBones;
		SelectedBones.Add(ChildIndex);
		ParentTransforms(GeometryCollection, TransformIndex, SelectedBones);
	}

	void ParentTransforms(FGeometryCollection* GeometryCollection, const int32 TransformIndex,
		const TArray<int32>& SelectedBones)
	{
		check(GeometryCollection != nullptr);

		TManagedArray<FTransform>& Transform = *GeometryCollection->Transform;
		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

		if (ensure(-1 <= TransformIndex && TransformIndex < Hierarchy.Num()))
		{
			// pre calculate global positions
			TArray<FTransform> GlobalTransform;
			GeometryCollectionAlgo::GlobalMatrices(GeometryCollection, GlobalTransform);

			// append children 
			for (int32 Index = 0; Index < SelectedBones.Num(); Index++)
			{
				int32 BoneIndex = SelectedBones[Index];
				if (ensure(0 <= BoneIndex && BoneIndex < Hierarchy.Num()))
				{
					// remove entry in previous parent
					int32 ParentIndex = Hierarchy[BoneIndex].Parent;
					if (ParentIndex != FGeometryCollectionBoneNode::InvalidBone)
					{
						if (ensure(0 <= ParentIndex && ParentIndex < Hierarchy.Num()))
						{
							Hierarchy[ParentIndex].Children.Remove(BoneIndex);
						}
					}

					// set new parent
					Hierarchy[BoneIndex].Parent = TransformIndex;
				}
			}

			FTransform ParentInverse = FTransform::Identity;
			if (TransformIndex != FGeometryCollection::Invalid)
			{
				Hierarchy[TransformIndex].Children.Append(SelectedBones);
				ParentInverse = GlobalTransform[TransformIndex].Inverse();
			}

			// move the children to the local space of the transform. 
			for (int32 Index = 0; Index < SelectedBones.Num(); Index++)
			{
				int32 BoneIndex = SelectedBones[Index];
				Transform[BoneIndex] = GlobalTransform[BoneIndex] * ParentInverse;
			}

		}

		// error check for circular dependencies
		ensure(!HasCycle(Hierarchy, TransformIndex));
		ensure(!HasCycle(Hierarchy, SelectedBones));
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
		if (Hierarchy[Index].Parent != FGeometryCollectionBoneNode::InvalidBone)
			Result = Result * GlobalMatricesRecursive(Hierarchy[Index].Parent, Hierarchy, Transform, AccelerationStructure);

		AccelerationStructure[Index].Global = Result;
		AccelerationStructure[Index].Processed = true;
		return AccelerationStructure[Index].Global;
	}

	FTransform GlobalMatrix(const FTransformCollection* TransformCollection, int32 Index)
	{
		FTransform Transform = FTransform::Identity;

		TManagedArray<FTransform>& Transforms = *TransformCollection->Transform;
		if (0 <= Index && Index < Transforms.Num())
		{
			TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *TransformCollection->BoneHierarchy;

			while (Index != FGeometryCollection::Invalid)
			{
				Transform = Transforms[Index] * Transform;
				Index = Hierarchy[Index].Parent;
			}
		}
		return Transform;
	}

	void GlobalMatrices(const FTransformCollection* TransformCollection, const TArray<int32>& Indices, TArray<FTransform> & Transforms)
	{
		TArray<CacheElement> AccelerationStructures;
		AccelerationStructures.AddDefaulted(TransformCollection->NumElements(FGeometryCollection::TransformGroup));

		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *TransformCollection->BoneHierarchy;
		TManagedArray<FTransform>& Transform = *TransformCollection->Transform;

		Transforms.SetNumUninitialized(Indices.Num(), false);
		for( int Idx=0; Idx<Indices.Num(); Idx++)
		{
			Transforms[Indices[Idx]] = GlobalMatricesRecursive(Indices[Idx], Hierarchy, Transform, AccelerationStructures);
		}
	}

	void GlobalMatrices(const FTransformCollection* TransformCollection, TArray<FTransform> & Transforms)
	{
		TArray<CacheElement> AccelerationStructures;
		AccelerationStructures.AddDefaulted(TransformCollection->NumElements(FGeometryCollection::TransformGroup));

		TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *TransformCollection->BoneHierarchy;
		TManagedArray<FTransform>& Transform = *TransformCollection->Transform;

		Transforms.SetNumUninitialized(Transform.Num(), false);
		for(int Index=0; Index<Transform.Num(); Index++)
		{
			Transforms[Index] = GlobalMatricesRecursive(Index, Hierarchy, Transform, AccelerationStructures);
		}
	}

	void PrepareForSimulation(FGeometryCollection* GeometryCollection, bool CenterAtOrigin/*=true*/)
	{
		check(GeometryCollection);
	}

	DEFINE_LOG_CATEGORY_STATIC(LogGeoemtryCollectionClean, Verbose, All);

	void ComputeCoincidentVertices(const FGeometryCollection* GeometryCollection, const float Tolerance, TMap<int32, int32>& CoincidentVerticesMap, TSet<int32>& VertexToDeleteSet)
	{
		check(GeometryCollection);

		const TManagedArray<FVector>& VertexArray = *GeometryCollection->Vertex;
		const TManagedArray<int32>& BoneMapArray = *GeometryCollection->BoneMap;
		const TManagedArray<int32>& TransformIndexArray = *GeometryCollection->TransformIndex;
		int32 NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
		int32 NumGeometries = GeometryCollection->NumElements(FGeometryCollection::GeometryGroup);

		for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
		{
			int32 TransformIndex = TransformIndexArray[IdxGeometry];
			for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
			{
				if (BoneMapArray[IdxVertex] == TransformIndex)
				{
					if (!VertexToDeleteSet.Contains(IdxVertex))
					{
						FVector Vertex = VertexArray[IdxVertex];
						for (int32 IdxOtherVertex = 0; IdxOtherVertex < NumVertices; ++IdxOtherVertex)
						{
							if (BoneMapArray[IdxOtherVertex] == TransformIndex)
							{
								if (!VertexToDeleteSet.Contains(IdxOtherVertex) && (IdxVertex != IdxOtherVertex))
								{
									FVector OtherVertex = VertexArray[IdxOtherVertex];
									if ((Vertex - OtherVertex).Size() < Tolerance)
									{
										VertexToDeleteSet.Add(IdxOtherVertex);
										CoincidentVerticesMap.Add(IdxOtherVertex, IdxVertex);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	void DeleteCoincidentVertices(FGeometryCollection* GeometryCollection, float Tolerance)
	{
		check(GeometryCollection);

		TSet<int32> VertexToDeleteSet;
		TMap<int32, int32> CoincidentVerticesMap;
		ComputeCoincidentVertices(GeometryCollection, Tolerance, CoincidentVerticesMap, VertexToDeleteSet);

		// Swap VertexIndex in Indices array
		TManagedArray<FIntVector>& IndicesArray = *GeometryCollection->Indices;
		int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
		for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
		{
			if (CoincidentVerticesMap.Contains(IndicesArray[IdxFace].X))
			{
				IndicesArray[IdxFace].X = CoincidentVerticesMap[IndicesArray[IdxFace].X];
			}
			if (CoincidentVerticesMap.Contains(IndicesArray[IdxFace].Y))
			{
				IndicesArray[IdxFace].Y = CoincidentVerticesMap[IndicesArray[IdxFace].Y];
			}
			if (CoincidentVerticesMap.Contains(IndicesArray[IdxFace].Z))
			{
				IndicesArray[IdxFace].Z = CoincidentVerticesMap[IndicesArray[IdxFace].Z];
			}
		}

		// Delete vertices
		TArray<int32> DelList = VertexToDeleteSet.Array();
		DelList.Sort();
		GeometryCollection->RemoveElements(FGeometryCollection::VerticesGroup, DelList);
	}

	void ComputeZeroAreaFaces(const FGeometryCollection* GeometryCollection, const float Tolerance, TSet<int32>& FaceToDeleteSet)
	{
		check(GeometryCollection);

		const TManagedArray<FVector>& VertexArray = *GeometryCollection->Vertex;
		const TManagedArray<FIntVector>& IndicesArray = *GeometryCollection->Indices;
		const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *GeometryCollection->BoneHierarchy;
		const TManagedArray<int32>& BoneMapArray = *GeometryCollection->BoneMap;

		int32 NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
		int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);

		for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
		{
			int32 TransformIndex = BoneMapArray[IndicesArray[IdxFace][0]];
			if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
			{
				FVector Vertex0 = VertexArray[IndicesArray[IdxFace][0]];
				FVector Vertex1 = VertexArray[IndicesArray[IdxFace][1]];
				FVector Vertex2 = VertexArray[IndicesArray[IdxFace][2]];

				float Area = 0.5f * ((Vertex0 - Vertex1) ^ (Vertex0 - Vertex2)).Size();
				if (Area < Tolerance)
				{
					FaceToDeleteSet.Add(IdxFace);
				}
			}
		}
	}

	void DeleteZeroAreaFaces(FGeometryCollection* GeometryCollection, float Tolerance)
	{
		check(GeometryCollection);

		TSet<int32> FaceToDeleteSet;
		ComputeZeroAreaFaces(GeometryCollection, Tolerance, FaceToDeleteSet);

		TArray<int32> DelList = FaceToDeleteSet.Array();
		DelList.Sort();
		GeometryCollection->RemoveElements(FGeometryCollection::FacesGroup, DelList);
	}

	void ComputeHiddenFaces(const FGeometryCollection* GeometryCollection, TSet<int32>& FaceToDeleteSet)
	{
		check(GeometryCollection);

		const TManagedArray<FIntVector>& IndicesArray = *GeometryCollection->Indices;
		const TManagedArray<bool>& VisibleArray = *GeometryCollection->Visible;
		const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *GeometryCollection->BoneHierarchy;
		const TManagedArray<int32>& BoneMapArray = *GeometryCollection->BoneMap;

		int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);

		for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
		{
			int32 TransformIndex = BoneMapArray[IndicesArray[IdxFace][0]];
			if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
			{
				if (!VisibleArray[IdxFace])
				{
					FaceToDeleteSet.Add(IdxFace);
				}
			}
		}
	}

	void DeleteHiddenFaces(FGeometryCollection* GeometryCollection)
	{
		check(GeometryCollection);

		TSet<int32> FaceToDeleteSet;
		ComputeHiddenFaces(GeometryCollection, FaceToDeleteSet);

		TArray<int32> DelList = FaceToDeleteSet.Array();
		DelList.Sort();
		GeometryCollection->RemoveElements(FGeometryCollection::FacesGroup, DelList);
	}

	void ComputeStaleVertices(const FGeometryCollection* GeometryCollection, TSet<int32>& VertexToDeleteSet)
	{
		check(GeometryCollection);

		const TManagedArray<FVector>& VertexArray = *GeometryCollection->Vertex;
		const TManagedArray<int32>& BoneMapArray = *GeometryCollection->BoneMap;
		const TManagedArray<int32>& TransformIndexArray = *GeometryCollection->TransformIndex;
		const TManagedArray<FIntVector>& IndicesArray = *GeometryCollection->Indices;
		const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *GeometryCollection->BoneHierarchy;

		int32 NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
		int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);

		TArray<int32> VertexInFaceArray;
		VertexInFaceArray.Init(0, NumVertices);
		for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
		{
			VertexInFaceArray[IndicesArray[IdxFace].X]++;
			VertexInFaceArray[IndicesArray[IdxFace].Y]++;
			VertexInFaceArray[IndicesArray[IdxFace].Z]++;
		}

		for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
		{
			if (VertexInFaceArray[IdxVertex] == 0)
			{
				VertexToDeleteSet.Add(IdxVertex);
			}
		}
	}

	void DeleteStaleVertices(FGeometryCollection* GeometryCollection)
	{
		check(GeometryCollection);

		TSet<int32> VertexToDeleteSet;
		ComputeStaleVertices(GeometryCollection, VertexToDeleteSet);

		TArray<int32> DelList = VertexToDeleteSet.Array();
		DelList.Sort();
		GeometryCollection->RemoveElements(FGeometryCollection::VerticesGroup, DelList);
	}

	void ComputeEdgeInFaces(const FGeometryCollection* GeometryCollection, TMap<FFaceEdge, int32>& FaceEdgeMap)
	{
		check(GeometryCollection);

		const TManagedArray<FIntVector>& IndicesArray = *GeometryCollection->Indices;

		int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);

		for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
		{
			for (int32 Idx = 0; Idx < 3; Idx++)
			{
				int32 VertexIndex1 = IndicesArray[IdxFace][Idx];
				int32 VertexIndex2 = IndicesArray[IdxFace][(Idx + 1) % 3];
				FFaceEdge Edge{ FMath::Min(VertexIndex1, VertexIndex2),
								FMath::Max(VertexIndex1, VertexIndex2) };
				if (FaceEdgeMap.Contains(Edge))
				{
					FaceEdgeMap[Edge]++;
				}
				else
				{
					FaceEdgeMap.Add(Edge, 1);
				}
			}
		}
	}

	void PrintStatistics(const FGeometryCollection* GeometryCollection)
	{
		check(GeometryCollection);

		int32 NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
		int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
		int32 NumGeometries = GeometryCollection->NumElements(FGeometryCollection::GeometryGroup);
		int32 NumTransforms = GeometryCollection->NumElements(FGeometryCollection::TransformGroup);
		int32 NumBreakings = GeometryCollection->NumElements(FGeometryCollection::BreakingGroup);

		FString Buffer;
		Buffer += FString::Printf(TEXT("\n\n"));
		Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
		Buffer += FString::Printf(TEXT("Number of transforms = %d\n"), NumTransforms);
		Buffer += FString::Printf(TEXT("Number of vertices = %d\n"), NumVertices);
		Buffer += FString::Printf(TEXT("Number of faces = %d\n"), NumFaces);
		Buffer += FString::Printf(TEXT("Number of geometries = %d\n"), NumGeometries);
		Buffer += FString::Printf(TEXT("Number of breakings = %d\n"), NumBreakings);
		Buffer += FString::Printf(TEXT("------------------------------------------------------------\n\n"));
		UE_LOG(LogGeoemtryCollectionClean, Log, TEXT("%s"), *Buffer);
	}
}