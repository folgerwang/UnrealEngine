// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MeshDescriptionHelper.h"

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Engine/EngineTypes.h"
#include "MeshDescription.h"
#include "mikktspace.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "RenderUtils.h"

namespace MeshDescriptionMikktSpaceInterface
{
	//Mikk t spce static function
	int MikkGetNumFaces(const SMikkTSpaceContext* Context);
	int MikkGetNumVertsOfFace(const SMikkTSpaceContext* Context, const int FaceIdx);
	void MikkGetPosition(const SMikkTSpaceContext* Context, float Position[3], const int FaceIdx, const int VertIdx);
	void MikkGetNormal(const SMikkTSpaceContext* Context, float Normal[3], const int FaceIdx, const int VertIdx);
	void MikkSetTSpaceBasic(const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int FaceIdx, const int VertIdx);
	void MikkGetTexCoord(const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx);
}

FMeshDescriptionHelper::FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings, const UMeshDescription* InOriginalMeshDescription)
	: OriginalMeshDescription(InOriginalMeshDescription)
	, BuildSettings(InBuildSettings)
{
}

UMeshDescription* FMeshDescriptionHelper::GetRenderMeshDescription(UObject* Owner)
{
	//Use the build settings to create the RenderMeshDescription
	UMeshDescription *RenderMeshDescription = NewObject<UMeshDescription>(Owner, NAME_None, RF_NoFlags);

	if(OriginalMeshDescription != nullptr)
	{
		//Copy The Original Mesh Description in the render mesh description
		CopyMeshDescription(const_cast<UMeshDescription*>(OriginalMeshDescription), RenderMeshDescription);
		float ComparisonThreshold = BuildSettings->bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;
		int32 NumWedges = RenderMeshDescription->VertexInstances().Num();
		// Find overlapping corners to accelerate adjacency.
		FindOverlappingCorners(ComparisonThreshold);

		// Figure out if we should recompute normals and tangents.
		int32 OriginalNormalCount = RenderMeshDescription->VertexInstances().Num();
		int32 OriginalTangentCount = OriginalNormalCount;
		int32 OriginalBiNormalCount = OriginalNormalCount;
		bool bRecomputeNormals = BuildSettings->bRecomputeNormals || OriginalNormalCount != NumWedges;
		bool bRecomputeTangents = BuildSettings->bRecomputeTangents || OriginalTangentCount != NumWedges || OriginalBiNormalCount != NumWedges;

		if (bRecomputeTangents || bRecomputeNormals)
		{
			for (const FVertexInstanceID& VertexInstanceID : RenderMeshDescription->VertexInstances().GetElementIDs())
			{
				FMeshVertexInstance& VertexInstance = RenderMeshDescription->GetVertexInstance(VertexInstanceID);
				// Dump normals and tangents if we are recomputing them.
				if (bRecomputeTangents)
				{
					//Dump the tangents
					VertexInstance.BinormalSign = 0.0f;
					VertexInstance.Tangent = FVector(0.0f);
				}
				if (bRecomputeNormals)
				{
					//Dump the normals
					VertexInstance.Normal = FVector(0.0f);
				}
			}
		}

		// Compute any missing tangents.
		{
			// Static meshes always blend normals of overlapping corners.
			uint32 TangentOptions = FMeshDescriptionHelper::ETangentOptions::BlendOverlappingNormals;
			if (BuildSettings->bRemoveDegenerates)
			{
				// If removing degenerate triangles, ignore them when computing tangents.
				TangentOptions |= FMeshDescriptionHelper::ETangentOptions::IgnoreDegenerateTriangles;
			}

			//Keep the original mesh description NTBs if we do not rebuild the normals or tangents.
			//MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
			if (BuildSettings->bUseMikkTSpace && (BuildSettings->bRecomputeNormals || BuildSettings->bRecomputeTangents))
			{
				ComputeNTB_MikkTSpace(RenderMeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions);
			}
			//TODO: builtin Compute of the tangents
			//else if(BuildSettings->bRecomputeNormals || BuildSettings->bRecomputeTangents)
			//{
				//ComputeTangents(RenderMeshDescription, OverlappingCorners, TangentOptions);
			//}
		}

		// TODO: Generate lightmap UVs
		/*if (BuildSettings->bGenerateLightmapUVs)
		{
			if (RawMesh.WedgeTexCoords[BuildSettings->SrcLightmapIndex].Num() == 0)
			{
				BuildSettings->SrcLightmapIndex = 0;
			}

			FLayoutUV Packer(&RawMesh, BuildSettings->SrcLightmapIndex, BuildSettings->DstLightmapIndex, BuildSettings->MinLightmapResolution);
			Packer.SetVersion(LightmapUVVersion);

			Packer.FindCharts(OverlappingCorners);
			bool bPackSuccess = Packer.FindBestPacking();
			if (bPackSuccess)
			{
				Packer.CommitPackedUVs();
			}
		}
		HasRawMesh[LODIndex] = true;*/
	}
	else
	{
	}

	return RenderMeshDescription;
}

void FMeshDescriptionHelper::CopyMeshDescription(UMeshDescription* SourceMeshDescription, UMeshDescription* DestinationMeshDescription) const
{
	//Copy the Source into the destination
	//Save the Source
	TArray<uint8> TempBytes;
	FMemoryWriter SaveAr(TempBytes, /*bIsPersistent=*/ true);
	SourceMeshDescription->Serialize(SaveAr);
	//Load the save array in the destination
	FMemoryReader LoadAr(TempBytes, /*bIsPersistent=*/ true);
	DestinationMeshDescription->Serialize(LoadAr);
}

bool FMeshDescriptionHelper::IsValidOriginalMeshDescription()
{
	return OriginalMeshDescription != nullptr;
}

void FMeshDescriptionHelper::FindOverlappingCorners(float ComparisonThreshold)
{
	//Empty the old data
	OverlappingCorners.Empty();
	const int32 NumWedges = OriginalMeshDescription->VertexInstances().Num();

	// Create a list of vertex Z/index pairs
	TArray<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Reserve(NumWedges);
	const FVertexArray& Vertices = OriginalMeshDescription->Vertices();
	const FVertexInstanceArray& VertexInstances = OriginalMeshDescription->VertexInstances();

	for (const FPolygonID& PolygonID : OriginalMeshDescription->Polygons().GetElementIDs())
	{
		const TArray<FMeshTriangle>& MeshTriangles = OriginalMeshDescription->GetPolygon(PolygonID).Triangles;
		for (const FMeshTriangle& MeshTriangle : MeshTriangles)
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				const FMeshVertexInstance& VertexInstance = VertexInstances[VertexInstanceID];

				new(VertIndexAndZ)FIndexAndZ(VertexInstanceID.GetValue(), Vertices[VertexInstance.VertexID].VertexPosition);
			}
		}
	}

	// Sort the vertices by z value
	VertIndexAndZ.Sort(FCompareIndexAndZ());

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		// only need to search forward, since we add pairs both ways
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > ComparisonThreshold)
				break; // can't be any more dups

			const FVector& PositionA = OriginalMeshDescription->GetVertex(OriginalMeshDescription->GetVertexInstance(FVertexInstanceID(VertIndexAndZ[i].Index)).VertexID).VertexPosition;
			const FVector& PositionB = OriginalMeshDescription->GetVertex(OriginalMeshDescription->GetVertexInstance(FVertexInstanceID(VertIndexAndZ[j].Index)).VertexID).VertexPosition;

			if (PositionA.Equals(PositionB, ComparisonThreshold))
			{
				OverlappingCorners.Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
				OverlappingCorners.Add(VertIndexAndZ[j].Index, VertIndexAndZ[i].Index);
			}
		}
	}
}

const FVector& FMeshDescriptionHelper::GetVertexPositionFromVertexInstance(UMeshDescription* MeshDescription, int32 VertexInstanceIndex) const
{
	return MeshDescription->GetVertex(MeshDescription->GetVertexInstance(FVertexInstanceID(VertexInstanceIndex)).VertexID).VertexPosition;
}

FVector2D FMeshDescriptionHelper::GetVertexInstanceUV(UMeshDescription* MeshDescription, int32 VertexInstanceIndex, int32 UVLayer) const
{
	return MeshDescription->VertexInstanceAttributes().GetAttribute<FVector2D>(FVertexInstanceID(VertexInstanceIndex), UEditableMeshAttribute::VertexTextureCoordinate(), UVLayer);
}

void FMeshDescriptionHelper::GetVertexInstanceNTB(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID, FVector &OutNormal, FVector &OutTangent, FVector &OutBiNormal) const
{
	const FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
	OutNormal = VertexInstance.Normal;
	OutTangent = VertexInstance.Tangent;
	if (!OutNormal.IsNearlyZero() && !OutTangent.IsNearlyZero())
	{
		OutBiNormal = FVector::CrossProduct(VertexInstance.Normal, VertexInstance.Tangent).GetSafeNormal() * VertexInstance.BinormalSign;
	}
	else
	{
		OutBiNormal = FVector(0.0f);
	}
}

void FMeshDescriptionHelper::SetVertexInstanceNTB(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID, FVector &OutNormal, FVector &OutTangent, FVector &OutBiNormal)
{
	FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
	VertexInstance.Normal = OutNormal;
	VertexInstance.Tangent = OutTangent;
	VertexInstance.BinormalSign = GetBasisDeterminantSign(OutTangent, OutBiNormal, OutNormal);
}

void FMeshDescriptionHelper::ComputeTriangleTangents(
	UMeshDescription* MeshDescription,
	TArray<FVector>& OutTangentX,
	TArray<FVector>& OutTangentY,
	TArray<FVector>& OutTangentZ,
	float ComparisonThreshold
)
{
	const int32 NumTriangles = MeshDescription->Polygons().Num();
	OutTangentX.Empty(NumTriangles);
	OutTangentY.Empty(NumTriangles);
	OutTangentZ.Empty(NumTriangles);

	for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		const TArray<FMeshTriangle>& MeshTriangles = MeshDescription->GetPolygon(PolygonID).Triangles;
		for (const FMeshTriangle& MeshTriangle : MeshTriangles)
		{
			int32 UVIndex = 0;

			FVector P[3];
			for (int32 i = 0; i < 3; ++i)
			{
				P[i] = GetVertexPositionFromVertexInstance(MeshDescription, MeshTriangle.GetVertexInstanceID(i).GetValue());
			}

			const FVector Normal = ((P[1] - P[2]) ^ (P[0] - P[2])).GetSafeNormal(ComparisonThreshold);
			FMatrix	ParameterToLocal(
				FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
				FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
				FPlane(P[0].X, P[0].Y, P[0].Z, 0),
				FPlane(0, 0, 0, 1)
			);

			const FVector2D T1 = GetVertexInstanceUV(MeshDescription, MeshTriangle.GetVertexInstanceID(0).GetValue(), 0);
			const FVector2D T2 = GetVertexInstanceUV(MeshDescription, MeshTriangle.GetVertexInstanceID(1).GetValue(), 0);
			const FVector2D T3 = GetVertexInstanceUV(MeshDescription, MeshTriangle.GetVertexInstanceID(2).GetValue(), 0);

			FMatrix ParameterToTexture(
				FPlane(T2.X - T1.X, T2.Y - T1.Y, 0, 0),
				FPlane(T3.X - T1.X, T3.Y - T1.Y, 0, 0),
				FPlane(T1.X, T1.Y, 1, 0),
				FPlane(0, 0, 0, 1)
			);

			// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
			const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

			OutTangentX.Add(TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal());
			OutTangentY.Add(TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal());
			OutTangentZ.Add(Normal);

			FVector::CreateOrthonormalBasis(
				OutTangentX[PolygonID.GetValue()],
				OutTangentY[PolygonID.GetValue()],
				OutTangentZ[PolygonID.GetValue()]
			);
		}
	}

	check(OutTangentX.Num() == NumTriangles);
	check(OutTangentY.Num() == NumTriangles);
	check(OutTangentZ.Num() == NumTriangles);
}

void FMeshDescriptionHelper::ComputeNTB_MikkTSpace(UMeshDescription* MeshDescription, FMeshDescriptionHelper::ETangentOptions TangentOptions)
{
	bool bBlendOverlappingNormals = (TangentOptions & FMeshDescriptionHelper::ETangentOptions::BlendOverlappingNormals) != 0;
	bool bIgnoreDegenerateTriangles = (TangentOptions & FMeshDescriptionHelper::ETangentOptions::IgnoreDegenerateTriangles) != 0;
	float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

	// Compute per-triangle tangents.
	TArray<FVector> TriangleTangentX;
	TArray<FVector> TriangleTangentY;
	TArray<FVector> TriangleTangentZ;

	ComputeTriangleTangents(
		MeshDescription,
		TriangleTangentX,
		TriangleTangentY,
		TriangleTangentZ,
		bIgnoreDegenerateTriangles ? SMALL_NUMBER : 0.0f
	);
	TArray<uint32> SmoothingGroupIndices;
	SmoothingGroupIndices.Reserve(MeshDescription->Polygons().Num());
	
	//TODO: support smooth group per edges, we currently do not import smooth group
	for (const FPolygonID& PolygonID : OriginalMeshDescription->Polygons().GetElementIDs())
	{
		SmoothingGroupIndices.Add(1);
	}

	TArray<FVector> OutTangentX;
	OutTangentX.AddZeroed(MeshDescription->VertexInstances().Num());
	TArray<FVector> OutTangentY;
	OutTangentY.AddZeroed(MeshDescription->VertexInstances().Num());
	TArray<FVector> OutTangentZ;
	OutTangentZ.AddZeroed(MeshDescription->VertexInstances().Num());

	for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		const TArray<FMeshTriangle>& MeshTriangles = MeshDescription->GetPolygon(PolygonID).Triangles;
		for (const FMeshTriangle& MeshTriangle : MeshTriangles)
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				GetVertexInstanceNTB(MeshDescription, VertexInstanceID, OutTangentZ[VertexInstanceID.GetValue()], OutTangentX[VertexInstanceID.GetValue()], OutTangentY[VertexInstanceID.GetValue()]);
			}
		}
	}
	// Declare these out here to avoid reallocations.
	TArray<FFanFace> RelevantFacesForCorner[3];
	TArray<int32> AdjacentFaces;
	TArray<int32> DupVerts;

	int32 NumWedges = MeshDescription->VertexInstances().Num();
	int32 NumFaces = NumWedges / 3;
	check(MeshDescription->Polygons().Num() == NumFaces);

	bool bWedgeTSpace = false;

	if (OutTangentX.Num() > 0 && OutTangentY.Num() > 0)
	{
		bWedgeTSpace = true;
		for (int32 WedgeIdx = 0; bWedgeTSpace && WedgeIdx < OutTangentX.Num() && WedgeIdx < OutTangentY.Num(); ++WedgeIdx)
		{
			bWedgeTSpace &= (!OutTangentX[WedgeIdx].IsNearlyZero()) && (!OutTangentY[WedgeIdx].IsNearlyZero());
		}
	}

	// Allocate storage for tangents if none were provided, and calculate normals for MikkTSpace.
	if (OutTangentZ.Num() != NumWedges)
	{
		// normals are not included, so we should calculate them
		OutTangentZ.Empty(NumWedges);
		OutTangentZ.AddZeroed(NumWedges);
	}

	// we need to calculate normals for MikkTSpace
	for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		const TArray<FMeshTriangle>& MeshTriangles = MeshDescription->GetPolygon(PolygonID).Triangles;
		for (const FMeshTriangle& MeshTriangle : MeshTriangles)
		{
			FVector CornerPositions[3];
			FVector CornerNormal[3];

			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				CornerNormal[CornerIndex] = FVector::ZeroVector;
				CornerPositions[CornerIndex] = GetVertexPositionFromVertexInstance(MeshDescription, VertexInstanceID.GetValue());
				RelevantFacesForCorner[CornerIndex].Reset();
			}

			// Don't process degenerate triangles.
			if (CornerPositions[0].Equals(CornerPositions[1], ComparisonThreshold)
				|| CornerPositions[0].Equals(CornerPositions[2], ComparisonThreshold)
				|| CornerPositions[1].Equals(CornerPositions[2], ComparisonThreshold))
			{
				continue;
			}

			// No need to process triangles if tangents already exist.
			bool bCornerHasNormal[3] = { 0 };
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				bCornerHasNormal[CornerIndex] = !OutTangentZ[VertexInstanceID.GetValue()].IsNearlyZero();
			}
			if (bCornerHasNormal[0] && bCornerHasNormal[1] && bCornerHasNormal[2])
			{
				continue;
			}

			// Start building a list of faces adjacent to this face.
			AdjacentFaces.Reset();
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				int32 ThisCornerIndex = VertexInstanceID.GetValue();
				DupVerts.Reset();
				OverlappingCorners.MultiFind(ThisCornerIndex, DupVerts);
				DupVerts.Add(ThisCornerIndex); // I am a "dup" of myself
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					AdjacentFaces.AddUnique(DupVerts[k] / 3);
				}
			}

			// We need to sort these here because the criteria for point equality is
			// exact, so we must ensure the exact same order for all dups.
			AdjacentFaces.Sort();

			// Process adjacent faces
			for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
			{
				int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
				for (int32 OurCornerIndex = 0; OurCornerIndex < 3; OurCornerIndex++)
				{
					if (bCornerHasNormal[OurCornerIndex])
						continue;

					FFanFace NewFanFace;
					int32 CommonIndexCount = 0;

					// Check for vertices in common.
					if (PolygonID.GetValue() == OtherFaceIndex)
					{
						CommonIndexCount = 3;
						NewFanFace.LinkedVertexIndex = OurCornerIndex;
					}
					else
					{
						// Check matching vertices against main vertex .
						for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
						{
							if (CornerPositions[OurCornerIndex].Equals(GetVertexPositionFromVertexInstance(MeshDescription, OtherFaceIndex * 3 + OtherCornerIndex), ComparisonThreshold))
							{
								CommonIndexCount++;
								NewFanFace.LinkedVertexIndex = OtherCornerIndex;
							}
						}
					}

					// Add if connected by at least one point. Smoothing matches are considered later.
					if (CommonIndexCount > 0)
					{
						NewFanFace.FaceIndex = OtherFaceIndex;
						NewFanFace.bFilled = (OtherFaceIndex == PolygonID.GetValue()); // Starter face for smoothing floodfill.
						NewFanFace.bBlendTangents = NewFanFace.bFilled;
						NewFanFace.bBlendNormals = NewFanFace.bFilled;
						RelevantFacesForCorner[OurCornerIndex].Add(NewFanFace);
					}
				}
			}

			// Find true relevance of faces for a vertex normal by traversing
			// smoothing-group-compatible connected triangle fans around common vertices.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				if (bCornerHasNormal[CornerIndex])
					continue;

				int32 NewConnections;
				do
				{
					NewConnections = 0;
					for (int32 OtherFaceIdx = 0; OtherFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); OtherFaceIdx++)
					{
						FFanFace& OtherFace = RelevantFacesForCorner[CornerIndex][OtherFaceIdx];
						// The vertex' own face is initially the only face with bFilled == true.
						if (OtherFace.bFilled)
						{
							for (int32 NextFaceIndex = 0; NextFaceIndex < RelevantFacesForCorner[CornerIndex].Num(); NextFaceIndex++)
							{
								FFanFace& NextFace = RelevantFacesForCorner[CornerIndex][NextFaceIndex];
								if (!NextFace.bFilled) // && !NextFace.bBlendTangents)
								{
									if ((NextFaceIndex != OtherFaceIdx)
										&& (SmoothingGroupIndices[NextFace.FaceIndex] & SmoothingGroupIndices[OtherFace.FaceIndex]))
									{
										int32 CommonVertices = 0;
										int32 CommonNormalVertices = 0;
										for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
										{
											for (int32 NextCornerIndex = 0; NextCornerIndex < 3; NextCornerIndex++)
											{
												int32 NextVertexIndex = MeshDescription->GetPolygon(FPolygonID(NextFace.FaceIndex)).Triangles[0].GetVertexInstanceID(NextCornerIndex).GetValue();
												int32 OtherVertexIndex = MeshDescription->GetPolygon(FPolygonID(OtherFace.FaceIndex)).Triangles[0].GetVertexInstanceID(OtherCornerIndex).GetValue();
												if (GetVertexPositionFromVertexInstance(MeshDescription, NextVertexIndex).Equals(GetVertexPositionFromVertexInstance(MeshDescription, OtherVertexIndex), ComparisonThreshold))
												{
													CommonVertices++;
													if (bBlendOverlappingNormals
														|| NextVertexIndex == OtherVertexIndex)
													{
														CommonNormalVertices++;
													}
												}
											}
										}
										// Flood fill faces with more than one common vertices which must be touching edges.
										if (CommonVertices > 1)
										{
											NextFace.bFilled = true;
											NextFace.bBlendNormals = (CommonNormalVertices > 1);
											NewConnections++;
										}
									}
								}
							}
						}
					}
				} while (NewConnections > 0);
			}

			// Vertex normal construction.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				if (bCornerHasNormal[CornerIndex])
				{
					CornerNormal[CornerIndex] = OutTangentZ[VertexInstanceID.GetValue()];
				}
				else
				{
					for (int32 RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); RelevantFaceIdx++)
					{
						FFanFace const& RelevantFace = RelevantFacesForCorner[CornerIndex][RelevantFaceIdx];
						if (RelevantFace.bFilled)
						{
							int32 OtherFaceIndex = RelevantFace.FaceIndex;
							if (RelevantFace.bBlendNormals)
							{
								CornerNormal[CornerIndex] += TriangleTangentZ[OtherFaceIndex];
							}
						}
					}
					if (!OutTangentZ[VertexInstanceID.GetValue()].IsZero())
					{
						CornerNormal[CornerIndex] = OutTangentZ[VertexInstanceID.GetValue()];
					}
				}
			}

			// Normalization.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				CornerNormal[CornerIndex].Normalize();
			}

			// Copy back to the outTangentZ (Normal).
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				OutTangentZ[VertexInstanceID.GetValue()] = CornerNormal[CornerIndex];
			}
		}
	}

	if (OutTangentX.Num() != NumWedges)
	{
		OutTangentX.Empty(NumWedges);
		OutTangentX.AddZeroed(NumWedges);
	}
	if (OutTangentY.Num() != NumWedges)
	{
		OutTangentY.Empty(NumWedges);
		OutTangentY.AddZeroed(NumWedges);
	}

	//Copy Back the OutTangent to the mesh description
	for (const FVertexInstanceID& VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
	{
		int32 VertexInstanceIDValue = VertexInstanceID.GetValue();
		SetVertexInstanceNTB(MeshDescription, VertexInstanceID, OutTangentZ[VertexInstanceIDValue], OutTangentX[VertexInstanceIDValue], OutTangentY[VertexInstanceIDValue]);
	}

	if (!bWedgeTSpace)
	{
		// we can use mikktspace to calculate the tangents
		SMikkTSpaceInterface MikkTInterface;
		MikkTInterface.m_getNormal = MeshDescriptionMikktSpaceInterface::MikkGetNormal;
		MikkTInterface.m_getNumFaces = MeshDescriptionMikktSpaceInterface::MikkGetNumFaces;
		MikkTInterface.m_getNumVerticesOfFace = MeshDescriptionMikktSpaceInterface::MikkGetNumVertsOfFace;
		MikkTInterface.m_getPosition = MeshDescriptionMikktSpaceInterface::MikkGetPosition;
		MikkTInterface.m_getTexCoord = MeshDescriptionMikktSpaceInterface::MikkGetTexCoord;
		MikkTInterface.m_setTSpaceBasic = MeshDescriptionMikktSpaceInterface::MikkSetTSpaceBasic;
		MikkTInterface.m_setTSpace = nullptr;

		SMikkTSpaceContext MikkTContext;
		MikkTContext.m_pInterface = &MikkTInterface;
		MikkTContext.m_pUserData = (void*)(MeshDescription);
		MikkTContext.m_bIgnoreDegenerates = bIgnoreDegenerateTriangles;
		genTangSpaceDefault(&MikkTContext);
	}

	check(OutTangentX.Num() == NumWedges);
	check(OutTangentY.Num() == NumWedges);
	check(OutTangentZ.Num() == NumWedges);
}

namespace MeshDescriptionMikktSpaceInterface
{
	int MikkGetNumFaces(const SMikkTSpaceContext* Context)
	{
		UMeshDescription *MeshDescription = (UMeshDescription*)(Context->m_pUserData);
		return MeshDescription->Polygons().Num();
	}

	int MikkGetNumVertsOfFace(const SMikkTSpaceContext* Context, const int FaceIdx)
	{
		// All of our meshes are triangles.
		return 3;
	}

	void MikkGetPosition(const SMikkTSpaceContext* Context, float Position[3], const int FaceIdx, const int VertIdx)
	{
		UMeshDescription *MeshDescription = (UMeshDescription*)(Context->m_pUserData);
		FVector VertexPosition = MeshDescription->GetVertex(MeshDescription->GetVertexInstance(FVertexInstanceID(FaceIdx * 3 + VertIdx)).VertexID).VertexPosition;
		Position[0] = VertexPosition.X;
		Position[1] = VertexPosition.Y;
		Position[2] = VertexPosition.Z;
	}

	void MikkGetNormal(const SMikkTSpaceContext* Context, float Normal[3], const int FaceIdx, const int VertIdx)
	{
		UMeshDescription *MeshDescription = (UMeshDescription*)(Context->m_pUserData);
		FVertexInstanceID VertexInstanceID(FaceIdx * 3 + VertIdx);
		const FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
		const FVector &VertexNormal = VertexInstance.Normal;
		for (int32 i = 0; i < 3; ++i)
		{
			Normal[i] = VertexNormal[i];
		}
	}

	void MikkSetTSpaceBasic(const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int FaceIdx, const int VertIdx)
	{
		UMeshDescription *MeshDescription = (UMeshDescription*)(Context->m_pUserData);
		FVertexInstanceID VertexInstanceID(FaceIdx * 3 + VertIdx);
		FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
		//Copy the tangents
		for (int32 i = 0; i < 3; ++i)
		{
			VertexInstance.Tangent[i] = Tangent[i];
		}
		//Copy the bi normal sign
		VertexInstance.BinormalSign = BitangentSign;
	}

	void MikkGetTexCoord(const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx)
	{
		UMeshDescription *MeshDescription = (UMeshDescription*)(Context->m_pUserData);
		const FVector2D &TexCoord = MeshDescription->VertexInstanceAttributes().GetAttribute<FVector2D>(FVertexInstanceID(FaceIdx * 3 + VertIdx), UEditableMeshAttribute::VertexTextureCoordinate(), 0);
		UV[0] = TexCoord.X;
		UV[1] = TexCoord.Y;
	}
}