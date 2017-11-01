// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MeshDescriptionHelper.h"

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "mikktspace.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "RenderUtils.h"
#include "LayoutUV.h"
#include "IMeshReductionInterfaces.h"

#include "IMeshReductionManagerModule.h"

DEFINE_LOG_CATEGORY(LogMeshBuilder);

//Enable all check
//#define ENABLE_NTB_CHECK

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
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Owner);
	check(StaticMesh);
	//Use the build settings to create the RenderMeshDescription
	UMeshDescription *RenderMeshDescription = NewObject<UMeshDescription>(Owner, NAME_None, RF_NoFlags);

	if (OriginalMeshDescription == nullptr)
	{
		//oups, we do not have a valid original mesh to create the render from
		return RenderMeshDescription;
	}
	//Copy The Original Mesh Description in the render mesh description
	CopyMeshDescription(const_cast<UMeshDescription*>(OriginalMeshDescription), RenderMeshDescription);
	float ComparisonThreshold = BuildSettings->bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;

	if (BuildSettings->bRecomputeTangents || BuildSettings->bRecomputeNormals)
	{
		for (const FVertexInstanceID& VertexInstanceID : RenderMeshDescription->VertexInstances().GetElementIDs())
		{
			FMeshVertexInstance& VertexInstance = RenderMeshDescription->GetVertexInstance(VertexInstanceID);
			// Dump normals and tangents if we are recomputing them.
			if (BuildSettings->bRecomputeTangents)
			{
				//Dump the tangents
				VertexInstance.BinormalSign = 0.0f;
				VertexInstance.Tangent = FVector(0.0f);
			}
			if (BuildSettings->bRecomputeNormals)
			{
				//Dump the normals
				VertexInstance.Normal = FVector(0.0f);
			}
		}
	}

	// Compute any missing normals or tangents.
	{
		// Static meshes always blend normals of overlapping corners.
		uint32 TangentOptions = FMeshDescriptionHelper::ETangentOptions::BlendOverlappingNormals;
		if (BuildSettings->bRemoveDegenerates)
		{
			// If removing degenerate triangles, ignore them when computing tangents.
			TangentOptions |= FMeshDescriptionHelper::ETangentOptions::IgnoreDegenerateTriangles;
		}

		//This function make sure the Polygon NTB are compute and also remove degenerated triangle from the render mesh description.
		CreatePolygonNTB(RenderMeshDescription, BuildSettings->bRemoveDegenerates ? SMALL_NUMBER : 0.0f);
		
		// Find overlapping corners to accelerate adjacency.
		FindOverlappingCorners(OverlappingCorners, RenderMeshDescription, ComparisonThreshold);
			
		//Keep the original mesh description NTBs if we do not rebuild the normals or tangents.
		bool bComputeTangentLegacy = !BuildSettings->bUseMikkTSpace && (BuildSettings->bRecomputeNormals || BuildSettings->bRecomputeTangents);
			
		//MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
		//We cannot use mikkt space with degenerated normals fallback on buitin.
		if (BuildSettings->bUseMikkTSpace && (BuildSettings->bRecomputeNormals || BuildSettings->bRecomputeTangents))
		{
			CreateNormals(RenderMeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions, false);
			CreateMikktTangents(RenderMeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions);
		}
		else
		{
			//Set the compute tangent to true when we do not build using mikkt space
			CreateNormals(RenderMeshDescription, (FMeshDescriptionHelper::ETangentOptions)TangentOptions, true);
		}
	}

	if (BuildSettings->bGenerateLightmapUVs && RenderMeshDescription->VertexInstances().Num() > 0)
	{
		if (!RenderMeshDescription->GetVertexInstance(FVertexInstanceID(0)).VertexUVs.IsValidIndex(BuildSettings->SrcLightmapIndex))
		{
			BuildSettings->SrcLightmapIndex = 0;
		}

		FLayoutUV Packer(RenderMeshDescription, BuildSettings->SrcLightmapIndex, BuildSettings->DstLightmapIndex, BuildSettings->MinLightmapResolution);
		Packer.SetVersion((FMeshDescriptionHelper::ELightmapUVVersion)(StaticMesh->LightmapUVVersion));

		Packer.FindCharts(OverlappingCorners);
		bool bPackSuccess = Packer.FindBestPacking();
		if (bPackSuccess)
		{
			Packer.CommitPackedUVs();
		}
	}

	return RenderMeshDescription;
}

void FMeshDescriptionHelper::ReduceLOD(const UMeshDescription* BaseMesh, UMeshDescription* DestMesh, const FMeshReductionSettings& ReductionSettings, const TMultiMap<int32, int32>& InOverlappingCorners)
{
	if (BaseMesh == nullptr || DestMesh == nullptr)
	{
		//UE_LOG(LogMeshUtilities, Error, TEXT("Mesh contains zero source models."));
		return;
	}

	IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = MeshReductionModule.GetStaticMeshReductionInterface();
	// Reduce this LOD mesh according to its reduction settings.

	if (!MeshReduction || (ReductionSettings.PercentTriangles >= 1.0f && ReductionSettings.MaxDeviation <= 0.0f))
	{
		return;
	}
	float MaxDeviation = ReductionSettings.MaxDeviation;
	MeshReduction->ReduceMeshDescription(DestMesh, MaxDeviation, BaseMesh, InOverlappingCorners, ReductionSettings);
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

void FMeshDescriptionHelper::FindOverlappingCorners(TMultiMap<int32, int32>& OverlappingCorners, const UMeshDescription* MeshDescription, float ComparisonThreshold)
{
	//Empty the old data
	OverlappingCorners.Reset();
	const int32 NumWedges = MeshDescription->VertexInstances().Num();

	// Create a list of vertex Z/index pairs
	TArray<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Reserve(NumWedges);
	const FVertexInstanceArray& VertexInstances = MeshDescription->VertexInstances();

	for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		const TArray<FMeshTriangle>& MeshTriangles = MeshDescription->GetPolygon(PolygonID).Triangles;
		for (const FMeshTriangle& MeshTriangle : MeshTriangles)
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const FVertexInstanceID& VertexInstanceID = MeshTriangle.GetVertexInstanceID(CornerIndex);
				const FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);

				new(VertIndexAndZ)FIndexAndZ(VertexInstanceID.GetValue(), MeshDescription->GetVertex(VertexInstance.VertexID).VertexPosition);
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

			const FVector& PositionA = MeshDescription->GetVertex(MeshDescription->GetVertexInstance(FVertexInstanceID(VertIndexAndZ[i].Index)).VertexID).VertexPosition;
			const FVector& PositionB = MeshDescription->GetVertex(MeshDescription->GetVertexInstance(FVertexInstanceID(VertIndexAndZ[j].Index)).VertexID).VertexPosition;

			if (PositionA.Equals(PositionB, ComparisonThreshold))
			{
				OverlappingCorners.Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
				OverlappingCorners.Add(VertIndexAndZ[j].Index, VertIndexAndZ[i].Index);
			}
		}
	}
}

void FMeshDescriptionHelper::FindOverlappingCorners(const UMeshDescription* MeshDescription, float ComparisonThreshold)
{
	FindOverlappingCorners(OverlappingCorners, MeshDescription, ComparisonThreshold);
}

const FVector& FMeshDescriptionHelper::GetVertexPositionFromVertexInstance(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID) const
{
	return MeshDescription->GetVertex(MeshDescription->GetVertexInstance(VertexInstanceID).VertexID).VertexPosition;
}

FVector2D &FMeshDescriptionHelper::GetVertexInstanceUV(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID, int32 UVLayer) const
{
	return MeshDescription->GetVertexInstance(VertexInstanceID).VertexUVs[UVLayer];
}

void FMeshDescriptionHelper::CreatePolygonNTB(UMeshDescription* MeshDescription, float ComparisonThreshold)
{
	TArray<FPolygonID> DegeneratePolygons;
	for (const FPolygonID& PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		FVector TangentX(0.0f);
		FVector TangentY(0.0f);
		FVector TangentZ(0.0f);

		FMeshPolygon& Polygon = MeshDescription->GetPolygon(PolygonID);
		const TArray<FMeshTriangle>& MeshTriangles = Polygon.Triangles;
#ifdef ENABLE_NTB_CHECK
		//Assume triangle are build
		check(MeshTriangles.Num() > 0);
#endif

		//We need only the first triangle since all triangle of a polygon must have the same normals (planar polygon)
		const FMeshTriangle& MeshTriangle = MeshTriangles[0];
		int32 UVIndex = 0;

		FVector P[3];
		for (int32 i = 0; i < 3; ++i)
		{
			P[i] = GetVertexPositionFromVertexInstance(MeshDescription, MeshTriangle.GetVertexInstanceID(i));
		}

		const FVector Normal = ((P[1] - P[2]) ^ (P[0] - P[2])).GetSafeNormal(ComparisonThreshold);
		//Check for degenerated polygons, avoid NAN
		if (!Normal.IsNearlyZero(ComparisonThreshold))
		{
			FMatrix	ParameterToLocal(
				FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
				FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
				FPlane(P[0].X, P[0].Y, P[0].Z, 0),
				FPlane(0, 0, 0, 1)
			);

			const FVector2D T1 = GetVertexInstanceUV(MeshDescription, MeshTriangle.GetVertexInstanceID(0), 0);
			const FVector2D T2 = GetVertexInstanceUV(MeshDescription, MeshTriangle.GetVertexInstanceID(1), 0);
			const FVector2D T3 = GetVertexInstanceUV(MeshDescription, MeshTriangle.GetVertexInstanceID(2), 0);

			FMatrix ParameterToTexture(
				FPlane(T2.X - T1.X, T2.Y - T1.Y, 0, 0),
				FPlane(T3.X - T1.X, T3.Y - T1.Y, 0, 0),
				FPlane(T1.X, T1.Y, 1, 0),
				FPlane(0, 0, 0, 1)
			);

			// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
			const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

			TangentX = TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal();
			TangentY = TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal();
			TangentZ = Normal;
			FVector::CreateOrthonormalBasis(TangentX, TangentY, TangentZ);
		}
		else
		{
			DegeneratePolygons.Add(PolygonID);
		}
		//Set the polygon Normal value only once if it is not set.
		if (Polygon.PolygonNormal.IsNearlyZero())
		{
			Polygon.PolygonTangent = TangentX;
			Polygon.PolygonBinormal = TangentY;
			Polygon.PolygonNormal = TangentZ;
		}
	}

	//Delete the degenerated polygons. The array is fill only if the remove degenerated option is turn on.
	if (DegeneratePolygons.Num() > 0)
	{
		TSet<FEdgeID> OrphanedEdges;
		TSet<FVertexInstanceID> OrphanedVertexInstances;
		TSet<FPolygonGroupID> OrphanedPolygonGroups;
		TSet<FVertexID> OrphanedVertices;
		for (FPolygonID& PolygonID : DegeneratePolygons)
		{
			MeshDescription->DeletePolygon(PolygonID, &OrphanedEdges, &OrphanedVertexInstances, &OrphanedPolygonGroups);
		}
		for (FPolygonGroupID& PolygonGroupID : OrphanedPolygonGroups)
		{
			MeshDescription->DeletePolygonGroup(PolygonGroupID);
		}
		for (FVertexInstanceID& VertexInstanceID : OrphanedVertexInstances)
		{
			MeshDescription->DeleteVertexInstance(VertexInstanceID, &OrphanedVertices);
		}
		for (FEdgeID& EdgeID : OrphanedEdges)
		{
			MeshDescription->DeleteEdge(EdgeID, &OrphanedVertices);
		}
		for (FVertexID& VertexID : OrphanedVertices)
		{
			MeshDescription->DeleteVertex(VertexID);
		}
		//Compact and Remap IDs so we have clean ID from 0 to n since we just erase some polygons
		//The render build need to have compact ID
		FElementIDRemappings RemappingInfos;
		MeshDescription->Compact(RemappingInfos);
	}
}

struct FVertexInfo
{
	FVertexInfo()
	{
		PolygonID = FPolygonID::Invalid;
		VertexInstanceID = FVertexInstanceID::Invalid;
		UVs = FVector2D(0.0f, 0.0f);
		EdgeIDs.Reserve(2);//Most of the time a edge has two triangles
	}

	FPolygonID PolygonID;
	FVertexInstanceID VertexInstanceID;
	FVector2D UVs;
	TArray<FEdgeID> EdgeIDs;
};

void RecursiveFillPolygonGroup(
	  UMeshDescription* MeshDescription
	, TMap<FPolygonID, FVertexInfo>& VertexInfoMap
	, TSet<FPolygonID>& CurrentGroup
	, FVertexInfo& CurrentVertexInfo
	, TSet<FPolygonID>& ConsumedPolygon)
{
	CurrentGroup.Add(CurrentVertexInfo.PolygonID);
	ConsumedPolygon.Add(CurrentVertexInfo.PolygonID);
	for (FEdgeID &EdgeID : CurrentVertexInfo.EdgeIDs)
	{
		FMeshEdge& Edge = MeshDescription->GetEdge(EdgeID);
		if (Edge.bIsHardEdge)
		{
			//End of the group
			continue;
		}
		for (const FPolygonID& PolygonID : Edge.ConnectedPolygons)
		{
			if (PolygonID == CurrentVertexInfo.PolygonID)
			{
				continue;
			}
			//Add this polygon to the group
			FVertexInfo& OtherVertexInfo = VertexInfoMap.FindOrAdd(PolygonID);
			//Do not repeat polygons
			if (!ConsumedPolygon.Contains(OtherVertexInfo.PolygonID))
			{
				RecursiveFillPolygonGroup(MeshDescription, VertexInfoMap, CurrentGroup, OtherVertexInfo, ConsumedPolygon);
			}
		}
	}
}

void FMeshDescriptionHelper::CreateNormals(UMeshDescription* MeshDescription, FMeshDescriptionHelper::ETangentOptions TangentOptions, bool bComputeTangent)
{
	//For each vertex compute the normals for every connected edges that are smooth betwween hard edges
	//         H   A    B
	//          \  ||  /
	//       G  -- ** -- C
	//          // |  \
	//         F   E    D
	//
	// The double ** are the vertex, the double line are hard edges, the single line are soft edge.
	// A and F are hard, all other edges are soft. The goal is to compute two average normals one from
	// A to F and a second from F to A. Then we can set the vertex instance normals accordingly.
	// First normal(A to F) = Normalize(A+B+C+D+E+F)
	// Second normal(F to A) = Normalize(F+G+H+A)
	// We found the connected edge using the triangle that share edges
	
	TMap<FPolygonID, FVertexInfo> VertexInfoMap;
	VertexInfoMap.Reserve(20);
	//Iterate all vertex to compute normals for all vertex instance
	for (const FVertexID& VertexID : MeshDescription->Vertices().GetElementIDs())
	{
		FMeshVertex& Vertex = MeshDescription->GetVertex(VertexID);

		VertexInfoMap.Reset();
		
		//Fill the VertexInfoMap
		for (const FEdgeID& EdgeID : Vertex.ConnectedEdgeIDs)
		{
			FMeshEdge& Edge = MeshDescription->GetEdge(EdgeID);
			for (const FPolygonID& PolygonID : Edge.ConnectedPolygons)
			{
				FVertexInfo& VertexInfo = VertexInfoMap.FindOrAdd(PolygonID);
				int32 EdgeIndex = VertexInfo.EdgeIDs.AddUnique(EdgeID);

				FMeshPolygon& Polygon = MeshDescription->GetPolygon(PolygonID);

				if (VertexInfo.PolygonID == FPolygonID::Invalid)
				{
					VertexInfo.PolygonID = PolygonID;
					for (FVertexInstanceID& VertexInstanceID : Polygon.PerimeterContour.VertexInstanceIDs)
					{
						FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
						if (VertexInstance.VertexID == VertexID)
						{
							VertexInfo.VertexInstanceID = VertexInstanceID;
							VertexInfo.UVs = VertexInstance.VertexUVs[0];
							break;
						}
					}
				}
			}
		}

		//Make sure we consume all our vertex instance
		check(VertexInfoMap.Num() == Vertex.VertexInstanceIDs.Num());

		//Build all group by recursively traverse all polygon connected to the vertex
		TArray<TSet<FPolygonID>> Groups;
		TSet<FPolygonID> ConsumedPolygon;
		for (auto Kvp : VertexInfoMap)
		{
			if (ConsumedPolygon.Contains(Kvp.Key))
			{
				continue;
			}

			int32 CurrentGroupIndex = Groups.AddZeroed();
			FVertexInfo& VertexInfo = VertexInfoMap.FindOrAdd(Kvp.Key);
			RecursiveFillPolygonGroup(MeshDescription, VertexInfoMap, Groups[CurrentGroupIndex], VertexInfo, ConsumedPolygon);
		}

		//Smooth every connected group
		ConsumedPolygon.Reset();
		for(TSet<FPolygonID> Group : Groups)
		{
			//Compute tangents data
			TMap<FVector2D, FVector> GroupTangent;
			TMap<FVector2D, FVector> GroupBiNormal;

			TArray<FVertexInstanceID> VertexInstanceInGroup;
			FVector GroupNormal(0.0f);
			for(FPolygonID& PolygonID : Group)
			{
#ifdef ENABLE_NTB_CHECK
				check(!ConsumedPolygon.Contains(PolygonID));
#endif
				ConsumedPolygon.Add(PolygonID);
				VertexInstanceInGroup.Add(VertexInfoMap[PolygonID].VertexInstanceID);
				FMeshPolygon& Polygon = MeshDescription->GetPolygon(PolygonID);
				GroupNormal += Polygon.PolygonNormal;
				if (bComputeTangent)
				{
					FVector2D& UVs = VertexInfoMap[PolygonID].UVs;
					bool CreateGroup = (!GroupTangent.Contains(UVs));
					FVector& GroupTangentValue = GroupTangent.FindOrAdd(UVs);
					FVector& GroupBiNormalValue = GroupBiNormal.FindOrAdd(UVs);
					GroupTangentValue = CreateGroup ? Polygon.PolygonTangent : GroupTangentValue + Polygon.PolygonTangent;
					GroupBiNormalValue = CreateGroup ? Polygon.PolygonBinormal : GroupBiNormalValue + Polygon.PolygonBinormal;
				}
			}

			//////////////////////////////////////////////////////////////////////////
			//Apply the group to the Mesh
			GroupNormal.Normalize();
			if (bComputeTangent)
			{
				for (auto Kvp : GroupTangent)
				{
					FVector& GroupTangentValue = GroupTangent.FindOrAdd(Kvp.Key);
					GroupTangentValue.Normalize();
				}
				for (auto Kvp : GroupBiNormal)
				{
					FVector& GroupBiNormalValue = GroupBiNormal.FindOrAdd(Kvp.Key);
					GroupBiNormalValue.Normalize();
				}
			}
			//Apply the average NTB on all Vertex instance
			for (FVertexInstanceID& VertexInstanceID : VertexInstanceInGroup)
			{
				FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstanceID);
				FVector2D& VertexUV = VertexInstance.VertexUVs[0];
			
				if (VertexInstance.Normal.IsNearlyZero(SMALL_NUMBER))
				{
					VertexInstance.Normal = GroupNormal;
				}
#ifdef ENABLE_NTB_CHECK
				check(!VertexInstance.Normal.IsNearlyZero(SMALL_NUMBER));
#endif
				if (bComputeTangent)
				{
					//Avoid changing the original group value
					FVector GroupTangentValue = GroupTangent[VertexUV];
					FVector GroupBiNormalValue = GroupBiNormal[VertexUV];

					if (!VertexInstance.Tangent.IsNearlyZero(SMALL_NUMBER))
					{
						GroupTangentValue = VertexInstance.Tangent;
					}
#ifdef ENABLE_NTB_CHECK
					check(!GroupTangentValue.IsNearlyZero(SMALL_NUMBER))
#endif
					FVector BiNormal(0.0f);
					if (!VertexInstance.Normal.IsNearlyZero(SMALL_NUMBER) && !VertexInstance.Tangent.IsNearlyZero(SMALL_NUMBER))
					{
						BiNormal = FVector::CrossProduct(VertexInstance.Normal, VertexInstance.Tangent).GetSafeNormal() * VertexInstance.BinormalSign;
					}
					if (!BiNormal.IsNearlyZero(SMALL_NUMBER))
					{
						GroupBiNormalValue = BiNormal;
					}
#ifdef ENABLE_NTB_CHECK
					check(!GroupBiNormalValue.IsNearlyZero(SMALL_NUMBER));
#endif
					// Gram-Schmidt orthogonalization
					GroupBiNormalValue -= GroupTangentValue * (GroupTangentValue | GroupBiNormalValue);
					GroupBiNormalValue.Normalize();
					
					GroupTangentValue -= VertexInstance.Normal * (VertexInstance.Normal | GroupTangentValue);
					GroupTangentValue.Normalize();
					
					GroupBiNormalValue -= VertexInstance.Normal * (VertexInstance.Normal | GroupBiNormalValue);
					GroupBiNormalValue.Normalize();
#ifdef ENABLE_NTB_CHECK
					check(!GroupTangentValue.IsNearlyZero(SMALL_NUMBER));
					check(!GroupBiNormalValue.IsNearlyZero(SMALL_NUMBER));
#endif
					//Set the value
					VertexInstance.Tangent = GroupTangentValue;
					//If the BiNormal is zero set the sign to 1.0f
					VertexInstance.BinormalSign = GetBasisDeterminantSign(GroupTangentValue, GroupBiNormalValue, VertexInstance.Normal);

				}
			}
		}
	}
}

void FMeshDescriptionHelper::CreateMikktTangents(UMeshDescription* MeshDescription, FMeshDescriptionHelper::ETangentOptions TangentOptions)
{
	bool bIgnoreDegenerateTriangles = (TangentOptions & FMeshDescriptionHelper::ETangentOptions::IgnoreDegenerateTriangles) != 0;
	float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

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
		VertexInstance.BinormalSign = -BitangentSign;
	}

	void MikkGetTexCoord(const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx)
	{
		UMeshDescription *MeshDescription = (UMeshDescription*)(Context->m_pUserData);
		const FVector2D &TexCoord = MeshDescription->GetVertexInstance(FVertexInstanceID(FaceIdx * 3 + VertIdx)).VertexUVs[0];
		UV[0] = TexCoord.X;
		UV[1] = TexCoord.Y;
	}
}