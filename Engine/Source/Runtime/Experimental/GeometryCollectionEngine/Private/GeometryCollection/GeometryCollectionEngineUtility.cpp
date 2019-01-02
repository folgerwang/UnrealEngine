// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionEngineUtility.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionCache.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

DEFINE_LOG_CATEGORY_STATIC(LogGeoemtryCollectionClean, Verbose, All);


void GeometryCollectionEngineUtility::PrintDetailedStatistics(const FGeometryCollection* GeometryCollection, const UGeometryCollectionCache* InCache)
{
	check(GeometryCollection);

	TSharedPtr< TManagedArray<FTransform> >   Transform;
	TSharedPtr< TManagedArray<FGeometryCollectionBoneNode> >  BoneHierarchy;

	int32 NumTransforms = GeometryCollection->NumElements(FGeometryCollection::TransformGroup);
	int32 NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
	int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
	int32 NumGeometries = GeometryCollection->NumElements(FGeometryCollection::GeometryGroup);
	int32 NumBreakings = GeometryCollection->NumElements(FGeometryCollection::BreakingGroup);

	const TManagedArray<FVector>& VertexArray = *GeometryCollection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *GeometryCollection->BoneMap;

	TArray<FTransform> GlobalTransformArray;
	GeometryCollectionAlgo::GlobalMatrices(GeometryCollection, GlobalTransformArray);

	FBox BoundingBox(ForceInitToZero);
	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		FTransform GlobalTransform = GlobalTransformArray[BoneMapArray[IdxVertex]];
		FVector VertexInWorld = GlobalTransform.TransformPosition(VertexArray[IdxVertex]);

		BoundingBox += VertexInWorld;
	}

	FString Buffer;
	Buffer += FString::Printf(TEXT("\n\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("TRANSFORM GROUP\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("Number of transforms = %d\n"), NumTransforms);
	// Print Transform array
	// Print BoneHierarchy array
	GeometryCollectionAlgo::PrintParentHierarchy(GeometryCollection);
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("VERTICES GROUP\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("Number of vertices = %d\n"), NumVertices);
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("FACES GROUP\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("Number of faces = %d\n"), NumFaces);
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("GEOMETRY GROUP\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("Number of geometries = %d\n"), NumGeometries);
	// Print TransformIndex array
	// Print Proximity array
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("BREAKING GROUP\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("Number of breakings = %d\n"), NumBreakings);
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("BOUNDING BOX\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("Min = (%f, %f, %f)\n"), BoundingBox.Min.X, BoundingBox.Min.Y, BoundingBox.Min.Z);
	Buffer += FString::Printf(TEXT("Max = (%f, %f, %f)\n"), BoundingBox.Max.X, BoundingBox.Max.Y, BoundingBox.Max.Z);
	Buffer += FString::Printf(TEXT("Center = (%f, %f, %f)\n"), BoundingBox.GetCenter().X, BoundingBox.GetCenter().Y, BoundingBox.GetCenter().Z);
	Buffer += FString::Printf(TEXT("Size = (%f, %f, %f)\n"), 2.f * BoundingBox.GetExtent().X, 2.f * BoundingBox.GetExtent().Y, 2.f * BoundingBox.GetExtent().Z);

	
	if(InCache && InCache->GetData())
	{
		Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
		Buffer += FString::Printf(TEXT("CACHE INFO\n"));
		Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
		const FRecordedTransformTrack* Track = InCache->GetData();
		int32 NumRecords = Track->Records.Num();
		if(NumRecords == 0)
		{
			Buffer += FString::Printf(TEXT("Cache is empty\n"));
		}
		else
		{
			float FirstRecordTimestamp = Track->Records[0].Timestamp;
			float LastRecordTimestamp = Track->Records[NumRecords - 1].Timestamp;

			TMultiMap<int32, int32> TimestampRecordMap;
			for(int32 IdxRecord = 0; IdxRecord < NumRecords; ++IdxRecord)
			{
				TimestampRecordMap.Add(FMath::FloorToInt(Track->Records[IdxRecord].Timestamp), IdxRecord);
			}

			TArray<int32> NumRecordsPerSecond;
			NumRecordsPerSecond.SetNum(FMath::CeilToInt(LastRecordTimestamp));
			for(int32 Idx = 0; Idx < FMath::CeilToInt(LastRecordTimestamp); ++Idx)
			{
				NumRecordsPerSecond[Idx] = TimestampRecordMap.Num(Idx);
			}

			int32 NumRecordsMin = INT_MAX, NumRecordsMax = INT_MIN;
			float NumRecordsAverage = 0.f;
			for(int32 Idx = 0; Idx < NumRecordsPerSecond.Num(); ++Idx)
			{
				if(NumRecordsPerSecond[Idx] < NumRecordsMin)
				{
					NumRecordsMin = NumRecordsPerSecond[Idx];
				}
				if(NumRecordsPerSecond[Idx] > NumRecordsMax)
				{
					NumRecordsMax = NumRecordsPerSecond[Idx];
				}
				NumRecordsAverage += (float)NumRecordsPerSecond[Idx];
			}
			NumRecordsAverage /= (float)NumRecordsPerSecond.Num();

			Buffer += FString::Printf(TEXT("Cache length [%f - %f]\n"), FirstRecordTimestamp, LastRecordTimestamp);
			Buffer += FString::Printf(TEXT("Number of recorded frames = %d\n"), NumRecords);
			for(int32 Idx = 0; Idx < NumRecordsPerSecond.Num(); ++Idx)
			{
				Buffer += FString::Printf(TEXT("Number of recorded frames at %ds = %d\n"), Idx, NumRecordsPerSecond[Idx]);
			}
			Buffer += FString::Printf(TEXT("Minimum number of recorded frames per second = %d\n"), NumRecordsMin);
			Buffer += FString::Printf(TEXT("Maximum number of recorded frames per second = %d\n"), NumRecordsMax);
			Buffer += FString::Printf(TEXT("Average number of recorded frames per second = %f\n"), NumRecordsAverage);

			TArray<int32> NumCollisionMinPerSecond;
			NumCollisionMinPerSecond.Init(0, FMath::CeilToInt(LastRecordTimestamp));
			TArray<int32> NumCollisionMaxPerSecond;
			NumCollisionMaxPerSecond.Init(0, FMath::CeilToInt(LastRecordTimestamp));
			TArray<float> NumCollisionAveragePerSecond;
			NumCollisionAveragePerSecond.Init(0.f, FMath::CeilToInt(LastRecordTimestamp));
			int32 NumTotalCollisions = 0;
			TArray<int32> RecordIdxForOneSecond;
			for(int32 IdxSeconds = 0; IdxSeconds < NumRecordsPerSecond.Num(); ++IdxSeconds)
			{
				RecordIdxForOneSecond.Empty();
				TimestampRecordMap.MultiFind(IdxSeconds, RecordIdxForOneSecond);

				for(int32 IdxRecord = 0; IdxRecord < RecordIdxForOneSecond.Num(); ++IdxRecord)
				{
					int32 NumCollisions = Track->Records[RecordIdxForOneSecond[IdxRecord]].Collisions.Num();
					if(NumCollisions > 0)
					{
						if(NumCollisions < NumCollisionMinPerSecond[IdxSeconds])
						{
							NumCollisionMinPerSecond[IdxSeconds] = NumCollisions;
						}
						if(NumCollisions > NumCollisionMaxPerSecond[IdxSeconds])
						{
							NumCollisionMaxPerSecond[IdxSeconds] = NumCollisions;
						}
						NumCollisionAveragePerSecond[IdxSeconds] += NumCollisions;
						NumTotalCollisions += NumCollisions;
					}
				}
				NumCollisionAveragePerSecond[IdxSeconds] /= (float)RecordIdxForOneSecond.Num();
			}

			Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
			for(int32 Idx = 0; Idx < NumRecordsPerSecond.Num(); ++Idx)
			{
				Buffer += FString::Printf(TEXT("Number of min collisions at %ds = %d\n"), Idx, NumCollisionMinPerSecond[Idx]);
				Buffer += FString::Printf(TEXT("Number of max collisions at %ds = %d\n"), Idx, NumCollisionMaxPerSecond[Idx]);
				Buffer += FString::Printf(TEXT("Number of average collisions at %ds = %f\n"), Idx, NumCollisionAveragePerSecond[Idx]);
			}
			Buffer += FString::Printf(TEXT("Number of total collisions = %d\n"), NumTotalCollisions);
		}
	}
	

	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));
	Buffer += FString::Printf(TEXT("MESH QUALITY\n"));
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n"));

	TSet<int32> VertexToDeleteSet;
	TMap<int32, int32> CoincidentVerticesMap;
	GeometryCollectionAlgo::ComputeCoincidentVertices(GeometryCollection, 1e-2, CoincidentVerticesMap, VertexToDeleteSet);
	int32 NumCoincidentVertices = VertexToDeleteSet.Num();

	TSet<int32> FaceToDeleteSet;
	GeometryCollectionAlgo::ComputeZeroAreaFaces(GeometryCollection, 1e-4, FaceToDeleteSet);
	int32 NumZeroAreaFaces = FaceToDeleteSet.Num();

	GeometryCollectionAlgo::ComputeHiddenFaces(GeometryCollection, FaceToDeleteSet);
	int32 NumHiddenFaces = FaceToDeleteSet.Num();

	GeometryCollectionAlgo::ComputeStaleVertices(GeometryCollection, VertexToDeleteSet);
	int32 NumStaleVertices = VertexToDeleteSet.Num();

	TMap<GeometryCollectionAlgo::FFaceEdge, int32> FaceEdgeMap;
	GeometryCollectionAlgo::ComputeEdgeInFaces(GeometryCollection, FaceEdgeMap);

	int32 NumBoundaryEdges = 0;
	int32 NumDegenerateEdges = 0;
	for (auto& Edge : FaceEdgeMap)
	{
		if (FaceEdgeMap[Edge.Key] == 0)
		{
			NumBoundaryEdges++;
		}
		else if (FaceEdgeMap[Edge.Key] > 2)
		{
			NumDegenerateEdges++;
		}
	}

	Buffer += FString::Printf(TEXT("Number of coincident vertices = %d\n"), NumCoincidentVertices);
	Buffer += FString::Printf(TEXT("Number of zero area faces = %d\n"), NumZeroAreaFaces);
	Buffer += FString::Printf(TEXT("Number of hidden faces = %d\n"), NumHiddenFaces);
	Buffer += FString::Printf(TEXT("Number of stale vertices = %d\n"), NumStaleVertices);
	Buffer += FString::Printf(TEXT("Number of boundary edges = %d\n"), NumBoundaryEdges);
	Buffer += FString::Printf(TEXT("Number of degenerate edges (included in more than 2 faces) = %d\n"), NumDegenerateEdges);
	Buffer += FString::Printf(TEXT("------------------------------------------------------------\n\n"));
	UE_LOG(LogGeoemtryCollectionClean, Log, TEXT("%s"), *Buffer);
}
