// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionProximityUtility.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

DEFINE_LOG_CATEGORY_STATIC(LogChaosProximity, Verbose, All);

bool FGeometryCollectionProximityUtility::IsPointInsideOfTriangle(const FVector& P, const FVector& Vertex0, const FVector& Vertex1, const FVector& Vertex2, float Threshold)
{
	float FaceArea = 0.5f * ((Vertex1 - Vertex0) ^ (Vertex2 - Vertex0)).Size();
	float Face1Area = 0.5f * ((Vertex0 - P) ^ (Vertex2 - P)).Size();
	float Face2Area = 0.5f * ((Vertex0 - P) ^ (Vertex1 - P)).Size();
	float Face3Area = 0.5f * ((Vertex2 - P) ^ (Vertex1 - P)).Size();

	return (FMath::Abs(Face1Area + Face2Area + Face3Area - FaceArea) < Threshold);
}

void FGeometryCollectionProximityUtility::UpdateProximity(FGeometryCollection* GeometryCollection)
{
	check(GeometryCollection);

	const TManagedArray<FVector>& VertexArray = *GeometryCollection->Vertex;
	const TManagedArray<int32>& BoneMapArray = *GeometryCollection->BoneMap;
	const TManagedArray<FIntVector>& IndicesArray = *GeometryCollection->Indices;
	const TManagedArray<int32>& TransformIndexArray = *GeometryCollection->TransformIndex;
	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *GeometryCollection->BoneHierarchy;

	TManagedArray<TSet<int32>>& ProximityArray = *GeometryCollection->Proximity;
	TManagedArray<int32>& BreakingFaceIndexArray = *GeometryCollection->BreakingFaceIndex;
	TManagedArray<int32>& BreakingSourceTransformIndexArray = *GeometryCollection->BreakingSourceTransformIndex;
	TManagedArray<int32>& BreakingTargetTransformIndexArray = *GeometryCollection->BreakingTargetTransformIndex;
	TManagedArray<FVector>& BreakingRegionCentroidArray = *GeometryCollection->BreakingRegionCentroid;
	TManagedArray<FVector>& BreakingRegionNormalArray = *GeometryCollection->BreakingRegionNormal;
	TManagedArray<float>& BreakingRegionRadiusArray = *GeometryCollection->BreakingRegionRadius;

	float DistanceThreshold = 1e-2;
	TArray<FFaceTransformData> FaceTransformDataArray;
	FaceTransformDataArray.Empty();

	//
	// Create a FaceTransformDataArray for fast <FaceIndex, TransformIndex. lookup
	// It only contains faces for GEOMETRY && !CLUSTERED
	//
	int32 NumFaces = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
	for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
	{
		int32 TransformIndex = BoneMapArray[IndicesArray[IdxFace][0]];

		//		UE_LOG(LogChaosProximity, Log, TEXT("IdxFace = %d, TransformIndex = %d>"), IdxFace, TransformIndex);

		if (BoneHierarchyArray[TransformIndex].IsGeometry() && !BoneHierarchyArray[TransformIndex].IsClustered())
		{
			//			UE_LOG(LogChaosProximity, Log, TEXT("ADDING TO FACETRANSFORMDATAARRAY"));

			FFaceTransformData FaceData{ IdxFace,
				TransformIndex
			};
			FaceTransformDataArray.Add(FaceData);
		}
		//		else
		//		{
		//			UE_LOG(LogChaosProximity, Log, TEXT("NOT VALID"));
		//		}
	}
	NumFaces = FaceTransformDataArray.Num();

	// Build reverse map between TransformIdx and GeometryGroup index
	TMap<int32, int32> GeometryGroupIndexMap;
	int32 NumGeometries = GeometryCollection->NumElements(FGeometryCollection::GeometryGroup);
	for (int32 Idx = 0; Idx < NumGeometries; ++Idx)
	{
		GeometryGroupIndexMap.Add(TransformIndexArray[Idx], Idx);
	}

	// Transform vertices into world space
	TArray<FTransform> GlobalTransformArray;
	GeometryCollectionAlgo::GlobalMatrices(GeometryCollection, GlobalTransformArray);

	TArray<FVector> VertexInWorldArray;
	int32 NumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
	VertexInWorldArray.SetNum(NumVertices);

	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		FTransform Transform = GlobalTransformArray[BoneMapArray[IdxVertex]];
		FVector VertexInWorld = Transform.TransformPosition(VertexArray[IdxVertex]);

		VertexInWorldArray[IdxVertex] = VertexInWorld;
	}

	TSet<FOverlappingFacePair> OverlappingFacePairSet;
	int32 IdxFace, IdxOtherFace;
	for (auto& FaceTransformData : FaceTransformDataArray)
	{
		IdxFace = FaceTransformData.FaceIdx;
		for (auto& OtherFaceTransformData : FaceTransformDataArray)
		{
			IdxOtherFace = OtherFaceTransformData.FaceIdx;

			if (FaceTransformData.TransformIndex != OtherFaceTransformData.TransformIndex)
			{
				//
				// Vertex coincidence test
				//
				bool VertexCoincidenceTestFoundOverlappingFaces = false;
				{
					TArray<FVertexPair> VertexPairArray;
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][0]] }); // 0
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]] }); // 1
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]] }); // 2

					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][0]] }); // 3
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]] }); // 4
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]] }); // 5

					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][2]], VertexInWorldArray[IndicesArray[IdxOtherFace][0]] }); // 6
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][2]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]] }); // 7
					VertexPairArray.Add({ VertexInWorldArray[IndicesArray[IdxFace][2]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]] }); // 8

					int32 NumCoincideVertices = 0;
					for (int32 Idx = 0; Idx < VertexPairArray.Num(); ++Idx)
					{
						if (VertexPairArray[Idx].Distance() < DistanceThreshold)
						{
							NumCoincideVertices++;
						}
					}

					if (NumCoincideVertices >= 3)
					{
						VertexCoincidenceTestFoundOverlappingFaces = true;

						if (!OverlappingFacePairSet.Contains(FOverlappingFacePair{ FMath::Min(IdxFace, IdxOtherFace), FMath::Max(IdxFace, IdxOtherFace) }))
						{
							OverlappingFacePairSet.Add(FOverlappingFacePair{ FMath::Min(IdxFace, IdxOtherFace), FMath::Max(IdxFace, IdxOtherFace) });
						}
					}
				}

				//
				// FaceN and OtherFaceN are parallel and points of Face are in OtherFace test
				//
				if (!VertexCoincidenceTestFoundOverlappingFaces)
				{
					FVector Edge1 = (VertexInWorldArray[IndicesArray[IdxFace][1]] - VertexInWorldArray[IndicesArray[IdxFace][0]]);
					FVector Edge2 = (VertexInWorldArray[IndicesArray[IdxFace][2]] - VertexInWorldArray[IndicesArray[IdxFace][0]]);
					FVector FaceN = Edge1 ^ Edge2;

					FVector OtherEdge1 = (VertexInWorldArray[IndicesArray[IdxOtherFace][1]] - VertexInWorldArray[IndicesArray[IdxOtherFace][0]]);
					FVector OtherEdge2 = (VertexInWorldArray[IndicesArray[IdxOtherFace][2]] - VertexInWorldArray[IndicesArray[IdxOtherFace][0]]);
					FVector OtherFaceN = OtherEdge1 ^ OtherEdge2;

					if (FVector::Parallel(FaceN, OtherFaceN, 1e-1))
					{
						FVector FaceCenter = (VertexInWorldArray[IndicesArray[IdxFace][0]] + VertexInWorldArray[IndicesArray[IdxFace][1]] + VertexInWorldArray[IndicesArray[IdxFace][2]]) / 3.f;
						FVector PointInFace1 = (VertexInWorldArray[IndicesArray[IdxFace][0]] + FaceCenter) / 2.f;
						FVector PointInFace2 = (VertexInWorldArray[IndicesArray[IdxFace][1]] + FaceCenter) / 2.f;
						FVector PointInFace3 = (VertexInWorldArray[IndicesArray[IdxFace][2]] + FaceCenter) / 2.f;

						// Check if points in Face are in OtherFace
						if (IsPointInsideOfTriangle(FaceCenter, VertexInWorldArray[IndicesArray[IdxOtherFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]], 1e-1) ||
							IsPointInsideOfTriangle(PointInFace1, VertexInWorldArray[IndicesArray[IdxOtherFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]], 1e-1) ||
							IsPointInsideOfTriangle(PointInFace2, VertexInWorldArray[IndicesArray[IdxOtherFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]], 1e-1) ||
							IsPointInsideOfTriangle(PointInFace3, VertexInWorldArray[IndicesArray[IdxOtherFace][0]], VertexInWorldArray[IndicesArray[IdxOtherFace][1]], VertexInWorldArray[IndicesArray[IdxOtherFace][2]], 1e-1))
						{
							if (!OverlappingFacePairSet.Contains(FOverlappingFacePair{ FMath::Min(IdxFace, IdxOtherFace), FMath::Max(IdxFace, IdxOtherFace) }))
							{
								OverlappingFacePairSet.Add(FOverlappingFacePair{ FMath::Min(IdxFace, IdxOtherFace), FMath::Max(IdxFace, IdxOtherFace) });
							}
						}
						else
						{
							FVector OtherFaceCenter = (VertexInWorldArray[IndicesArray[IdxOtherFace][0]] + VertexInWorldArray[IndicesArray[IdxOtherFace][1]] + VertexInWorldArray[IndicesArray[IdxOtherFace][2]]) / 3.f;
							PointInFace1 = (VertexInWorldArray[IndicesArray[IdxOtherFace][0]] + OtherFaceCenter) / 2.f;
							PointInFace2 = (VertexInWorldArray[IndicesArray[IdxOtherFace][1]] + OtherFaceCenter) / 2.f;
							PointInFace3 = (VertexInWorldArray[IndicesArray[IdxOtherFace][2]] + OtherFaceCenter) / 2.f;

							// Check if points in OtherFace are in Face
							if (IsPointInsideOfTriangle(OtherFaceCenter, VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxFace][2]], 1e-1) ||
								IsPointInsideOfTriangle(PointInFace1, VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxFace][2]], 1e-1) ||
								IsPointInsideOfTriangle(PointInFace2, VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxFace][2]], 1e-1) ||
								IsPointInsideOfTriangle(PointInFace3, VertexInWorldArray[IndicesArray[IdxFace][0]], VertexInWorldArray[IndicesArray[IdxFace][1]], VertexInWorldArray[IndicesArray[IdxFace][2]], 1e-1))
							{
								if (!OverlappingFacePairSet.Contains(FOverlappingFacePair{ FMath::Min(IdxFace, IdxOtherFace), FMath::Max(IdxFace, IdxOtherFace) }))
								{
									OverlappingFacePairSet.Add(FOverlappingFacePair{ FMath::Min(IdxFace, IdxOtherFace), FMath::Max(IdxFace, IdxOtherFace) });
								}
							}
						}
					}
				}
			}
		}
	}

	if (!OverlappingFacePairSet.Num())
	{
		return;
	}

	// Populate Proximity, BreakingFaceIndex, BreakingSourceTransformIndex, BreakingTargetTransformIndex structures
	for (int32 IdxGeometry = 0; IdxGeometry < NumGeometries; ++IdxGeometry)
	{
		ProximityArray[IdxGeometry].Empty();
	}

	TArray<int32> AllBreakingFaceIndexArray;
	TArray<int32> AllBreakingSourceTransformIndexArray;
	TArray<int32> AllBreakingTargetTransformIndexArray;

	int32 NewArraySize = 2 * OverlappingFacePairSet.Num();
	AllBreakingFaceIndexArray.SetNum(NewArraySize);
	AllBreakingSourceTransformIndexArray.SetNum(NewArraySize);
	AllBreakingTargetTransformIndexArray.SetNum(NewArraySize);

	//
	// Create the {BreakingSourceTransformIndex, BreakingTargetTransformIndex} <-> FaceIndex data in the
	// AllBreakingFaceIndexArray, AllBreakingSourceTransformIndexArray, AllBreakingTargetTransformIndexArray arrays
	// This contains every connected face pairs, a lot of data
	//
	int32 IdxBreak = 0;
	for (auto& OverlappingFacePair : OverlappingFacePairSet)
	{
		int32 TransformIndex1 = BoneMapArray[IndicesArray[OverlappingFacePair.FaceIdx1][0]];
		int32 TransformIndex2 = BoneMapArray[IndicesArray[OverlappingFacePair.FaceIdx2][0]];

		check(BoneHierarchyArray[TransformIndex1].IsGeometry() && !BoneHierarchyArray[TransformIndex1].IsClustered());
		check(BoneHierarchyArray[TransformIndex2].IsGeometry() && !BoneHierarchyArray[TransformIndex2].IsClustered());

//		if (!ProximityArray[GeometryGroupIndexMap[TransformIndex1]].Contains(TransformIndex2))
//		{
//			ProximityArray[GeometryGroupIndexMap[TransformIndex1]].Add(TransformIndex2);
//		}
		if (!ProximityArray[GeometryGroupIndexMap[TransformIndex1]].Contains(GeometryGroupIndexMap[TransformIndex2]))
		{
			ProximityArray[GeometryGroupIndexMap[TransformIndex1]].Add(GeometryGroupIndexMap[TransformIndex2]);
		}

		AllBreakingFaceIndexArray[IdxBreak] = OverlappingFacePair.FaceIdx1;
		AllBreakingSourceTransformIndexArray[IdxBreak] = TransformIndex1;
		AllBreakingTargetTransformIndexArray[IdxBreak] = TransformIndex2;
		IdxBreak++;

//		if (!ProximityArray[GeometryGroupIndexMap[TransformIndex2]].Contains(TransformIndex1))
//		{
//			ProximityArray[GeometryGroupIndexMap[TransformIndex2]].Add(TransformIndex1);
//		}
		if (!ProximityArray[GeometryGroupIndexMap[TransformIndex2]].Contains(GeometryGroupIndexMap[TransformIndex1]))
		{
			ProximityArray[GeometryGroupIndexMap[TransformIndex2]].Add(GeometryGroupIndexMap[TransformIndex1]);
		}

		AllBreakingFaceIndexArray[IdxBreak] = OverlappingFacePair.FaceIdx2;
		AllBreakingSourceTransformIndexArray[IdxBreak] = TransformIndex2;
		AllBreakingTargetTransformIndexArray[IdxBreak] = TransformIndex1;
		IdxBreak++;
	}

	//
	// Store the data as a MultiMap<{BreakingSourceTransformIndex, BreakingTargetTransformIndex}, FaceIndex>
	//
	TMultiMap<FOverlappingFacePairTransformIndex, int32> FaceByConnectedTransformsMap;
	if (AllBreakingFaceIndexArray.Num())
	{
		for (int32 Idx = 0; Idx < AllBreakingFaceIndexArray.Num(); ++Idx)
		{
			FaceByConnectedTransformsMap.Add(FOverlappingFacePairTransformIndex{ AllBreakingSourceTransformIndexArray[Idx], AllBreakingTargetTransformIndexArray[Idx] },
				AllBreakingFaceIndexArray[Idx]);
		}
	}

	//
	// Get all the keys from the MultiMap
	//
	TArray<FOverlappingFacePairTransformIndex> FaceByConnectedTransformsMapKeys;
	FaceByConnectedTransformsMap.GenerateKeyArray(FaceByConnectedTransformsMapKeys);

	//
	// Delete all the duplicates
	//
	TSet<FOverlappingFacePairTransformIndex> FaceByConnectedTransformsMapKeysSet;
	for (int32 Idx = 0; Idx < FaceByConnectedTransformsMapKeys.Num(); ++Idx)
	{
		if (!FaceByConnectedTransformsMapKeysSet.Contains(FaceByConnectedTransformsMapKeys[Idx]))
		{
			FaceByConnectedTransformsMapKeysSet.Add(FaceByConnectedTransformsMapKeys[Idx]);
		}
	}

	int LastIndex = GeometryCollection->AddElements(FaceByConnectedTransformsMapKeysSet.Num() - BreakingFaceIndexArray.Num(), FGeometryCollection::BreakingGroup);

	//
	// Get one Face for every {BreakingSourceTransformIndex, BreakingTargetTransformIndex} pair and store the data in
	// BreakingFaceIndexArray, BreakingSourceTransformIndexArray, BreakingTargetTransformIndexArray
	//		
	IdxBreak = 0;
	for (auto& Elem : FaceByConnectedTransformsMapKeysSet)
	{
		TArray<int32> FaceIndexArray;
		FaceByConnectedTransformsMap.MultiFind(Elem, FaceIndexArray);

		// Find the centroid of the region and save it into BreakingRegionCentroidArray
		FVector Centroid = FVector(0.f);
		float TotalArea = 0.f;
		for (int32 LocalIdxFace = 0; LocalIdxFace < FaceIndexArray.Num(); ++LocalIdxFace)
		{
			FVector Vertex0 = VertexArray[IndicesArray[FaceIndexArray[LocalIdxFace]][0]];
			FVector Vertex1 = VertexArray[IndicesArray[FaceIndexArray[LocalIdxFace]][1]];
			FVector Vertex2 = VertexArray[IndicesArray[FaceIndexArray[LocalIdxFace]][2]];

			FVector FaceCentroid = (Vertex0 + Vertex1 + Vertex2) / 3.f;
			float FaceArea = 0.5f * ((Vertex1 - Vertex0) ^ (Vertex2 - Vertex0)).Size();

			Centroid = (TotalArea * Centroid + FaceArea * FaceCentroid) / (TotalArea + FaceArea);

			TotalArea += FaceArea;
		}
		BreakingRegionCentroidArray[IdxBreak] = Centroid;

		// Find the inner radius of the region and save it into BreakingRegionRadiusArray
		float RadiusMin = FLT_MAX;
		float RadiusMax = FLT_MIN;

		// We test all the points and half points on the boundary edges of the region
		/*
		TMap<FFaceEdge, int32> FaceEdgeMap;
		for (int32 IdxFace = 0; IdxFace < FaceIndexArray.Num(); ++IdxFace)
		{
			for (int32 Idx = 0; Idx < 3; Idx++)
			{
				int32 VertexIndex1 = IndicesArray[FaceIndexArray[IdxFace]][Idx];
				int32 VertexIndex2 = IndicesArray[FaceIndexArray[IdxFace]][(Idx + 1) % 3];
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

		TArray<FVector> TestPoints;
		for (auto& Edge : FaceEdgeMap)
		{
			if (FaceEdgeMap[Edge.Key] == 1)
			{
				TestPoints.Add(VertexArray[Edge.Key.VertexIdx1]);
				TestPoints.Add(VertexArray[Edge.Key.VertexIdx2]);
//				TestPoints.Add((VertexArray[Edge.Key.VertexIdx1] + VertexArray[Edge.Key.VertexIdx2]) / 2.f);
			}
		}
		*/
		
		TArray<FVector> TestPoints;
		for (int32 LocalIdxFace = 0; LocalIdxFace < FaceIndexArray.Num(); ++LocalIdxFace)
		{
			for (int32 Idx = 0; Idx < 3; Idx++)
			{
				TestPoints.Add(VertexArray[IndicesArray[FaceIndexArray[LocalIdxFace]][Idx]]);
			}
		}
		
 		for (int32 IdxPoint = 0; IdxPoint < TestPoints.Num(); ++IdxPoint)
		{
			float Distance = (Centroid - TestPoints[IdxPoint]).Size();
			if (Distance < RadiusMin)
			{
				RadiusMin = Distance;
			}
			if (Distance > RadiusMax)
			{
				RadiusMax = Distance;
			}
		}
		BreakingRegionRadiusArray[IdxBreak] = RadiusMin;

		// Normal
		FVector VertexA = VertexArray[IndicesArray[FaceIndexArray[0]][0]];
		FVector VertexB = VertexArray[IndicesArray[FaceIndexArray[0]][1]];
		FVector VertexC = VertexArray[IndicesArray[FaceIndexArray[0]][2]];
		BreakingRegionNormalArray[IdxBreak] = ((VertexA - VertexB) ^ (VertexC - VertexB)).GetSafeNormal();

		// grab the first face from the region and save it into BreakingFaceIndexArray
		BreakingFaceIndexArray[IdxBreak] = FaceIndexArray[0];
		BreakingSourceTransformIndexArray[IdxBreak] = Elem.TransformIdx1;
		BreakingTargetTransformIndexArray[IdxBreak] = Elem.TransformIdx2;
		IdxBreak++;
	}
}

