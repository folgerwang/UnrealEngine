// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshDescriptionOperations.h"
#include "UObject/Package.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "RawMesh.h"
#include "LayoutUV.h"
#include "OverlappingCorners.h"
#include "RenderUtils.h"
#include "mikktspace.h"
#include "UVMapSettings.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MeshDescriptionOperations)

DEFINE_LOG_CATEGORY(LogMeshDescriptionOperations);

#define LOCTEXT_NAMESPACE "MeshDescriptionOperations"

//////////////////////////////////////////////////////////////////////////
// Local structure
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

/** Helper struct for building acceleration structures. */
namespace MeshDescriptionOperationNamespace
{
	struct FIndexAndZ
	{
		float Z;
		int32 Index;
		const FVector *OriginalVector;

		/** Default constructor. */
		FIndexAndZ() {}

		/** Initialization constructor. */
		FIndexAndZ(int32 InIndex, const FVector& V)
		{
			Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
			Index = InIndex;
			OriginalVector = &V;
		}
	};
	/** Sorting function for vertex Z/index pairs. */
	struct FCompareIndexAndZ
	{
		FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
	};
}


//////////////////////////////////////////////////////////////////////////
// Converters

void FMeshDescriptionOperations::ConvertHardEdgesToSmoothGroup(const FMeshDescription& SourceMeshDescription, TArray<uint32>& FaceSmoothingMasks)
{
	TMap<FPolygonID, uint32> PolygonSmoothGroup;
	PolygonSmoothGroup.Reserve(SourceMeshDescription.Polygons().GetArraySize());
	TArray<bool> ConsumedPolygons;
	ConsumedPolygons.AddZeroed(SourceMeshDescription.Polygons().GetArraySize());

	TMap < FPolygonID, uint32> PolygonAvoidances;

	TEdgeAttributesConstRef<bool> EdgeHardnesses = SourceMeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	int32 TriangleCount = 0;
	for (const FPolygonID PolygonID : SourceMeshDescription.Polygons().GetElementIDs())
	{
		TriangleCount += SourceMeshDescription.GetPolygonTriangles(PolygonID).Num();
		if (ConsumedPolygons[PolygonID.GetValue()])
		{
			continue;
		}
		TArray<FPolygonID> ConnectedPolygons;
		TArray<FPolygonID> LastConnectedPolygons;
		ConnectedPolygons.Add(PolygonID);
		LastConnectedPolygons.Add(FPolygonID::Invalid);
		while (ConnectedPolygons.Num() > 0)
		{
			check(LastConnectedPolygons.Num() == ConnectedPolygons.Num());
			FPolygonID LastPolygonID = LastConnectedPolygons.Pop(true);
			FPolygonID CurrentPolygonID = ConnectedPolygons.Pop(true);
			if (ConsumedPolygons[CurrentPolygonID.GetValue()])
			{
				continue;
			}
			TArray<FPolygonID> SoftEdgeNeigbors;
			uint32& SmoothGroup = PolygonSmoothGroup.FindOrAdd(CurrentPolygonID);
			uint32 AvoidSmoothGroup = 0;
			uint32 NeighborSmoothGroup = 0;
			const uint32 LastSmoothGroupValue = (LastPolygonID == FPolygonID::Invalid) ? 0 : PolygonSmoothGroup[LastPolygonID];
			TArray<FEdgeID> PolygonEdges;
			SourceMeshDescription.GetPolygonEdges(CurrentPolygonID, PolygonEdges);
			for (const FEdgeID& EdgeID : PolygonEdges)
			{
				bool bIsHardEdge = EdgeHardnesses[EdgeID];
				const TArray<FPolygonID>& EdgeConnectedPolygons = SourceMeshDescription.GetEdgeConnectedPolygons(EdgeID);
				for (const FPolygonID& EdgePolygonID : EdgeConnectedPolygons)
				{
					if (EdgePolygonID == CurrentPolygonID)
					{
						continue;
					}
					uint32 SmoothValue = 0;
					if (PolygonSmoothGroup.Contains(EdgePolygonID))
					{
						SmoothValue = PolygonSmoothGroup[EdgePolygonID];
					}

					if (bIsHardEdge) //Hard Edge
					{
						AvoidSmoothGroup |= SmoothValue;
					}
					else
					{
						NeighborSmoothGroup |= SmoothValue;
						//Put all none hard edge polygon in the next iteration
						if (!ConsumedPolygons[EdgePolygonID.GetValue()])
						{
							ConnectedPolygons.Add(EdgePolygonID);
							LastConnectedPolygons.Add(CurrentPolygonID);
						}
						else
						{
							SoftEdgeNeigbors.Add(EdgePolygonID);
						}
					}
				}
			}

			if (AvoidSmoothGroup != 0)
			{
				PolygonAvoidances.FindOrAdd(CurrentPolygonID) = AvoidSmoothGroup;
				//find neighbor avoidance
				for (FPolygonID& NeighborID : SoftEdgeNeigbors)
				{
					if (!PolygonAvoidances.Contains(NeighborID))
					{
						continue;
					}
					AvoidSmoothGroup |= PolygonAvoidances[NeighborID];
				}
				uint32 NewSmoothGroup = 1;
				while ((NewSmoothGroup & AvoidSmoothGroup) != 0 && NewSmoothGroup < MAX_uint32)
				{
					//Shift the smooth group
					NewSmoothGroup = NewSmoothGroup << 1;
				}
				SmoothGroup = NewSmoothGroup;
				//Apply to all neighboard
				for (FPolygonID& NeighborID : SoftEdgeNeigbors)
				{
					PolygonSmoothGroup[NeighborID] |= NewSmoothGroup;
				}
			}
			else if (NeighborSmoothGroup != 0)
			{
				SmoothGroup |= LastSmoothGroupValue | NeighborSmoothGroup;
			}
			else
			{
				SmoothGroup = 1;
			}
			ConsumedPolygons[CurrentPolygonID.GetValue()] = true;
		}
	}
	//Set the smooth group in the FaceSmoothingMasks parameter
	check(FaceSmoothingMasks.Num() == TriangleCount);
	int32 TriangleIndex = 0;
	for (const FPolygonID PolygonID : SourceMeshDescription.Polygons().GetElementIDs())
	{
		uint32 PolygonSmoothValue = PolygonSmoothGroup[PolygonID];
		const TArray<FMeshTriangle>& Triangles = SourceMeshDescription.GetPolygonTriangles(PolygonID);
		for (const FMeshTriangle& MeshTriangle : Triangles)
		{
			FaceSmoothingMasks[TriangleIndex++] = PolygonSmoothValue;
		}
	}
}

void FMeshDescriptionOperations::ConvertSmoothGroupToHardEdges(const TArray<uint32>& FaceSmoothingMasks, FMeshDescription& DestinationMeshDescription)
{
	TEdgeAttributesRef<bool> EdgeHardnesses = DestinationMeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);

	TArray<bool> ConsumedPolygons;
	ConsumedPolygons.AddZeroed(DestinationMeshDescription.Polygons().Num());
	for (const FPolygonID PolygonID : DestinationMeshDescription.Polygons().GetElementIDs())
	{
		if (ConsumedPolygons[PolygonID.GetValue()])
		{
			continue;
		}
		TArray<FPolygonID> ConnectedPolygons;
		ConnectedPolygons.Add(PolygonID);
		while (ConnectedPolygons.Num() > 0)
		{
			FPolygonID CurrentPolygonID = ConnectedPolygons.Pop(true);
			int32 CurrentPolygonIDValue = CurrentPolygonID.GetValue();
			check(FaceSmoothingMasks.IsValidIndex(CurrentPolygonIDValue));
			const uint32 ReferenceSmoothGroup = FaceSmoothingMasks[CurrentPolygonIDValue];
			TArray<FEdgeID> PolygonEdges;
			DestinationMeshDescription.GetPolygonEdges(CurrentPolygonID, PolygonEdges);
			for (const FEdgeID& EdgeID : PolygonEdges)
			{
				const bool bIsHardEdge = EdgeHardnesses[EdgeID];
				if (bIsHardEdge)
				{
					continue;
				}
				const TArray<FPolygonID>& EdgeConnectedPolygons = DestinationMeshDescription.GetEdgeConnectedPolygons(EdgeID);
				for (const FPolygonID& EdgePolygonID : EdgeConnectedPolygons)
				{
					int32 EdgePolygonIDValue = EdgePolygonID.GetValue();
					if (EdgePolygonID == CurrentPolygonID || ConsumedPolygons[EdgePolygonIDValue])
					{
						continue;
					}
					check(FaceSmoothingMasks.IsValidIndex(EdgePolygonIDValue));
					const uint32 TestSmoothGroup = FaceSmoothingMasks[EdgePolygonIDValue];
					if ((TestSmoothGroup & ReferenceSmoothGroup) == 0)
					{
						EdgeHardnesses[EdgeID] = true;
						break;
					}
					else
					{
						ConnectedPolygons.Add(EdgePolygonID);
					}
				}
			}
			ConsumedPolygons[CurrentPolygonID.GetValue()] = true;
		}
	}
}

void FMeshDescriptionOperations::ConvertToRawMesh(const FMeshDescription& SourceMeshDescription, FRawMesh& DestinationRawMesh, const TMap<FName, int32>& MaterialMap)
{
	DestinationRawMesh.Empty();

	//Gather all array data
	TVertexAttributesConstRef<FVector> VertexPositions = SourceMeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	TVertexInstanceAttributesConstRef<FVector> VertexInstanceNormals = SourceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesConstRef<FVector> VertexInstanceTangents = SourceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = SourceMeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesConstRef<FVector4> VertexInstanceColors = SourceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = SourceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	TPolygonGroupAttributesConstRef<FName> PolygonGroupMaterialSlotName = SourceMeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	DestinationRawMesh.VertexPositions.AddZeroed(SourceMeshDescription.Vertices().Num());
	TArray<int32> RemapVerts;
	RemapVerts.AddZeroed(SourceMeshDescription.Vertices().GetArraySize());
	int32 VertexIndex = 0;
	for (const FVertexID& VertexID : SourceMeshDescription.Vertices().GetElementIDs())
	{
		DestinationRawMesh.VertexPositions[VertexIndex] = VertexPositions[VertexID];
		RemapVerts[VertexID.GetValue()] = VertexIndex;
		++VertexIndex;
	}

	int32 TriangleNumber = 0;
	for (const FPolygonID& PolygonID : SourceMeshDescription.Polygons().GetElementIDs())
	{
		TriangleNumber += SourceMeshDescription.GetPolygonTriangles(PolygonID).Num();
	}
	DestinationRawMesh.FaceMaterialIndices.AddZeroed(TriangleNumber);
	DestinationRawMesh.FaceSmoothingMasks.AddZeroed(TriangleNumber);

	bool bHasVertexColor = HasVertexColor(SourceMeshDescription);

	int32 WedgeIndexNumber = TriangleNumber * 3;
	if (bHasVertexColor)
	{
		DestinationRawMesh.WedgeColors.AddZeroed(WedgeIndexNumber);
	}
	DestinationRawMesh.WedgeIndices.AddZeroed(WedgeIndexNumber);
	DestinationRawMesh.WedgeTangentX.AddZeroed(WedgeIndexNumber);
	DestinationRawMesh.WedgeTangentY.AddZeroed(WedgeIndexNumber);
	DestinationRawMesh.WedgeTangentZ.AddZeroed(WedgeIndexNumber);
	int32 ExistingUVCount = VertexInstanceUVs.GetNumIndices();
	for (int32 UVIndex = 0; UVIndex < ExistingUVCount; ++UVIndex)
	{
		DestinationRawMesh.WedgeTexCoords[UVIndex].AddZeroed(WedgeIndexNumber);
	}

	int32 TriangleIndex = 0;
	int32 WedgeIndex = 0;
	for (const FPolygonID PolygonID : SourceMeshDescription.Polygons().GetElementIDs())
	{
		const FPolygonGroupID& PolygonGroupID = SourceMeshDescription.GetPolygonPolygonGroup(PolygonID);
		int32 PolygonIDValue = PolygonID.GetValue();
		const TArray<FMeshTriangle>& Triangles = SourceMeshDescription.GetPolygonTriangles(PolygonID);
		for (const FMeshTriangle& MeshTriangle : Triangles)
		{
			if (MaterialMap.Num() > 0 && MaterialMap.Contains(PolygonGroupMaterialSlotName[PolygonGroupID]))
			{
				DestinationRawMesh.FaceMaterialIndices[TriangleIndex] = MaterialMap[PolygonGroupMaterialSlotName[PolygonGroupID]];
			}
			else
			{
				DestinationRawMesh.FaceMaterialIndices[TriangleIndex] = PolygonGroupID.GetValue();
			}
			DestinationRawMesh.FaceSmoothingMasks[TriangleIndex] = 0; //Conversion of soft/hard to smooth mask is done after the geometry is converted
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVertexInstanceID VertexInstanceID = MeshTriangle.GetVertexInstanceID(Corner);

				if (bHasVertexColor)
				{
					DestinationRawMesh.WedgeColors[WedgeIndex] = FLinearColor(VertexInstanceColors[VertexInstanceID]).ToFColor(true);
				}
				DestinationRawMesh.WedgeIndices[WedgeIndex] = RemapVerts[SourceMeshDescription.GetVertexInstanceVertex(VertexInstanceID).GetValue()];
				DestinationRawMesh.WedgeTangentX[WedgeIndex] = VertexInstanceTangents[VertexInstanceID];
				DestinationRawMesh.WedgeTangentY[WedgeIndex] = FVector::CrossProduct(VertexInstanceNormals[VertexInstanceID], VertexInstanceTangents[VertexInstanceID]).GetSafeNormal() * VertexInstanceBinormalSigns[VertexInstanceID];
				DestinationRawMesh.WedgeTangentZ[WedgeIndex] = VertexInstanceNormals[VertexInstanceID];
				for (int32 UVIndex = 0; UVIndex < ExistingUVCount; ++UVIndex)
				{
					DestinationRawMesh.WedgeTexCoords[UVIndex][WedgeIndex] = VertexInstanceUVs.Get(VertexInstanceID, UVIndex);
				}
				++WedgeIndex;
			}
			++TriangleIndex;
		}
	}
	//Convert the smoothgroup
	ConvertHardEdgesToSmoothGroup(SourceMeshDescription, DestinationRawMesh.FaceSmoothingMasks);
}

//We want to fill the FMeshDescription vertex position mesh attribute with the FRawMesh vertex position
//We will also weld the vertex position (old FRawMesh is not always welded) and construct a mapping array to match the FVertexID
void FillMeshDescriptionVertexPositionNoDuplicate(const TArray<FVector>& RawMeshVertexPositions, FMeshDescription& DestinationMeshDescription, TArray<FVertexID>& RemapVertexPosition)
{
	TVertexAttributesRef<FVector> VertexPositions = DestinationMeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	const int32 NumVertex = RawMeshVertexPositions.Num();

	TMap<int32, int32> TempRemapVertexPosition;
	TempRemapVertexPosition.Reserve(NumVertex);

	// Create a list of vertex Z/index pairs
	TArray<MeshDescriptionOperationNamespace::FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Reserve(NumVertex);

	for (int32 VertexIndex = 0; VertexIndex < NumVertex; ++VertexIndex)
	{
		new(VertIndexAndZ)MeshDescriptionOperationNamespace::FIndexAndZ(VertexIndex, RawMeshVertexPositions[VertexIndex]);
	}

	// Sort the vertices by z value
	VertIndexAndZ.Sort(MeshDescriptionOperationNamespace::FCompareIndexAndZ());

	int32 VertexCount = 0;
	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		int32 Index_i = VertIndexAndZ[i].Index;
		if (TempRemapVertexPosition.Contains(Index_i))
		{
			continue;
		}
		TempRemapVertexPosition.FindOrAdd(Index_i) = VertexCount;
		// only need to search forward, since we add pairs both ways
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > SMALL_NUMBER)
				break; // can't be any more dups

			const FVector& PositionA = *(VertIndexAndZ[i].OriginalVector);
			const FVector& PositionB = *(VertIndexAndZ[j].OriginalVector);

			if (PositionA.Equals(PositionB, SMALL_NUMBER))
			{
				TempRemapVertexPosition.FindOrAdd(VertIndexAndZ[j].Index) = VertexCount;
			}
		}
		VertexCount++;
	}

	//Make sure the vertex are added in the same order to be lossless when converting the FRawMesh
	//In case there is a duplicate even reordering it will not be lossless, but MeshDescription do not support
	//bad data like duplicated vertex position.
	RemapVertexPosition.AddUninitialized(NumVertex);
	DestinationMeshDescription.ReserveNewVertices(VertexCount);
	TArray<FVertexID> UniqueVertexDone;
	UniqueVertexDone.AddUninitialized(VertexCount);
	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		UniqueVertexDone[VertexIndex] = FVertexID::Invalid;
	}
	for (int32 VertexIndex = 0; VertexIndex < NumVertex; ++VertexIndex)
	{
		int32 RealIndex = TempRemapVertexPosition[VertexIndex];
		if (UniqueVertexDone[RealIndex] != FVertexID::Invalid)
		{
			RemapVertexPosition[VertexIndex] = UniqueVertexDone[RealIndex];
			continue;
		}
		FVertexID VertexID = DestinationMeshDescription.CreateVertex();
		UniqueVertexDone[RealIndex] = VertexID;
		VertexPositions[VertexID] = RawMeshVertexPositions[VertexIndex];
		RemapVertexPosition[VertexIndex] = VertexID;
	}
}

//Discover degenerated triangle
bool IsTriangleDegenerated(const FRawMesh& SourceRawMesh, const TArray<FVertexID>& RemapVertexPosition, const int32 VerticeIndexBase)
{
	FVertexID VertexIDs[3];
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		int32 VerticeIndex = VerticeIndexBase + Corner;
		VertexIDs[Corner] = RemapVertexPosition[SourceRawMesh.WedgeIndices[VerticeIndex]];
	}
	return (VertexIDs[0] == VertexIDs[1] || VertexIDs[0] == VertexIDs[2] || VertexIDs[1] == VertexIDs[2]);
}

void FMeshDescriptionOperations::ConvertFromRawMesh(const FRawMesh& SourceRawMesh, FMeshDescription& DestinationMeshDescription, const TMap<int32, FName>& MaterialMap)
{
	DestinationMeshDescription.Empty();

	DestinationMeshDescription.ReserveNewVertexInstances(SourceRawMesh.WedgeIndices.Num());
	DestinationMeshDescription.ReserveNewPolygons(SourceRawMesh.WedgeIndices.Num() / 3);
	//Approximately 2.5 edges per polygons
	DestinationMeshDescription.ReserveNewEdges(SourceRawMesh.WedgeIndices.Num() * 2.5f / 3);

	//Gather all array data
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = DestinationMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = DestinationMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = DestinationMeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = DestinationMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = DestinationMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = DestinationMeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	int32 NumTexCoords = 0;
	int32 MaxTexCoords = MAX_MESH_TEXTURE_COORDS;
	TArray<int32> TextureCoordinnateRemapIndex;
	TextureCoordinnateRemapIndex.AddZeroed(MaxTexCoords);
	for (int32 TextureCoordinnateIndex = 0; TextureCoordinnateIndex < MaxTexCoords; ++TextureCoordinnateIndex)
	{
		TextureCoordinnateRemapIndex[TextureCoordinnateIndex] = INDEX_NONE;
		if (SourceRawMesh.WedgeTexCoords[TextureCoordinnateIndex].Num() == SourceRawMesh.WedgeIndices.Num())
		{
			TextureCoordinnateRemapIndex[TextureCoordinnateIndex] = NumTexCoords;
			NumTexCoords++;
		}
	}
	VertexInstanceUVs.SetNumIndices(NumTexCoords);

	//Ensure we do not have any duplicate, We found all duplicated vertex and compact them and build a remap indice array to remap the wedgeindices
	TArray<FVertexID> RemapVertexPosition;
	FillMeshDescriptionVertexPositionNoDuplicate(SourceRawMesh.VertexPositions, DestinationMeshDescription, RemapVertexPosition);

	bool bHasColors = SourceRawMesh.WedgeColors.Num() > 0;
	bool bHasTangents = SourceRawMesh.WedgeTangentX.Num() > 0 && SourceRawMesh.WedgeTangentY.Num() > 0;
	bool bHasNormals = SourceRawMesh.WedgeTangentZ.Num() > 0;

	TArray<FPolygonGroupID> PolygonGroups;
	TMap<int32, FPolygonGroupID> MaterialIndexToPolygonGroup;

	//Create the PolygonGroups
	for(int32 MaterialIndex : SourceRawMesh.FaceMaterialIndices)
	{
		if (!MaterialIndexToPolygonGroup.Contains(MaterialIndex))
		{
			FPolygonGroupID PolygonGroupID(MaterialIndex);
			DestinationMeshDescription.CreatePolygonGroupWithID(PolygonGroupID);
			if (MaterialMap.Contains(MaterialIndex))
			{
				PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = MaterialMap[MaterialIndex];
			}
			else
			{
				PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = FName(*FString::Printf(TEXT("MaterialSlot_%d"), MaterialIndex));
			}
			PolygonGroups.Add(PolygonGroupID);
			MaterialIndexToPolygonGroup.Add(MaterialIndex, PolygonGroupID);
		}
	}

	//Triangles
	int32 TriangleCount = SourceRawMesh.WedgeIndices.Num() / 3;
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		int32 VerticeIndexBase = TriangleIndex * 3;
		//Check if the triangle is degenerated and skip the data if its the case
		if (IsTriangleDegenerated(SourceRawMesh, RemapVertexPosition, VerticeIndexBase))
		{
			continue;
		}

		//PolygonGroup
		FPolygonGroupID PolygonGroupID = FPolygonGroupID::Invalid;
		FName PolygonGroupImportedMaterialSlotName = NAME_None;
		int32 MaterialIndex = SourceRawMesh.FaceMaterialIndices[TriangleIndex];
		if (MaterialIndexToPolygonGroup.Contains(MaterialIndex))
		{
			PolygonGroupID = MaterialIndexToPolygonGroup[MaterialIndex];
		}
		else if (MaterialMap.Num() > 0 && MaterialMap.Contains(MaterialIndex))
		{
			PolygonGroupImportedMaterialSlotName = MaterialMap[MaterialIndex];
			for (const FPolygonGroupID& SearchPolygonGroupID : DestinationMeshDescription.PolygonGroups().GetElementIDs())
			{
				if (PolygonGroupImportedMaterialSlotNames[SearchPolygonGroupID] == PolygonGroupImportedMaterialSlotName)
				{
					PolygonGroupID = SearchPolygonGroupID;
					break;
				}
			}
		}

		if (PolygonGroupID == FPolygonGroupID::Invalid)
		{
			PolygonGroupID = DestinationMeshDescription.CreatePolygonGroup();
			PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = PolygonGroupImportedMaterialSlotName == NAME_None ? FName(*FString::Printf(TEXT("MaterialSlot_%d"), MaterialIndex)) : PolygonGroupImportedMaterialSlotName;
			PolygonGroups.Add(PolygonGroupID);
			MaterialIndexToPolygonGroup.Add(MaterialIndex, PolygonGroupID);
		}
		TArray<FVertexInstanceID> TriangleVertexInstanceIDs;
		TriangleVertexInstanceIDs.SetNum(3);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			int32 VerticeIndex = VerticeIndexBase + Corner;
			FVertexID VertexID = RemapVertexPosition[SourceRawMesh.WedgeIndices[VerticeIndex]];
			FVertexInstanceID VertexInstanceID = DestinationMeshDescription.CreateVertexInstance(VertexID);
			TriangleVertexInstanceIDs[Corner] = VertexInstanceID;
			VertexInstanceColors[VertexInstanceID] = bHasColors ? FLinearColor::FromSRGBColor(SourceRawMesh.WedgeColors[VerticeIndex]) : FLinearColor::White;
			VertexInstanceTangents[VertexInstanceID] = bHasTangents ? SourceRawMesh.WedgeTangentX[VerticeIndex] : FVector(ForceInitToZero);
			VertexInstanceBinormalSigns[VertexInstanceID] = bHasTangents ? GetBasisDeterminantSign(SourceRawMesh.WedgeTangentX[VerticeIndex].GetSafeNormal(), SourceRawMesh.WedgeTangentY[VerticeIndex].GetSafeNormal(), SourceRawMesh.WedgeTangentZ[VerticeIndex].GetSafeNormal()) : 0.0f;
			VertexInstanceNormals[VertexInstanceID] = bHasNormals ? SourceRawMesh.WedgeTangentZ[VerticeIndex] : FVector(ForceInitToZero);
			for (int32 TextureCoordinnateIndex = 0; TextureCoordinnateIndex < NumTexCoords; ++TextureCoordinnateIndex)
			{
				int32 TextureCoordIndex = TextureCoordinnateRemapIndex[TextureCoordinnateIndex];
				if (TextureCoordIndex == INDEX_NONE)
				{
					continue;
				}
				VertexInstanceUVs.Set(VertexInstanceID, TextureCoordIndex, SourceRawMesh.WedgeTexCoords[TextureCoordinnateIndex][VerticeIndex]);
			}
		}

		const FPolygonID NewPolygonID = DestinationMeshDescription.CreatePolygon(PolygonGroupID, TriangleVertexInstanceIDs);
		int32 NewTriangleIndex = DestinationMeshDescription.GetPolygonTriangles(NewPolygonID).AddDefaulted();
		FMeshTriangle& NewTriangle = DestinationMeshDescription.GetPolygonTriangles(NewPolygonID)[NewTriangleIndex];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			FVertexInstanceID VertexInstanceID = TriangleVertexInstanceIDs[Corner];
			NewTriangle.SetVertexInstanceID(Corner, VertexInstanceID);
		}
	}

	ConvertSmoothGroupToHardEdges(SourceRawMesh.FaceSmoothingMasks, DestinationMeshDescription);

	//Create the missing normals and tangents, should we use Mikkt space for tangent???
	if (!bHasNormals || !bHasTangents)
	{
		//DestinationMeshDescription.ComputePolygonTangentsAndNormals(0.0f);
		FMeshDescriptionOperations::CreatePolygonNTB(DestinationMeshDescription, 0.0f);

		//EComputeNTBsOptions ComputeNTBsOptions = (bHasNormals ? EComputeNTBsOptions::None : EComputeNTBsOptions::Normals) | (bHasTangents ? EComputeNTBsOptions::None : EComputeNTBsOptions::Tangents);
		//DestinationMeshDescription.ComputeTangentsAndNormals(ComputeNTBsOptions);
		//Create the missing normals and tangents
		if (!bHasNormals)
		{
			CreateNormals(DestinationMeshDescription, ETangentOptions::BlendOverlappingNormals, false);
		}
		CreateMikktTangents(DestinationMeshDescription, ETangentOptions::BlendOverlappingNormals);
	}
}

void FMeshDescriptionOperations::AppendMeshDescription(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, FAppendSettings& AppendSettings)
{
	//Vertex Attributes
	TVertexAttributesConstRef<FVector> SourceVertexPositions = SourceMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexAttributesConstRef<float> SourceVertexCornerSharpness = SourceMesh.VertexAttributes().GetAttributesRef<float>(MeshAttribute::Vertex::CornerSharpness);

	TVertexAttributesRef<FVector> TargetVertexPositions = TargetMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexAttributesRef<float> TargetVertexCornerSharpness = TargetMesh.VertexAttributes().GetAttributesRef<float>(MeshAttribute::Vertex::CornerSharpness);

	//Edge Attributes
	TEdgeAttributesConstRef<bool> SourceEdgeHardnesses = SourceMesh.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesConstRef<float> SourceEdgeCreaseSharpnesses = SourceMesh.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);

	TEdgeAttributesRef<bool> TargetEdgeHardnesses = TargetMesh.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> TargetEdgeCreaseSharpnesses = TargetMesh.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);

	//PolygonGroup Attributes
	TPolygonGroupAttributesConstRef<FName> SourceImportedMaterialSlotNames = SourceMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	TPolygonGroupAttributesRef<FName> TargetImportedMaterialSlotNames = TargetMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	//VertexInstance Attributes
	TVertexInstanceAttributesConstRef<FVector> SourceVertexInstanceNormals = SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesConstRef<FVector> SourceVertexInstanceTangents = SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesConstRef<float> SourceVertexInstanceBinormalSigns = SourceMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesConstRef<FVector4> SourceVertexInstanceColors = SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesConstRef<FVector2D> SourceVertexInstanceUVs = SourceMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	TVertexInstanceAttributesRef<FVector> TargetVertexInstanceNormals = TargetMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> TargetVertexInstanceTangents = TargetMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> TargetVertexInstanceBinormalSigns = TargetMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> TargetVertexInstanceColors = TargetMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> TargetVertexInstanceUVs = TargetMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	//Copy into the target mesh
	TargetMesh.ReserveNewVertices(SourceMesh.Vertices().Num());
	TargetMesh.ReserveNewVertexInstances(SourceMesh.VertexInstances().Num());
	TargetMesh.ReserveNewEdges(SourceMesh.Edges().Num());
	TargetMesh.ReserveNewPolygons(SourceMesh.Polygons().Num());

	if (SourceVertexInstanceUVs.GetNumIndices() > TargetVertexInstanceUVs.GetNumIndices())
	{
		TargetVertexInstanceUVs.SetNumIndices(SourceVertexInstanceUVs.GetNumIndices());
	}

	//PolygonGroups
	PolygonGroupMap RemapPolygonGroup;
	if (AppendSettings.PolygonGroupsDelegate.IsBound())
	{
		AppendSettings.PolygonGroupsDelegate.Execute(SourceMesh, TargetMesh, RemapPolygonGroup);
	}
	else
	{
		for (FPolygonGroupID SourcePolygonGroupID : SourceMesh.PolygonGroups().GetElementIDs())
		{
			FPolygonGroupID TargetMatchingID = FPolygonGroupID::Invalid;
			for (FPolygonGroupID TargetPolygonGroupID : TargetMesh.PolygonGroups().GetElementIDs())
			{
				if (SourceImportedMaterialSlotNames[SourcePolygonGroupID] == TargetImportedMaterialSlotNames[TargetPolygonGroupID])
				{
					TargetMatchingID = TargetPolygonGroupID;
					break;
				}
			}
			if (TargetMatchingID == FPolygonGroupID::Invalid)
			{
				TargetMatchingID = TargetMesh.CreatePolygonGroup();
				TargetImportedMaterialSlotNames[TargetMatchingID] = SourceImportedMaterialSlotNames[SourcePolygonGroupID];
			}
			RemapPolygonGroup.Add(SourcePolygonGroupID, TargetMatchingID);
		}
	}

	//Vertices
	TMap<FVertexID, FVertexID> SourceVertexIDRemap;
	SourceVertexIDRemap.Reserve(SourceMesh.Vertices().Num());
	for (FVertexID SourceVertexID : SourceMesh.Vertices().GetElementIDs())
	{
		FVertexID TargetVertexID = TargetMesh.CreateVertex();
		TargetVertexPositions[TargetVertexID] = (SourceVertexPositions[SourceVertexID]-AppendSettings.MergedAssetPivot);
		TargetVertexCornerSharpness[TargetVertexID] = SourceVertexCornerSharpness[SourceVertexID];
		SourceVertexIDRemap.Add(SourceVertexID, TargetVertexID);
	}

	// Transform vertices properties
	if (AppendSettings.MeshTransform)
	{
		const FTransform& Transform = AppendSettings.MeshTransform.GetValue();
		for (const TPair<FVertexID, FVertexID>& VertexIDPair : SourceVertexIDRemap)
		{
			FVector& Position = TargetVertexPositions[VertexIDPair.Value];
			Position = Transform.TransformPosition(Position);
		}
	}

	//Edges
	TMap<FEdgeID, FEdgeID> SourceEdgeIDRemap;
	SourceEdgeIDRemap.Reserve(SourceMesh.Edges().Num());
	for (const FEdgeID& SourceEdgeID : SourceMesh.Edges().GetElementIDs())
	{
		const FMeshEdge& SourceEdge = SourceMesh.GetEdge(SourceEdgeID);
		FEdgeID TargetEdgeID = TargetMesh.CreateEdge(SourceVertexIDRemap[SourceEdge.VertexIDs[0]], SourceVertexIDRemap[SourceEdge.VertexIDs[1]]);
		TargetEdgeHardnesses[TargetEdgeID] = SourceEdgeHardnesses[SourceEdgeID];
		TargetEdgeCreaseSharpnesses[TargetEdgeID] = SourceEdgeCreaseSharpnesses[SourceEdgeID];
		SourceEdgeIDRemap.Add(SourceEdgeID, TargetEdgeID);
	}

	//VertexInstances
	TMap<FVertexInstanceID, FVertexInstanceID> SourceVertexInstanceIDRemap;
	SourceVertexInstanceIDRemap.Reserve(SourceMesh.VertexInstances().Num());
	for (const FVertexInstanceID& SourceVertexInstanceID : SourceMesh.VertexInstances().GetElementIDs())
	{
		FVertexInstanceID TargetVertexInstanceID = TargetMesh.CreateVertexInstance(SourceVertexIDRemap[SourceMesh.GetVertexInstanceVertex(SourceVertexInstanceID)]);
		SourceVertexInstanceIDRemap.Add(SourceVertexInstanceID, TargetVertexInstanceID);

		TargetVertexInstanceNormals[TargetVertexInstanceID] = SourceVertexInstanceNormals[SourceVertexInstanceID];
		TargetVertexInstanceTangents[TargetVertexInstanceID] = SourceVertexInstanceTangents[SourceVertexInstanceID];
		TargetVertexInstanceBinormalSigns[TargetVertexInstanceID] = SourceVertexInstanceBinormalSigns[SourceVertexInstanceID];

		if (AppendSettings.bMergeVertexColor)
		{
			TargetVertexInstanceColors[TargetVertexInstanceID] = SourceVertexInstanceColors[SourceVertexInstanceID];
		}

		for (int32 UVChannelIndex = 0; UVChannelIndex < SourceVertexInstanceUVs.GetNumIndices(); ++UVChannelIndex)
		{
			TargetVertexInstanceUVs.Set(TargetVertexInstanceID, UVChannelIndex, SourceVertexInstanceUVs.Get(SourceVertexInstanceID, UVChannelIndex));
		}
	}

	// Transform vertex instances properties
	if (AppendSettings.MeshTransform)
	{
		const FTransform& Transform = AppendSettings.MeshTransform.GetValue();
		bool bFlipBinormal = Transform.GetDeterminant() < 0;
		float BinormalSignsFactor = bFlipBinormal ? -1.f : 1.f;
		for (const TPair<FVertexInstanceID, FVertexInstanceID>& VertexInstanceIDPair : SourceVertexInstanceIDRemap)
		{
			FVertexInstanceID InstanceID = VertexInstanceIDPair.Value;

			FVector& Normal = TargetVertexInstanceNormals[InstanceID];
			Normal = Transform.TransformVectorNoScale(Normal);

			FVector& Tangent = TargetVertexInstanceTangents[InstanceID];
			Tangent = Transform.TransformVectorNoScale(Tangent);

			TargetVertexInstanceBinormalSigns[InstanceID] *= BinormalSignsFactor;
		}
	}

	//Polygons
	for (const FPolygonID& SourcePolygonID : SourceMesh.Polygons().GetElementIDs())
	{
		const FMeshPolygon& SourcePolygon = SourceMesh.GetPolygon(SourcePolygonID);
		//Find the polygonGroupID
		FPolygonGroupID TargetPolygonGroupID = RemapPolygonGroup[SourcePolygon.PolygonGroupID];

		int32 PolygonVertexCount = SourcePolygon.PerimeterContour.VertexInstanceIDs.Num();
		TArray<FVertexInstanceID> VertexInstanceIDs;
		VertexInstanceIDs.Reserve(PolygonVertexCount);
		for (const FVertexInstanceID VertexInstanceID : SourcePolygon.PerimeterContour.VertexInstanceIDs)
		{
			VertexInstanceIDs.Add(SourceVertexInstanceIDRemap[VertexInstanceID]);
		}
		// Insert a polygon into the mesh
		const FPolygonID TargetPolygonID = TargetMesh.CreatePolygon(TargetPolygonGroupID, VertexInstanceIDs);
		//Triangulate the polygon
		FMeshPolygon& Polygon = TargetMesh.GetPolygon(TargetPolygonID);
		TargetMesh.ComputePolygonTriangulation(TargetPolygonID, Polygon.Triangles);
	}
}

//////////////////////////////////////////////////////////////////////////
// Normals tangents and Bi-normals
void FMeshDescriptionOperations::RecomputeNormalsAndTangentsIfNeeded(FMeshDescription& MeshDescription, ETangentOptions TangentOptions, bool bUseMikkTSpace, bool bForceRecomputeNormals, bool bForceRecomputeTangents)
{
	bool bRecomputeNormals = bForceRecomputeNormals;
	bool bRecomputeTangents = bForceRecomputeTangents;
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	if (!bRecomputeNormals || !bRecomputeTangents)
	{
		for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
		{
			bRecomputeNormals |= (VertexInstanceNormals[VertexInstanceID].IsNearlyZero() || VertexInstanceNormals[VertexInstanceID].ContainsNaN());
			bRecomputeTangents |= (VertexInstanceTangents[VertexInstanceID].IsNearlyZero() || VertexInstanceTangents[VertexInstanceID].ContainsNaN());
			if (bRecomputeNormals && bRecomputeTangents)
			{
				break;
			}
		}
	}

	if (bRecomputeNormals || bRecomputeTangents)
	{
		//Zero out all value that need to be recompute
		for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
		{
			if (bRecomputeNormals)
			{
				VertexInstanceNormals[VertexInstanceID] = FVector::ZeroVector;
			}
			if (bRecomputeTangents)
			{
				VertexInstanceTangents[VertexInstanceID] = FVector::ZeroVector;
			}
		}


		if (bRecomputeNormals)
		{
			FMeshDescriptionOperations::CreateNormals(MeshDescription, TangentOptions, bUseMikkTSpace ? false : bRecomputeTangents);
		}
		if (bUseMikkTSpace && bRecomputeTangents)
		{
			FMeshDescriptionOperations::CreateMikktTangents(MeshDescription, TangentOptions);
		}
	}
}

void FMeshDescriptionOperations::CreatePolygonNTB(FMeshDescription& MeshDescription, float ComparisonThreshold)
{
	MeshDescription.PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Normal, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient);
	MeshDescription.PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Tangent, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient);
	MeshDescription.PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Binormal, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient);
	MeshDescription.PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Center, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient);

	TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexInstanceAttributesConstRef<FVector2D> VertexUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	TPolygonAttributesRef<FVector> PolygonNormals = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Normal);
	TPolygonAttributesRef<FVector> PolygonTangents = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Tangent);
	TPolygonAttributesRef<FVector> PolygonBinormals = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Binormal);
	TPolygonAttributesRef<FVector> PolygonCenters = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Center);

	FVertexInstanceArray& VertexInstanceArray = MeshDescription.VertexInstances();
	FVertexArray& VertexArray = MeshDescription.Vertices();
	FPolygonArray& PolygonArray = MeshDescription.Polygons();

	for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
	{
		if (!PolygonNormals[PolygonID].IsNearlyZero())
		{
			//By pass normal calculation if its already done
			continue;
		}
		const TArray<FMeshTriangle>& MeshTriangles = MeshDescription.GetPolygonTriangles(PolygonID);
		FVector TangentX(0.0f);
		FVector TangentY(0.0f);
		FVector TangentZ(0.0f);
		for (const FMeshTriangle& MeshTriangle : MeshTriangles)
		{
			int32 UVIndex = 0;

			FVector P[3];
			FVector2D UVs[3];

			for (int32 i = 0; i < 3; ++i)
			{
				const FVertexInstanceID VertexInstanceID = MeshTriangle.GetVertexInstanceID(i);
				UVs[i] = VertexUVs.Get(VertexInstanceID, 0); // UV0
				P[i] = VertexPositions[MeshDescription.GetVertexInstanceVertex(VertexInstanceID)];
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

				FMatrix ParameterToTexture(
					FPlane(UVs[1].X - UVs[0].X, UVs[1].Y - UVs[0].Y, 0, 0),
					FPlane(UVs[2].X - UVs[0].X, UVs[2].Y - UVs[0].Y, 0, 0),
					FPlane(UVs[0].X, UVs[0].Y, 1, 0),
					FPlane(0, 0, 0, 1)
				);

				// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
				const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

				FVector TmpTangentX(0.0f);
				FVector TmpTangentY(0.0f);
				FVector TmpTangentZ(0.0f);
				TmpTangentX = TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal();
				TmpTangentY = TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal();
				TmpTangentZ = Normal;
				FVector::CreateOrthonormalBasis(TmpTangentX, TmpTangentY, TmpTangentZ);
				TangentX += TmpTangentX;
				TangentY += TmpTangentY;
				TangentZ += TmpTangentZ;
			}
			else
			{
				//This will force a recompute of the normals and tangents
				TangentX = FVector(0.0f);
				TangentY = FVector(0.0f);
				TangentZ = FVector(0.0f);
				break;
			}
		}
		TangentX.Normalize();
		TangentY.Normalize();
		TangentZ.Normalize();
		PolygonTangents[PolygonID] = TangentX;
		PolygonBinormals[PolygonID] = TangentY;
		PolygonNormals[PolygonID] = TangentZ;

		// Calculate polygon center: just an average of all vertex positions.
		FVector Center = FVector::ZeroVector;
		const TArray<FVertexInstanceID>& VertexInstanceIDs = MeshDescription.GetPolygonPerimeterVertexInstances(PolygonID);
		for (const FVertexInstanceID VertexInstanceID : VertexInstanceIDs)
		{
			Center += VertexPositions[MeshDescription.GetVertexInstanceVertex(VertexInstanceID)];
		}
		Center /= float(VertexInstanceIDs.Num());
		PolygonCenters[PolygonID] = Center;
	}
}

struct FNTBGroupKeyFuncs : public TDefaultMapKeyFuncs<FVector2D, FVector, false>
{
	//We need to sanitize the key here to make sure -0.0f fall on the same hash then 0.0f
	static FORCEINLINE_DEBUGGABLE uint32 GetKeyHash(KeyInitType Key)
	{
		FVector2D TmpKey;
		TmpKey.X = FMath::IsNearlyZero(Key.X) ? 0.0f : Key.X;
		TmpKey.Y = FMath::IsNearlyZero(Key.Y) ? 0.0f : Key.Y;
		return FCrc::MemCrc32(&TmpKey, sizeof(TmpKey));
	}
};

void FMeshDescriptionOperations::CreateNormals(FMeshDescription& MeshDescription, FMeshDescriptionOperations::ETangentOptions TangentOptions, bool bComputeTangent)
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

	// @todo: provide an option to weight each contributing polygon normal according to the size of
	// the angle it makes with the vertex being calculated. This means that triangulated faces whose
	// internal edge meets the vertex doesn't get undue extra weight.

	TVertexInstanceAttributesConstRef<FVector2D> VertexUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	TVertexInstanceAttributesRef<FVector> VertexNormals = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexTangents = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexBinormalSigns = MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);

	TPolygonAttributesConstRef<FVector> PolygonNormals = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Normal);
	TPolygonAttributesConstRef<FVector> PolygonTangents = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Tangent);
	TPolygonAttributesConstRef<FVector> PolygonBinormals = MeshDescription.PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Binormal);
	check(PolygonNormals.IsValid());
	check(PolygonTangents.IsValid());
	check(PolygonBinormals.IsValid());

	TMap<FPolygonID, FVertexInfo> VertexInfoMap;
	VertexInfoMap.Reserve(20);
	//Iterate all vertex to compute normals for all vertex instance
	for (const FVertexID VertexID : MeshDescription.Vertices().GetElementIDs())
	{
		VertexInfoMap.Reset();

		bool bPointHasAllTangents = true;
		//Fill the VertexInfoMap
		for (const FEdgeID EdgeID : MeshDescription.GetVertexConnectedEdges(VertexID))
		{
			for (const FPolygonID PolygonID : MeshDescription.GetEdgeConnectedPolygons(EdgeID))
			{
				FVertexInfo& VertexInfo = VertexInfoMap.FindOrAdd(PolygonID);
				int32 EdgeIndex = VertexInfo.EdgeIDs.AddUnique(EdgeID);
				if (VertexInfo.PolygonID == FPolygonID::Invalid)
				{
					VertexInfo.PolygonID = PolygonID;
					for (const FVertexInstanceID VertexInstanceID : MeshDescription.GetPolygonPerimeterVertexInstances(PolygonID))
					{
						if (MeshDescription.GetVertexInstanceVertex(VertexInstanceID) == VertexID)
						{
							VertexInfo.VertexInstanceID = VertexInstanceID;
							VertexInfo.UVs = VertexUVs.Get(VertexInstanceID, 0);	// UV0
							bPointHasAllTangents &= !VertexNormals[VertexInstanceID].IsNearlyZero() && !VertexTangents[VertexInstanceID].IsNearlyZero();
							if (bPointHasAllTangents)
							{
								FVector TangentX = VertexTangents[VertexInstanceID].GetSafeNormal();
								FVector TangentZ = VertexNormals[VertexInstanceID].GetSafeNormal();
								FVector TangentY = (FVector::CrossProduct(TangentZ, TangentX).GetSafeNormal() * VertexBinormalSigns[VertexInstanceID]).GetSafeNormal();
								if (TangentX.ContainsNaN() || TangentX.IsNearlyZero(SMALL_NUMBER) ||
									TangentY.ContainsNaN() || TangentY.IsNearlyZero(SMALL_NUMBER) ||
									TangentZ.ContainsNaN() || TangentZ.IsNearlyZero(SMALL_NUMBER))
								{
									bPointHasAllTangents = false;
								}
							}
							break;
						}
					}
				}
			}
		}

		if (bPointHasAllTangents)
		{
			continue;
		}

		//Build all group by recursively traverse all polygon connected to the vertex
		TArray<TArray<FPolygonID>> Groups;
		TArray<FPolygonID> ConsumedPolygon;
		for (auto Kvp : VertexInfoMap)
		{
			if (ConsumedPolygon.Contains(Kvp.Key))
			{
				continue;
			}

			int32 CurrentGroupIndex = Groups.AddZeroed();
			TArray<FPolygonID>& CurrentGroup = Groups[CurrentGroupIndex];
			TArray<FPolygonID> PolygonQueue;
			PolygonQueue.Add(Kvp.Key); //Use a queue to avoid recursive function
			while (PolygonQueue.Num() > 0)
			{
				FPolygonID CurrentPolygonID = PolygonQueue.Pop(true);
				FVertexInfo& CurrentVertexInfo = VertexInfoMap.FindOrAdd(CurrentPolygonID);
				CurrentGroup.AddUnique(CurrentVertexInfo.PolygonID);
				ConsumedPolygon.AddUnique(CurrentVertexInfo.PolygonID);
				const TEdgeAttributesRef<bool> EdgeHardnesses = MeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
				for (const FEdgeID EdgeID : CurrentVertexInfo.EdgeIDs)
				{
					if (EdgeHardnesses[EdgeID])
					{
						//End of the group
						continue;
					}
					for (const FPolygonID PolygonID : MeshDescription.GetEdgeConnectedPolygons(EdgeID))
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
							PolygonQueue.Add(PolygonID);
						}
					}
				}
			}
		}

		//Smooth every connected group
		ConsumedPolygon.Reset();
		for (const TArray<FPolygonID>& Group : Groups)
		{
			//Compute tangents data
			TMap<FVector2D, FVector,FDefaultSetAllocator, FNTBGroupKeyFuncs> GroupTangent;
			TMap<FVector2D, FVector, FDefaultSetAllocator, FNTBGroupKeyFuncs> GroupBiNormal;

			TArray<FVertexInstanceID> VertexInstanceInGroup;
			FVector GroupNormal(0.0f);
			for (const FPolygonID PolygonID : Group)
			{
				FVector PolyNormal = PolygonNormals[PolygonID];
				FVector PolyTangent = PolygonTangents[PolygonID];
				FVector PolyBinormal = PolygonBinormals[PolygonID];

				ConsumedPolygon.Add(PolygonID);
				VertexInstanceInGroup.Add(VertexInfoMap[PolygonID].VertexInstanceID);
				if (!PolyNormal.IsNearlyZero(SMALL_NUMBER) && !PolyNormal.ContainsNaN())
				{
					GroupNormal += PolyNormal;
				}
				if (bComputeTangent)
				{
					const FVector2D UVs = VertexInfoMap[PolygonID].UVs;
					bool CreateGroup = (!GroupTangent.Contains(UVs));
					FVector& GroupTangentValue = GroupTangent.FindOrAdd(UVs);
					FVector& GroupBiNormalValue = GroupBiNormal.FindOrAdd(UVs);
					if (CreateGroup)
					{
						GroupTangentValue = FVector(0.0f);
						GroupBiNormalValue = FVector(0.0f);
					}
					if (!PolyTangent.IsNearlyZero(SMALL_NUMBER) && !PolyTangent.ContainsNaN())
					{
						GroupTangentValue += PolyTangent;
					}
					if (!PolyBinormal.IsNearlyZero(SMALL_NUMBER) && !PolyBinormal.ContainsNaN())
					{
						GroupBiNormalValue += PolyBinormal;
					}
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
			for (const FVertexInstanceID VertexInstanceID : VertexInstanceInGroup)
			{
				const FVector2D VertexUV = VertexUVs.Get(VertexInstanceID, 0);	// UV0

				if (VertexNormals[VertexInstanceID].IsNearlyZero(SMALL_NUMBER))
				{
					VertexNormals[VertexInstanceID] = GroupNormal;
				}
				if (bComputeTangent)
				{
					//Avoid changing the original group value
					FVector GroupTangentValue = GroupTangent[VertexUV];
					FVector GroupBiNormalValue = GroupBiNormal[VertexUV];

					if (!VertexTangents[VertexInstanceID].IsNearlyZero(SMALL_NUMBER))
					{
						GroupTangentValue = VertexTangents[VertexInstanceID];
					}
					FVector BiNormal(0.0f);
					if (!VertexNormals[VertexInstanceID].IsNearlyZero(SMALL_NUMBER) && !VertexTangents[VertexInstanceID].IsNearlyZero(SMALL_NUMBER))
					{
						BiNormal = FVector::CrossProduct(VertexNormals[VertexInstanceID], VertexTangents[VertexInstanceID]).GetSafeNormal() * VertexBinormalSigns[VertexInstanceID];
					}
					if (!BiNormal.IsNearlyZero(SMALL_NUMBER))
					{
						GroupBiNormalValue = BiNormal;
					}
					// Gram-Schmidt orthogonalization
					GroupBiNormalValue -= GroupTangentValue * (GroupTangentValue | GroupBiNormalValue);
					GroupBiNormalValue.Normalize();

					GroupTangentValue -= VertexNormals[VertexInstanceID] * (VertexNormals[VertexInstanceID] | GroupTangentValue);
					GroupTangentValue.Normalize();

					GroupBiNormalValue -= VertexNormals[VertexInstanceID] * (VertexNormals[VertexInstanceID] | GroupBiNormalValue);
					GroupBiNormalValue.Normalize();
					//Set the value
					VertexTangents[VertexInstanceID] = GroupTangentValue;
					//If the BiNormal is zero set the sign to 1.0f
					VertexBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(GroupTangentValue, GroupBiNormalValue, VertexNormals[VertexInstanceID]);

				}
			}
		}
	}
}

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

void FMeshDescriptionOperations::CreateMikktTangents(FMeshDescription& MeshDescription, FMeshDescriptionOperations::ETangentOptions TangentOptions)
{
	// The Mikkt interface does not handle properly polygon array with 'holes'
	// Compact mesh description if this is the case
	if (MeshDescription.Polygons().Num() != MeshDescription.Polygons().GetArraySize())
	{
		FElementIDRemappings Remappings;
		MeshDescription.Compact(Remappings);
	}

	bool bIgnoreDegenerateTriangles = (TangentOptions & FMeshDescriptionOperations::ETangentOptions::IgnoreDegenerateTriangles) != 0;

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
	MikkTContext.m_pUserData = (void*)(&MeshDescription);
	MikkTContext.m_bIgnoreDegenerates = bIgnoreDegenerateTriangles;
	genTangSpaceDefault(&MikkTContext);
}

namespace MeshDescriptionMikktSpaceInterface
{
	int MikkGetNumFaces(const SMikkTSpaceContext* Context)
	{
		FMeshDescription *MeshDescription = (FMeshDescription*)(Context->m_pUserData);
		return MeshDescription->Polygons().GetArraySize();
	}

	int MikkGetNumVertsOfFace(const SMikkTSpaceContext* Context, const int FaceIdx)
	{
		// All of our meshes are triangles.
		FMeshDescription *MeshDescription = (FMeshDescription*)(Context->m_pUserData);
		if (MeshDescription->IsPolygonValid(FPolygonID(FaceIdx)))
		{
			const FMeshPolygon& Polygon = MeshDescription->GetPolygon(FPolygonID(FaceIdx));
			return Polygon.PerimeterContour.VertexInstanceIDs.Num();
		}

		return 0;
	}

	void MikkGetPosition(const SMikkTSpaceContext* Context, float Position[3], const int FaceIdx, const int VertIdx)
	{
		FMeshDescription* MeshDescription = (FMeshDescription*)(Context->m_pUserData);
		const FMeshPolygon& Polygon = MeshDescription->GetPolygon(FPolygonID(FaceIdx));
		const FVertexInstanceID VertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[VertIdx];
		const FVertexID VertexID = MeshDescription->GetVertexInstanceVertex(VertexInstanceID);
		const FVector& VertexPosition = MeshDescription->VertexAttributes().GetAttribute<FVector>(VertexID, MeshAttribute::Vertex::Position);
		Position[0] = VertexPosition.X;
		Position[1] = VertexPosition.Y;
		Position[2] = VertexPosition.Z;
	}

	void MikkGetNormal(const SMikkTSpaceContext* Context, float Normal[3], const int FaceIdx, const int VertIdx)
	{
		FMeshDescription* MeshDescription = (FMeshDescription*)(Context->m_pUserData);
		const FMeshPolygon& Polygon = MeshDescription->GetPolygon(FPolygonID(FaceIdx));
		const FVertexInstanceID VertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[VertIdx];
		const FVector& VertexNormal = MeshDescription->VertexInstanceAttributes().GetAttribute<FVector>(VertexInstanceID, MeshAttribute::VertexInstance::Normal);
		Normal[0] = VertexNormal.X;
		Normal[1] = VertexNormal.Y;
		Normal[2] = VertexNormal.Z;
	}

	void MikkSetTSpaceBasic(const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int FaceIdx, const int VertIdx)
	{
		FMeshDescription* MeshDescription = (FMeshDescription*)(Context->m_pUserData);
		const FMeshPolygon& Polygon = MeshDescription->GetPolygon(FPolygonID(FaceIdx));
		const FVertexInstanceID VertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[VertIdx];
		const FVector VertexTangent(Tangent[0], Tangent[1], Tangent[2]);
		MeshDescription->VertexInstanceAttributes().SetAttribute<FVector>(VertexInstanceID, MeshAttribute::VertexInstance::Tangent, 0, VertexTangent);
		MeshDescription->VertexInstanceAttributes().SetAttribute<float>(VertexInstanceID, MeshAttribute::VertexInstance::BinormalSign, 0, -BitangentSign);
	}

	void MikkGetTexCoord(const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx)
	{
		FMeshDescription* MeshDescription = (FMeshDescription*)(Context->m_pUserData);
		const FMeshPolygon& Polygon = MeshDescription->GetPolygon(FPolygonID(FaceIdx));
		const FVertexInstanceID VertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[VertIdx];
		const FVector2D& TexCoord = MeshDescription->VertexInstanceAttributes().GetAttribute<FVector2D>(VertexInstanceID, MeshAttribute::VertexInstance::TextureCoordinate, 0);
		UV[0] = TexCoord.X;
		UV[1] = TexCoord.Y;
	}
}

void FMeshDescriptionOperations::FindOverlappingCorners(FOverlappingCorners& OutOverlappingCorners, const FMeshDescription& MeshDescription, float ComparisonThreshold)
{
	// @todo: this should be shared with FOverlappingCorners

	const FVertexInstanceArray& VertexInstanceArray = MeshDescription.VertexInstances();
	const FVertexArray& VertexArray = MeshDescription.Vertices();

	int32 NumWedges = 0;
	for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
	{
		NumWedges += 3 * MeshDescription.GetPolygonTriangles(PolygonID).Num();
	}

	// Empty the old data and reserve space for new
	OutOverlappingCorners.Init(NumWedges);

	// Create a list of vertex Z/index pairs
	TArray<MeshDescriptionOperationNamespace::FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Reserve(NumWedges);

	TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	int32 WedgeIndex = 0;
	for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
	{
		const TArray<FMeshTriangle>& Triangles = MeshDescription.GetPolygonTriangles(PolygonID);
		for (const FMeshTriangle& MeshTriangle : Triangles)
		{
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVertexInstanceID VertexInstanceID = MeshTriangle.GetVertexInstanceID(Corner);
				new(VertIndexAndZ)MeshDescriptionOperationNamespace::FIndexAndZ(WedgeIndex, VertexPositions[MeshDescription.GetVertexInstanceVertex(VertexInstanceID)]);
				++WedgeIndex;
			}
		}
	}

	// Sort the vertices by z value
	VertIndexAndZ.Sort(MeshDescriptionOperationNamespace::FCompareIndexAndZ());

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		// only need to search forward, since we add pairs both ways
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > ComparisonThreshold)
				break; // can't be any more dups

			const FVector& PositionA = *(VertIndexAndZ[i].OriginalVector);
			const FVector& PositionB = *(VertIndexAndZ[j].OriginalVector);

			if (PositionA.Equals(PositionB, ComparisonThreshold))
			{
				OutOverlappingCorners.Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
				OutOverlappingCorners.Add(VertIndexAndZ[j].Index, VertIndexAndZ[i].Index);
			}
		}
	}

	OutOverlappingCorners.FinishAdding();
}

struct FLayoutUVMeshDescriptionView final : FLayoutUV::IMeshView
{
	FMeshDescription& MeshDescription;
	TVertexAttributesConstRef<FVector> Positions;
	TVertexInstanceAttributesConstRef<FVector> Normals;
	TVertexInstanceAttributesRef<FVector2D> TexCoords;

	const uint32 SrcChannel;
	const uint32 DstChannel;

	uint32 NumIndices = 0;
	TArray<int32> RemapVerts;
	TArray<FVector2D> FlattenedTexCoords;

	FLayoutUVMeshDescriptionView(FMeshDescription& InMeshDescription, uint32 InSrcChannel, uint32 InDstChannel)
		: MeshDescription(InMeshDescription)
		, Positions(InMeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position))
		, Normals(InMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal))
		, TexCoords(InMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate))
		, SrcChannel(InSrcChannel)
		, DstChannel(InDstChannel)
	{
		uint32 NumTris = 0;
		for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
		{
			NumTris += MeshDescription.GetPolygonTriangles(PolygonID).Num();
		}

		NumIndices = NumTris * 3;

		FlattenedTexCoords.SetNumUninitialized(NumIndices);
		RemapVerts.SetNumUninitialized(NumIndices);

		int32 WedgeIndex = 0;

		for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
		{
			const TArray<FMeshTriangle>& Triangles = MeshDescription.GetPolygonTriangles(PolygonID);
			for (const FMeshTriangle MeshTriangle : Triangles)
			{
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const FVertexInstanceID VertexInstanceID = MeshTriangle.GetVertexInstanceID(Corner);

					FlattenedTexCoords[WedgeIndex] = TexCoords.Get(VertexInstanceID, SrcChannel);
					RemapVerts[WedgeIndex] = VertexInstanceID.GetValue();
					++WedgeIndex;
				}
			}
		}
	}

	uint32 GetNumIndices() const override { return NumIndices; }

	FVector GetPosition(uint32 Index) const override
	{
		FVertexInstanceID VertexInstanceID(RemapVerts[Index]);
		FVertexID VertexID = MeshDescription.GetVertexInstanceVertex(VertexInstanceID);
		return Positions[VertexID];
	}

	FVector GetNormal(uint32 Index) const override
	{
		FVertexInstanceID VertexInstanceID(RemapVerts[Index]);
		return Normals[VertexInstanceID];
	}

	FVector2D GetInputTexcoord(uint32 Index) const override
	{
		return FlattenedTexCoords[Index];
	}

	void InitOutputTexcoords(uint32 Num) override
	{
		// If current DstChannel is out of range of the number of UVs defined by the mesh description, change the index count accordingly
		const uint32 NumUVs = TexCoords.GetNumIndices();
		if (DstChannel >= NumUVs)
		{
			TexCoords.SetNumIndices(DstChannel + 1);
			ensure(false);	// not expecting it to get here
		}
	}

	void SetOutputTexcoord(uint32 Index, const FVector2D& Value) override
	{
		const FVertexInstanceID VertexInstanceID(RemapVerts[Index]);
		TexCoords.Set(VertexInstanceID, DstChannel, Value);
	}
};

int32 FMeshDescriptionOperations::GetUVChartCount(FMeshDescription& MeshDescription, int32 SrcLightmapIndex, ELightmapUVVersion LightmapUVVersion, const FOverlappingCorners& OverlappingCorners)
{
	uint32 UnusedDstIndex = -1;
	FLayoutUVMeshDescriptionView MeshDescriptionView(MeshDescription, SrcLightmapIndex, UnusedDstIndex);
	FLayoutUV Packer(MeshDescriptionView);
	Packer.SetVersion(LightmapUVVersion);
	return Packer.FindCharts(OverlappingCorners);
}

bool FMeshDescriptionOperations::CreateLightMapUVLayout(FMeshDescription& MeshDescription,
	int32 SrcLightmapIndex,
	int32 DstLightmapIndex,
	int32 MinLightmapResolution,
	ELightmapUVVersion LightmapUVVersion,
	const FOverlappingCorners& OverlappingCorners)
{
	FLayoutUVMeshDescriptionView MeshDescriptionView(MeshDescription, SrcLightmapIndex, DstLightmapIndex);
	FLayoutUV Packer(MeshDescriptionView);
	Packer.SetVersion(LightmapUVVersion);

	Packer.FindCharts(OverlappingCorners);
	bool bPackSuccess = Packer.FindBestPacking(MinLightmapResolution);
	if (bPackSuccess)
	{
		Packer.CommitPackedUVs();
	}
	return bPackSuccess;
}

bool FMeshDescriptionOperations::GenerateUniqueUVsForStaticMesh(const FMeshDescription& MeshDescription, int32 TextureResolution, bool bMergeIdenticalMaterials, TArray<FVector2D>& OutTexCoords)
{
	// Create a copy of original mesh (only copy necessary data)
	FMeshDescription DuplicateMeshDescription(MeshDescription);
	
	//Make sure we have a destination UV TextureCoordinnate
	{
		TVertexInstanceAttributesRef<FVector2D> DuplicateVertexInstanceUVs = DuplicateMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		if (DuplicateVertexInstanceUVs.GetNumIndices() < 2)
		{
			DuplicateVertexInstanceUVs.SetNumIndices(2);
		}
	}

	TMap<FVertexInstanceID, FVertexInstanceID> RemapVertexInstance;
	//Remove the identical material
	if (bMergeIdenticalMaterials)
	{
		TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = DuplicateMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		TArray<FPolygonID> ToDeletePolygons;
		RemapVertexInstance.Reserve(DuplicateMeshDescription.VertexInstances().Num());
		TArray<FPolygonID> UniquePolygons;
		for (FPolygonID RefPolygonID : DuplicateMeshDescription.Polygons().GetElementIDs())
		{
			FPolygonGroupID RefPolygonGroupID = DuplicateMeshDescription.GetPolygonPolygonGroup(RefPolygonID);
			const TArray<FVertexInstanceID>& RefVertexInstances = DuplicateMeshDescription.GetPolygonPerimeterVertexInstances(RefPolygonID);
			TArray<FVector2D> RefUVs;
			for (FVertexInstanceID RefVertexInstanceID : RefVertexInstances)
			{
				RefUVs.Add(VertexInstanceUVs[RefVertexInstanceID]);
			}
			FPolygonID MatchPolygonID = FPolygonID::Invalid;
			for (FPolygonID TestPolygonID : UniquePolygons)
			{
				FPolygonGroupID TestPolygonGroupID = DuplicateMeshDescription.GetPolygonPolygonGroup(TestPolygonID);
				if (TestPolygonGroupID != RefPolygonGroupID)
				{
					continue;
				}
				const TArray<FVertexInstanceID>& TestVertexInstances = DuplicateMeshDescription.GetPolygonPerimeterVertexInstances(TestPolygonID);
				if (TestVertexInstances.Num() != RefVertexInstances.Num())
				{
					continue;
				}
				bool bIdentical = true;
				int32 UVIndex = 0;
				for (FVertexInstanceID TestVertexInstanceID : TestVertexInstances)
				{
					if (VertexInstanceUVs[TestVertexInstanceID] != RefUVs[UVIndex])
					{
						bIdentical = false;
						break;
					}
					UVIndex++;
				}
				if (bIdentical)
				{
					MatchPolygonID = TestPolygonID;
					break;
				}
			}

			if (MatchPolygonID == FPolygonID::Invalid)
			{
				UniquePolygons.Add(RefPolygonID);
				for (FVertexInstanceID RefVertexInstanceID : RefVertexInstances)
				{
					RemapVertexInstance.Add(RefVertexInstanceID, RefVertexInstanceID);
				}
			}
			else
			{
				const TArray<FVertexInstanceID>& TestVertexInstances = DuplicateMeshDescription.GetPolygonPerimeterVertexInstances(MatchPolygonID);
				int32 VertexInstanceIndex = 0;
				for (FVertexInstanceID RefVertexInstanceID : RefVertexInstances)
				{
					RemapVertexInstance.Add(RefVertexInstanceID, TestVertexInstances[VertexInstanceIndex]);
					VertexInstanceIndex++;
				}
				ToDeletePolygons.Add(RefPolygonID);
			}
		}

		//Delete polygons
		if(ToDeletePolygons.Num() > 0)
		{
			TArray<FEdgeID> OrphanedEdges;
			TArray<FVertexInstanceID> OrphanedVertexInstances;
			TArray<FPolygonGroupID> OrphanedPolygonGroups;
			TArray<FVertexID> OrphanedVertices;
			for (FPolygonID PolygonID : ToDeletePolygons)
			{
				DuplicateMeshDescription.DeletePolygon(PolygonID, &OrphanedEdges, &OrphanedVertexInstances, &OrphanedPolygonGroups);
			}
			for (FPolygonGroupID PolygonGroupID : OrphanedPolygonGroups)
			{
				DuplicateMeshDescription.DeletePolygonGroup(PolygonGroupID);
			}
			for (FVertexInstanceID VertexInstanceID : OrphanedVertexInstances)
			{
				DuplicateMeshDescription.DeleteVertexInstance(VertexInstanceID, &OrphanedVertices);
			}
			for (FEdgeID EdgeID : OrphanedEdges)
			{
				DuplicateMeshDescription.DeleteEdge(EdgeID, &OrphanedVertices);
			}
			for (FVertexID VertexID : OrphanedVertices)
			{
				DuplicateMeshDescription.DeleteVertex(VertexID);
			}
			//Avoid compacting the DuplicateMeshDescription, since the remap of the VertexInstaceID will not be good anymore
		}
	}
	// Find overlapping corners for UV generator. Allow some threshold - this should not produce any error in a case if resulting
	// mesh will not merge these vertices.
	FOverlappingCorners OverlappingCorners;
	FindOverlappingCorners(OverlappingCorners, DuplicateMeshDescription, THRESH_POINTS_ARE_SAME);

	// Generate new UVs
	FLayoutUVMeshDescriptionView DuplicateMeshDescriptionView(DuplicateMeshDescription, 0, 1);
	FLayoutUV Packer(DuplicateMeshDescriptionView);
	Packer.FindCharts(OverlappingCorners);

	bool bPackSuccess = Packer.FindBestPacking(FMath::Clamp(TextureResolution / 4, 32, 512));
	if (bPackSuccess)
	{
		Packer.CommitPackedUVs();
		TVertexInstanceAttributesConstRef<FVector2D> DupVertexInstanceUVs = DuplicateMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		// Save generated UVs
		check(DupVertexInstanceUVs.GetNumIndices() > 1);
		OutTexCoords.AddZeroed(VertexInstanceUVs.GetNumElements());
		int32 TextureCoordIndex = 0;
		for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
		{
			FVertexInstanceID RemapID = bMergeIdenticalMaterials ? RemapVertexInstance[VertexInstanceID] : VertexInstanceID;
			// Save generated UVs
			OutTexCoords[TextureCoordIndex] = DupVertexInstanceUVs.Get(RemapID, 1);	// UV1
			TextureCoordIndex++;
		}
	}

	return bPackSuccess;
}

bool FMeshDescriptionOperations::AddUVChannel(FMeshDescription& MeshDescription)
{
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	if (VertexInstanceUVs.GetNumIndices() >= MAX_MESH_TEXTURE_COORDS)
	{
		UE_LOG(LogMeshDescriptionOperations, Error, TEXT("AddUVChannel: Cannot add UV channel. Maximum number of UV channels reached (%d)."), MAX_MESH_TEXTURE_COORDS);
		return false;
	}

	VertexInstanceUVs.SetNumIndices(VertexInstanceUVs.GetNumIndices() + 1);
	return true;
}

bool FMeshDescriptionOperations::InsertUVChannel(FMeshDescription& MeshDescription, int32 UVChannelIndex)
{
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	if (UVChannelIndex < 0 || UVChannelIndex > VertexInstanceUVs.GetNumIndices())
	{
		UE_LOG(LogMeshDescriptionOperations, Error, TEXT("InsertUVChannel: Cannot insert UV channel. Given UV channel index %d is out of bounds."), UVChannelIndex);
		return false;
	}

	if (VertexInstanceUVs.GetNumIndices() >= MAX_MESH_TEXTURE_COORDS)
	{
		UE_LOG(LogMeshDescriptionOperations, Error, TEXT("InsertUVChannel: Cannot insert UV channel. Maximum number of UV channels reached (%d)."), MAX_MESH_TEXTURE_COORDS);
		return false;
	}

	VertexInstanceUVs.InsertIndex(UVChannelIndex);
	return true;
}

bool FMeshDescriptionOperations::RemoveUVChannel(FMeshDescription& MeshDescription, int32 UVChannelIndex)
{
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	if (VertexInstanceUVs.GetNumIndices() == 1)
	{
		UE_LOG(LogMeshDescriptionOperations, Error, TEXT("RemoveUVChannel: Cannot remove UV channel. There must be at least one channel."));
		return false;
	}

	if (UVChannelIndex < 0 || UVChannelIndex >= VertexInstanceUVs.GetNumIndices())
	{
		UE_LOG(LogMeshDescriptionOperations, Error, TEXT("RemoveUVChannel: Cannot remove UV channel. Given UV channel index %d is out of bounds."), UVChannelIndex);
		return false;
	}

	VertexInstanceUVs.RemoveIndex(UVChannelIndex);
	return true;
}

void FMeshDescriptionOperations::GeneratePlanarUV(const FMeshDescription& MeshDescription, const FUVMapParameters& Params, TArray<FVector2D>& OutTexCoords)
{
	// Project along X-axis (left view), UV along Z Y axes
	FVector U = FVector::UpVector;
	FVector V = FVector::RightVector;

	TMeshAttributesConstRef<FVertexID, FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	OutTexCoords.AddZeroed(MeshDescription.VertexInstances().Num());

	FVector Size = Params.Size * Params.Scale;
	FVector Offset = Params.Position - Size / 2.f;

	int32 TextureCoordIndex = 0;
	for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
	{
		const FVertexID VertexID = MeshDescription.GetVertexInstanceVertex(VertexInstanceID);
		FVector Vertex = VertexPositions[VertexID];

		// Apply the gizmo transforms
		Vertex = Params.Rotation.RotateVector(Vertex);
		Vertex -= Offset;
		Vertex /= Size;

		float UCoord = FVector::DotProduct(Vertex, U) * Params.UVTile.X;
		float VCoord = FVector::DotProduct(Vertex, V) * Params.UVTile.Y;
		OutTexCoords[TextureCoordIndex++] = FVector2D(UCoord, VCoord);
	}
}

void FMeshDescriptionOperations::GenerateCylindricalUV(FMeshDescription& MeshDescription, const FUVMapParameters& Params, TArray<FVector2D>& OutTexCoords)
{
	FVector Size = Params.Size * Params.Scale;
	FVector Offset = Params.Position;

	// Cylinder along X-axis, counterclockwise from -Y axis as seen from left view
	FVector V = FVector::ForwardVector;
	Offset.X -= Size.X / 2.f;

	TMeshAttributesConstRef<FVertexID, FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	OutTexCoords.AddZeroed(MeshDescription.VertexInstances().Num());

	const float AngleOffset = PI; // offset to get the same result as in 3dsmax
	int32 TextureCoordIndex = 0;

	for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
	{
		const FVertexID VertexID = MeshDescription.GetVertexInstanceVertex(VertexInstanceID);
		FVector Vertex = VertexPositions[VertexID];

		// Apply the gizmo transforms
		Vertex = Params.Rotation.RotateVector(Vertex);
		Vertex -= Offset;
		Vertex /= Size;

		float Angle = FMath::Atan2(Vertex.Z, Vertex.Y);

		Angle += AngleOffset;
		Angle *= Params.UVTile.X;

		float UCoord = Angle / (2 * PI);
		float VCoord = FVector::DotProduct(Vertex, V) * Params.UVTile.Y;

		OutTexCoords[TextureCoordIndex++] = FVector2D(UCoord, VCoord);
	}

	// Fix the UV coordinates for triangles at the seam where the angle wraps around
	for (const FPolygonID& PolygonID : MeshDescription.Polygons().GetElementIDs())
	{
		const TArray<FVertexInstanceID>& VertexInstances = MeshDescription.GetPolygonPerimeterVertexInstances(PolygonID);
		int32 NumInstances = VertexInstances.Num();
		if (NumInstances >= 2)
		{
			for (int32 StartIndex = 1; StartIndex < NumInstances; ++StartIndex)
			{
				int32 EndIndex = StartIndex + 1;
				if (EndIndex >= NumInstances)
				{
					EndIndex = EndIndex % NumInstances;
				}

				const FVector2D& StartUV = OutTexCoords[VertexInstances[StartIndex].GetValue()];
				FVector2D& EndUV = OutTexCoords[VertexInstances[EndIndex].GetValue()];

				// TODO: Improve fix for UVTile other than 1
				float Threshold = 0.5f / Params.UVTile.X;
				if (FMath::Abs(EndUV.X - StartUV.X) > Threshold)
				{
					// Fix the U coordinate to get the texture go counterclockwise
					if (EndUV.X > Threshold)
					{
						EndUV.X -= 1.f;
					}
					else
					{
						EndUV.X += 1.f;
					}
				}
			}
		}
	}
}

void FMeshDescriptionOperations::GenerateBoxUV(const FMeshDescription& MeshDescription, const FUVMapParameters& Params, TArray<FVector2D>& OutTexCoords)
{
	FVector Size = Params.Size * Params.Scale;
	FVector HalfSize = Size / 2.0f;

	TMeshAttributesConstRef<FVertexID, FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	OutTexCoords.AddZeroed(MeshDescription.VertexInstances().Num());

	// Setup the UVs such that the mapping is from top-left to bottom-right when viewed orthographically
	TArray<TPair<FVector, FVector>> PlaneUVs;
	PlaneUVs.Add(TPair<FVector, FVector>(FVector::ForwardVector, FVector::RightVector));	// Top view
	PlaneUVs.Add(TPair<FVector, FVector>(FVector::BackwardVector, FVector::RightVector));	// Bottom view
	PlaneUVs.Add(TPair<FVector, FVector>(FVector::ForwardVector, FVector::DownVector));		// Right view
	PlaneUVs.Add(TPair<FVector, FVector>(FVector::BackwardVector, FVector::DownVector));	// Left view
	PlaneUVs.Add(TPair<FVector, FVector>(FVector::LeftVector, FVector::DownVector));		// Front view
	PlaneUVs.Add(TPair<FVector, FVector>(FVector::RightVector, FVector::DownVector));		// Back view

	TArray<FPlane> BoxPlanes;
	const FVector& Center = Params.Position;

	BoxPlanes.Add(FPlane(Center + FVector(0, 0, HalfSize.Z), FVector::UpVector));		// Top plane
	BoxPlanes.Add(FPlane(Center - FVector(0, 0, HalfSize.Z), FVector::DownVector));		// Bottom plane
	BoxPlanes.Add(FPlane(Center + FVector(0, HalfSize.Y, 0), FVector::RightVector));	// Right plane
	BoxPlanes.Add(FPlane(Center - FVector(0, HalfSize.Y, 0), FVector::LeftVector));		// Left plane
	BoxPlanes.Add(FPlane(Center + FVector(HalfSize.X, 0, 0), FVector::ForwardVector));	// Front plane
	BoxPlanes.Add(FPlane(Center - FVector(HalfSize.X, 0, 0), FVector::BackwardVector));	// Back plane

	// For each polygon, find the box plane that best matches the polygon normal
	for (const FPolygonID& PolygonID : MeshDescription.Polygons().GetElementIDs())
	{
		const TArray<FVertexInstanceID>& VertexInstances = MeshDescription.GetPolygonPerimeterVertexInstances(PolygonID);
		check(VertexInstances.Num() == 3);

		FVector Vertex0 = VertexPositions[MeshDescription.GetVertexInstanceVertex(VertexInstances[0])];
		FVector Vertex1 = VertexPositions[MeshDescription.GetVertexInstanceVertex(VertexInstances[1])];
		FVector Vertex2 = VertexPositions[MeshDescription.GetVertexInstanceVertex(VertexInstances[2])];

		FPlane PolygonPlane(Vertex0, Vertex2, Vertex1);

		// Find the box plane that is most aligned with the polygon plane
		// TODO: Also take the distance between the planes into consideration
		float MaxProj = 0.f;
		int32 BestPlaneIndex = 0;
		for (int32 Index = 0; Index < BoxPlanes.Num(); ++Index)
		{
			float Proj = FVector::DotProduct(BoxPlanes[Index], PolygonPlane);
			if (Proj > MaxProj)
			{
				MaxProj = Proj;
				BestPlaneIndex = Index;
			}
		}

		FVector U = PlaneUVs[BestPlaneIndex].Key;
		FVector V = PlaneUVs[BestPlaneIndex].Value;
		FVector Offset = Params.Position - HalfSize * (U + V);

		for (const FVertexInstanceID& VertexInstanceID : VertexInstances)
		{
			const FVertexID VertexID = MeshDescription.GetVertexInstanceVertex(VertexInstanceID);
			FVector Vertex = VertexPositions[VertexID];

			// Apply the gizmo transforms
			Vertex = Params.Rotation.RotateVector(Vertex);
			Vertex -= Offset;
			Vertex /= Size;

			float UCoord = FVector::DotProduct(Vertex, U) * Params.UVTile.X;
			float VCoord = FVector::DotProduct(Vertex, V) * Params.UVTile.Y;

			OutTexCoords[VertexInstanceID.GetValue()] = FVector2D(UCoord, VCoord);
		}
	}
}

void FMeshDescriptionOperations::RemapPolygonGroups(FMeshDescription& MeshDescription, TMap<FPolygonGroupID, FPolygonGroupID>& Remap)
{
	TPolygonGroupAttributesRef<FName> PolygonGroupNames = MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	struct FOldPolygonGroupData
	{
		FName Name;
		TArray<FPolygonID> Polygons;
	};

	TMap<FPolygonGroupID, FOldPolygonGroupData> OldData;
	for (const FPolygonGroupID& PolygonGroupID : MeshDescription.PolygonGroups().GetElementIDs())
	{
		if (!Remap.Contains(PolygonGroupID) || PolygonGroupID == Remap[PolygonGroupID])
		{
			//No need to change this one
			continue;
		}
		FOldPolygonGroupData& PolygonGroupData = OldData.FindOrAdd(PolygonGroupID);
		PolygonGroupData.Name = PolygonGroupNames[PolygonGroupID];
		FMeshPolygonGroup& PolygonGroup = MeshDescription.GetPolygonGroup(PolygonGroupID);
		PolygonGroupData.Polygons = PolygonGroup.Polygons;
		PolygonGroup.Polygons.Empty();
		MeshDescription.DeletePolygonGroup(PolygonGroupID);
	}
	for (auto Kvp : OldData)
	{
		FPolygonGroupID GroupID = Kvp.Key;
		FPolygonGroupID ToGroupID = Remap[GroupID];
		if (!MeshDescription.PolygonGroups().IsValid(ToGroupID))
		{
			MeshDescription.CreatePolygonGroupWithID(ToGroupID);
		}
		TArray<FPolygonID>& Polygons = MeshDescription.GetPolygonGroup(ToGroupID).Polygons;
		Polygons.Append(Kvp.Value.Polygons);
		PolygonGroupNames[ToGroupID] = Kvp.Value.Name;
		for (FPolygonID PolygonID : Polygons)
		{
			MeshDescription.GetPolygon(PolygonID).PolygonGroupID = ToGroupID;
		}
	}
}

void FMeshDescriptionOperations::SwapPolygonPolygonGroup(FMeshDescription& MeshDescription, int32 SectionIndex, int32 TriangleIndexStart, int32 TriangleIndexEnd, bool bRemoveEmptyPolygonGroup)
{
	int32 TriangleIndex = 0;
	TPolygonGroupAttributesRef<FName> PolygonGroupNames = MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	FPolygonGroupID TargetPolygonGroupID(SectionIndex);
	if (!bRemoveEmptyPolygonGroup)
	{
		while (!MeshDescription.PolygonGroups().IsValid(TargetPolygonGroupID))
		{
			TargetPolygonGroupID = MeshDescription.CreatePolygonGroup();
			PolygonGroupNames[TargetPolygonGroupID] = FName(*(TEXT("SwapPolygonMaterialSlotName_") + FString::FromInt(TargetPolygonGroupID.GetValue())));
			TargetPolygonGroupID = FPolygonGroupID(SectionIndex);
		}
	}
	else
	{
		//This will not follow the SectionIndex value if the value is greater then the number of section (do not use this when merging meshes)
		if (!MeshDescription.PolygonGroups().IsValid(TargetPolygonGroupID))
		{
			TargetPolygonGroupID = MeshDescription.CreatePolygonGroup();
			PolygonGroupNames[TargetPolygonGroupID] = FName(*(TEXT("SwapPolygonMaterialSlotName_") + FString::FromInt(TargetPolygonGroupID.GetValue())));
		}
	}

	for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
	{
		int32 TriangleCount = MeshDescription.GetPolygonTriangles(PolygonID).Num();
		if (TriangleIndex >= TriangleIndexStart && TriangleIndex < TriangleIndexEnd)
		{
			check(TriangleIndex + (TriangleCount-1) < TriangleIndexEnd);
			FMeshPolygon& Polygon = MeshDescription.GetPolygon(PolygonID);
			FPolygonGroupID OldpolygonGroupID = Polygon.PolygonGroupID;
			if (OldpolygonGroupID != TargetPolygonGroupID)
			{
				MeshDescription.SetPolygonPolygonGroup(PolygonID, TargetPolygonGroupID);
				if (bRemoveEmptyPolygonGroup && MeshDescription.GetPolygonGroupPolygons(OldpolygonGroupID).Num() < 1)
				{
					MeshDescription.DeletePolygonGroup(OldpolygonGroupID);
				}
			}
		}
		TriangleIndex += TriangleCount;
	}
}

bool FMeshDescriptionOperations::HasVertexColor(const FMeshDescription& MeshDescription)
{
	TVertexInstanceAttributesConstRef<FVector4> VertexInstanceColors = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	bool bHasVertexColor = false;
	FVector4 WhiteColor(FLinearColor::White);
	for (const FVertexInstanceID& VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
	{
		if (VertexInstanceColors[VertexInstanceID] != WhiteColor)
		{
			bHasVertexColor = true;
			break;
		}
	}
	return bHasVertexColor;
}

void FMeshDescriptionOperations::BuildWeldedVertexIDRemap(const FMeshDescription& MeshDescription, const float WeldingThreshold, TMap<FVertexID, FVertexID>& OutVertexIDRemap)
{
	TVertexAttributesConstRef<FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

	int32 NumVertex = MeshDescription.Vertices().Num();
	OutVertexIDRemap.Reserve(NumVertex);

	// Create a list of vertex Z/index pairs
	TArray<MeshDescriptionOperationNamespace::FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Reserve(NumVertex);

	for (const FVertexID VertexID : MeshDescription.Vertices().GetElementIDs())
	{
		new(VertIndexAndZ)MeshDescriptionOperationNamespace::FIndexAndZ(VertexID.GetValue(), VertexPositions[VertexID]);
	}

	// Sort the vertices by z value
	VertIndexAndZ.Sort(MeshDescriptionOperationNamespace::FCompareIndexAndZ());

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		FVertexID Index_i = FVertexID(VertIndexAndZ[i].Index);
		if (OutVertexIDRemap.Contains(Index_i))
		{
			continue;
		}
		OutVertexIDRemap.FindOrAdd(Index_i) = Index_i;
		// only need to search forward, since we add pairs both ways
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > WeldingThreshold)
				break; // can't be any more dups

			const FVector& PositionA = *(VertIndexAndZ[i].OriginalVector);
			const FVector& PositionB = *(VertIndexAndZ[j].OriginalVector);

			if (PositionA.Equals(PositionB, WeldingThreshold))
			{
				OutVertexIDRemap.FindOrAdd(FVertexID(VertIndexAndZ[j].Index)) = Index_i;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
