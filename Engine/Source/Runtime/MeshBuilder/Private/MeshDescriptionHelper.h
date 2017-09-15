// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

class UObject;
class UMeshDescription;
struct FMeshBuildSettings;
struct FVertexInstanceID;

class FMeshDescriptionHelper
{
public:
	enum ETangentOptions
	{
		None = 0,
		BlendOverlappingNormals = 0x1,
		IgnoreDegenerateTriangles = 0x2,
		UseMikkTSpace = 0x4,
	};

	enum class ELightmapUVVersion : int32
	{
		BitByBit = 0,
		Segments = 1,
		SmallChartPacking = 2,
		Latest = SmallChartPacking
	};

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings, const UMeshDescription* InOriginalMeshDescription);

	//Build a render mesh description with the BuildSettings. This will update the InRenderMeshDescription ptr content
	UMeshDescription* GetRenderMeshDescription(UObject* Owner);

	void CopyMeshDescription(UMeshDescription* SourceMeshDescription, UMeshDescription* DestinationMeshDescription) const;

	//Return true if there is a valid original mesh description, false otherwise(Auto generate LOD).
	bool IsValidOriginalMeshDescription();

	const TMultiMap<int32, int32>& GetOverlappingCorners() const { return OverlappingCorners; }

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	const FVector& GetVertexPositionFromVertexInstance(UMeshDescription* MeshDescription, int32 VertexInstanceIndex) const;
	FVector2D GetVertexInstanceUV(UMeshDescription* MeshDescription, int32 VertexInstanceIndex, int32 UVLayer) const;
	void GetVertexInstanceNTB(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID, FVector &OutNormal, FVector &OutTangent, FVector &OutBiNormal) const;
	void SetVertexInstanceNTB(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID, FVector &OutNormal, FVector &OutTangent, FVector &OutBiNormal);
	void ComputeNTB_MikkTSpace(UMeshDescription* MeshDescription, ETangentOptions TangentOptions);
	void ComputeTriangleTangents(UMeshDescription* MeshDescription, TArray<FVector>& OutTangentX, TArray<FVector>& OutTangentY, TArray<FVector>& OutTangentZ, float ComparisonThreshold);
	void FindOverlappingCorners(float ComparisonThreshold);

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	const UMeshDescription* OriginalMeshDescription;
	FMeshBuildSettings *BuildSettings;
	TMultiMap<int32, int32> OverlappingCorners;

	
	//////////////////////////////////////////////////////////////////////////
	//INLINE small helper use to optimize search and compare

	/** Helper struct for building acceleration structures. */
	struct FIndexAndZ
	{
		float Z;
		int32 Index;

		/** Default constructor. */
		FIndexAndZ() {}

		/** Initialization constructor. */
		FIndexAndZ(int32 InIndex, FVector V)
		{
			Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
			Index = InIndex;
		}
	};

	/** Sorting function for vertex Z/index pairs. */
	struct FCompareIndexAndZ
	{
		FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
	};

	/**
	* Smoothing group interpretation helper structure.
	*/
	struct FFanFace
	{
		int32 FaceIndex;
		int32 LinkedVertexIndex;
		bool bFilled;
		bool bBlendTangents;
		bool bBlendNormals;
	};

};
