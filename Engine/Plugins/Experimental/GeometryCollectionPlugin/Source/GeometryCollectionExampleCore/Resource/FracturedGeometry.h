// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Array.h"
#include "Math/Transform.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"

namespace GeometryCollectionExample
{

	class FracturedGeometry
	{
	public:
		FracturedGeometry();
		~FracturedGeometry();

		static const TArray<float>	RawVertexArray;
		static const TArray<int32>	RawIndicesArray;
		static const TArray<int32>	RawBoneMapArray;
		static const TArray<FTransform> RawTransformArray;
		static const TArray<FGeometryCollectionBoneNode> RawBoneHierarchyArray;
	};

	/**
	 * A version of \c FracturedGeometry in a global point pool.
	 */
	class GlobalFracturedGeometry
	{
	public:
		GlobalFracturedGeometry();
		~GlobalFracturedGeometry();

		TArray<float> RawVertexArray;
		TArray<int32>	RawIndicesArray0; // Randomly coincident to RawIndicesArray1
		TArray<int32>	RawIndicesArray1; // Unchanged from FracturedGeometry
		TArray<int32>	RawIndicesArray2; // Offset in Y
		const TArray<int32> &RawIndicesArray; // Points to RawIndicesArray1
		TArray<int32> RawIndicesArrayMerged; // RawIndicesArray 0, 1, and 2 concatenated
		const TArray<int32>	RawBoneMapArray;
		const TArray<FTransform> RawTransformArray;
		const TArray<FGeometryCollectionBoneNode> RawBoneHierarchyArray;
	};
}