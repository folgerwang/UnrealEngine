// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ManagedArray.h"

class FTransformCollection;
class FGeometryCollection;

namespace GeometryCollectionAlgo
{
	struct FFaceEdge
	{
		int32 VertexIdx1;
		int32 VertexIdx2;

		friend inline uint32 GetTypeHash(const FFaceEdge& Other)
		{
			return HashCombine(::GetTypeHash(Other.VertexIdx1), ::GetTypeHash(Other.VertexIdx2));
		}

		friend bool operator==(const FFaceEdge& A, const FFaceEdge& B)
		{
			return A.VertexIdx1 == B.VertexIdx1 && A.VertexIdx2 == B.VertexIdx2;
		}
	};

	/*
	* Print the parent hierarchy of the collection.
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	PrintParentHierarchy(const FGeometryCollection * Collection);

	/*
	*  Generate a contiguous array of int32's from 0 to Length-1
	*/
	TSharedRef<TArray<int32> > 
	GEOMETRYCOLLECTIONCORE_API 
	ContiguousArray(int32 Length);

	/**
	* Offset list for re-incrementing deleted elements.
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API
	BuildIncrementMask(const TArray<int32> & SortedDeletionList, const int32 & Size, TArray<int32> & Mask);

	/**
	*
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API
	BuildLookupMask(const TArray<int32> & SortedDeletionList, const int32 & Size, TArray<bool> & Mask);


	/*
	*
	*/
	void
	GEOMETRYCOLLECTIONCORE_API
	BuildTransformGroupToGeometryGroupMap(const FGeometryCollection& GeometryCollection, TArray<int32> & TransformToGeometry);


	/*
	*
	*/
	void
	GEOMETRYCOLLECTIONCORE_API
	BuildFaceGroupToGeometryGroupMap(const FGeometryCollection& GeometryCollection, const TArray<int32>& TransformToGeometryMap, TArray<int32> & FaceToGeometry);


	/**
	* Make sure the deletion list if correctly formed.
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API
	ValidateSortedList(const TArray<int32>&SortedDeletionList, const int32 & ListSize);

	/*
	*  Check if the Collection has multiple transform roots.
	*/
	bool 
	GEOMETRYCOLLECTIONCORE_API 
	HasMultipleRoots(FGeometryCollection * Collection);

	/*
	*
	*/
	bool 
	GEOMETRYCOLLECTIONCORE_API
	HasCycle(TManagedArray<FGeometryCollectionBoneNode> & Hierarchy, int32 Node);

	/*
	*
	*/
	bool 
	GEOMETRYCOLLECTIONCORE_API
	HasCycle(TManagedArray<FGeometryCollectionBoneNode> & Hierarchy, const TArray<int32>& SelectedBones);

	/*
	* Parent a single transform
	*/
	void
	GEOMETRYCOLLECTIONCORE_API 
	ParentTransform(FGeometryCollection* GeometryCollection, const int32 TransformIndex, const int32 ChildIndex);
		
	/*
	*  Parent the list of transforms to the selected index. 
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	ParentTransforms(FGeometryCollection* GeometryCollection, const int32 TransformIndex, const TArray<int32>& SelectedBones);

	/*
	*  Find the average position of the transforms.
	*/
	FVector
	GEOMETRYCOLLECTIONCORE_API
	AveragePosition(FGeometryCollection* Collection, const TArray<int32>& Indices);


	/*
	*  Global Matrices of the specified index.
	*/
	FTransform
	GEOMETRYCOLLECTIONCORE_API
	GlobalMatrix(const FTransformCollection* TransformCollection, int32 Index);


	/*
	*  Global Matrices of the collection based on list of indices
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	GlobalMatrices(const FTransformCollection* TransformCollection, const TArray<int32>& Indices, TArray<FTransform> & Transforms);

	/*
	*  Global Matrices of the collection, transforms will be resized to fit
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	GlobalMatrices(const FTransformCollection* TransformCollection, TArray<FTransform> & Transforms);

	/*
	*  Perpare for simulation moves the geometry to center of mass aligned, with option to re-center bones around origin of actor
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API
	PrepareForSimulation(FGeometryCollection* GeometryCollection, bool CenterAtOrigin=true);

	void
	GEOMETRYCOLLECTIONCORE_API
	ComputeCoincidentVertices(const FGeometryCollection* GeometryCollection, const float Tolerance, TMap<int32, int32>& CoincidentVerticesMap, TSet<int32>& VertexToDeleteSet);

	void
	GEOMETRYCOLLECTIONCORE_API
	DeleteCoincidentVertices(FGeometryCollection* GeometryCollection, float Tolerance = 1e-2);

	void
	GEOMETRYCOLLECTIONCORE_API
	ComputeZeroAreaFaces(const FGeometryCollection* GeometryCollection, const float Tolerance, TSet<int32>& FaceToDeleteSet);

	void
	GEOMETRYCOLLECTIONCORE_API
	DeleteZeroAreaFaces(FGeometryCollection* GeometryCollection, float Tolerance = 1e-4);

	void
	GEOMETRYCOLLECTIONCORE_API
	ComputeHiddenFaces(const FGeometryCollection* GeometryCollection, TSet<int32>& FaceToDeleteSet);

	void
	GEOMETRYCOLLECTIONCORE_API
	DeleteHiddenFaces(FGeometryCollection* GeometryCollection);

	void
	GEOMETRYCOLLECTIONCORE_API
	ComputeStaleVertices(const FGeometryCollection* GeometryCollection, TSet<int32>& VertexToDeleteSet);

	void
	GEOMETRYCOLLECTIONCORE_API
	DeleteStaleVertices(FGeometryCollection* GeometryCollection);

	void
	GEOMETRYCOLLECTIONCORE_API
	ComputeEdgeInFaces(const FGeometryCollection* GeometryCollection, TMap<FFaceEdge, int32>& FaceEdgeMap);

	void
	GEOMETRYCOLLECTIONCORE_API
	PrintStatistics(const FGeometryCollection* GeometryCollection);
}
