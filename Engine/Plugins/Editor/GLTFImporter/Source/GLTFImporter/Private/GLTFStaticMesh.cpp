// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GLTFStaticMesh.h"
#include "GLTFPackage.h"

#include "Engine/StaticMesh.h"
//#include "RawMesh.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"
#include "Materials/Material.h"
#include "AssetRegistryModule.h"

using namespace GLTF;

template <typename T>
TArray<T> ReIndex(const TArray<T>& Source, const TArray<uint32>& Indices)
{
	TArray<T> Result;
	Result.Reserve(Indices.Num());
	for (uint32 Index : Indices)
	{
		Result.Add(Source[Index]);
	}
	return Result;
}

static FVector ConvertVec3(const FVector& In)
{
	// glTF uses a right-handed coordinate system, with Y up.
	// UE4 uses a left-handed coordinate system, with Z up.
	return { In.X, In.Z, In.Y };
}

static FVector ConvertPosition(const FVector& In)
{
	constexpr float Scale = 100.0f; // glTF (m) to UE4 (cm)
	return Scale * ConvertVec3(In);
}

// Convert tangent from Vec4 (glTF) to Vec3 (Unreal)
static FVector Tanslate(const FVector4& In)
{
	// Ignore In.W for now. (TODO: the right thing)
	// Its sign indicates handedness of the tangent basis.
	return ConvertVec3({ In.X, In.Y, In.Z });
}

// Convert tangents from Vec4 (glTF) to Vec3 (Unreal)
static TArray<FVector> Tanslate(const TArray<FVector4>& In)
{
	TArray<FVector> Result;
	Result.Reserve(In.Num());

	for (const FVector& Vec : In)
	{
		Result.Add(Tanslate(Vec));
	}

	return Result;
}


static TArray<FVector> GenerateFlatNormals(const TArray<FVector>& Positions, const TArray<uint32>& Indices)
{
	TArray<FVector> Normals;
	const uint32 N = Indices.Num();
	check(N % 3 == 0);
	Normals.AddUninitialized(N);

	for (uint32 i = 0; i < N; i += 3)
	{
		const FVector& A = Positions[Indices[i]];
		const FVector& B = Positions[Indices[i + 1]];
		const FVector& C = Positions[Indices[i + 2]];

		const FVector Normal = FVector::CrossProduct(A - B, A - C).GetSafeNormal();

		// Same for each corner of the triangle.
		Normals[i] = Normal;
		Normals[i + 1] = Normal;
		Normals[i + 2] = Normal;
	}

	return Normals;
}

// Add N copies of Value to end of Array
template <typename T>
static void AddN(TArray<T>& Array, T Value, uint32 N)
{
	Array.Reserve(Array.Num() + N);
	while (N--)
	{
		Array.Add(Value);
	}
}

static void AssignMaterials(UStaticMesh* StaticMesh, TMap<int32, int32>& OutMaterialIndexToSlot, const TArray<UMaterial*>& Materials, const TSet<int32>& MaterialIndices)
{
	// Create material slots for this mesh, only for the materials it uses.

	// Sort material indices so slots will be in same order as glTF file. Likely the same order as content creation app!
	// (first entry will be INDEX_NONE if present)
	TArray<int32> SortedMaterialIndices = MaterialIndices.Array();
	SortedMaterialIndices.Sort();

	const int32 N = MaterialIndices.Num();
	OutMaterialIndexToSlot.Empty(N);
	StaticMesh->StaticMaterials.Reserve(N);

	for (int32 MaterialIndex : SortedMaterialIndices)
	{
		UMaterial* Mat;
		int32 MeshSlot;

		if (MaterialIndex == INDEX_NONE)
		{
			// Add a slot for the default material.
			static UMaterial* DefaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
			Mat = DefaultMat;
			MeshSlot = StaticMesh->StaticMaterials.Add(DefaultMat);
		}
		else
		{
			// Add a slot for a real material.
			Mat = Materials[MaterialIndex];
			FName MatName(*(Mat->GetName()));
			MeshSlot = StaticMesh->StaticMaterials.Emplace(Mat, MatName, MatName);
		}

		OutMaterialIndexToSlot.Add(MaterialIndex, MeshSlot);

		StaticMesh->SectionInfoMap.Set(0, MeshSlot, FMeshSectionInfo(MeshSlot));
	}
}

UStaticMesh* ImportStaticMesh(const FAsset& Asset, const TArray<UMaterial*>& Materials, UObject* InParent, FName InName, EObjectFlags Flags, uint32 Index)
{
	// We should warn if certain things are "fixed up" during import.
	bool bDidGenerateTexCoords = false;
	bool bDidGenerateTangents = false;
	bool bMeshUsesEmptyMaterial = false;

	const FMesh& Mesh = Asset.Meshes[Index];

	if (Mesh.HasJointWeights())
	{
		UE_LOG(LogTemp, Warning, TEXT("Mesh has joint weights; import as Skeletal Mesh?"));
	}

	FString AssetName;
	UPackage* AssetPackage = GetAssetPackageAndName<UStaticMesh>(InParent, Mesh.Name, TEXT("SM"), InName, Index, AssetName);

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(AssetPackage, FName(*AssetName), Flags);

	FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
	//GLTF do not support LOD yet so assuming LODIndex of 0
	int32 LODIndex = 0;
	FMeshDescription* MeshDescription = StaticMesh->CreateOriginalMeshDescription(LODIndex);
	StaticMesh->RegisterMeshAttributes(*MeshDescription);

	TVertexAttributesRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TEdgeAttributesRef<bool> EdgeHardnesses = MeshDescription->EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = MeshDescription->EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	//TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	int32 NumUVs = 0;
	for (int32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS_MD; ++UVIndex)
	{
		if (Mesh.HasTexCoords(UVIndex))
		{
			NumUVs++;
		}
		else
		{
			break;
		}
	}

	VertexInstanceUVs.SetNumIndices(NumUVs);


	FMeshBuildSettings& Settings = SourceModel.BuildSettings;

	Settings.bRecomputeNormals = false;
	Settings.bRecomputeTangents = !Mesh.HasTangents();
	Settings.bUseMikkTSpace = true;

	Settings.bRemoveDegenerates = false;
	Settings.bBuildAdjacencyBuffer = false;
	Settings.bBuildReversedIndexBuffer = false;

	Settings.bUseHighPrecisionTangentBasis = false;
	Settings.bUseFullPrecisionUVs = false;

	Settings.bGenerateLightmapUVs = false; // set to true if asset has no UV1 ?

	TSet<int32> MaterialIndicesUsed;
	//Add the vertex
	TArray<TMap<int32, FVertexID>> PositionIndexToVertexID_PerPrim;
	PositionIndexToVertexID_PerPrim.AddDefaulted(Mesh.Primitives.Num());
	for (int32 PrimIndex = 0; PrimIndex < Mesh.Primitives.Num(); ++PrimIndex)
	{
		const FPrimitive& Prim = Mesh.Primitives[PrimIndex];
		// Remember which primitives use which materials.
		MaterialIndicesUsed.Add(Prim.MaterialIndex);

		const TArray<FVector>& Positions = Prim.GetPositions();
		PositionIndexToVertexID_PerPrim[PrimIndex].Reserve(Positions.Num());
		for (int32 PositionIndex = 0; PositionIndex < Positions.Num(); ++PositionIndex)
		{
			const FVertexID& VertexID = MeshDescription->CreateVertex();
			VertexPositions[VertexID] = ConvertPosition(Positions[PositionIndex]);
			PositionIndexToVertexID_PerPrim[PrimIndex].Add(PositionIndex, VertexID);
		}
	}
	TMap<int32, int32> MaterialIndexToSlot;
	AssignMaterials(StaticMesh, MaterialIndexToSlot, Materials, MaterialIndicesUsed);

	TMap<int32, FPolygonGroupID> MaterialIndexToPolygonGroupID;
	//Add the PolygonGroup
	for (int32 MaterialIndex : MaterialIndicesUsed)
	{
		const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
		MaterialIndexToPolygonGroupID.Add(MaterialIndex, PolygonGroupID);
		PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = StaticMesh->StaticMaterials[MaterialIndexToSlot[MaterialIndex]].ImportedMaterialSlotName;
	}
	//Add the vertexInstance
	for (int32 PrimIndex = 0; PrimIndex < Mesh.Primitives.Num(); ++PrimIndex)
	{
		const FPrimitive& Prim = Mesh.Primitives[PrimIndex];
		FPolygonGroupID CurrentPolygonGroupID = MaterialIndexToPolygonGroupID[Prim.MaterialIndex];
		const uint32 TriCount = Prim.TriangleCount();
		const TArray<uint32> Indices = Prim.GetTriangleIndices();
		const TArray<FVector>& Positions = Prim.GetPositions();
		TArray<FVector> Normals;
		// glTF does not guarantee each primitive within a mesh has the same attributes.
		// Fill in gaps as needed:
		// - missing normals will be flat, based on triangle orientation
		// - missing UVs will be (0,0)
		// - missing tangents will be (0,0,1)
		if (Prim.HasNormals())
		{
			Normals = ReIndex(Prim.GetNormals(), Indices);
		}
		else
		{
			Normals = GenerateFlatNormals(Positions, Indices);
		}
		TArray<FVector> Tangents;
		if (Prim.HasTangents())
		{
			// glTF stores tangent as Vec4, with W component indicating handedness of tangent basis.
			Tangents = ReIndex(Tanslate(Prim.GetTangents()), Indices);
		}
		else if (Mesh.HasTangents())
		{
			// If other primitives in this mesh have tangents, generate filler ones for this primitive, to avoid gaps.
			AddN(Tangents, FVector(0.0f, 0.0f, 1.0f), Prim.VertexCount());
			bDidGenerateTangents = true;
		}
		TArray<FVector2D> UVs[MAX_MESH_TEXTURE_COORDS_MD];
		for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex)
		{
			if (Prim.HasTexCoords(UVIndex))
			{
				UVs[UVIndex] = ReIndex(Prim.GetTexCoords(UVIndex), Indices);
			}
			else
			{
				// Unreal StaticMesh must have UV channel 0.
				// glTF doesn't require this since not all materials need texture coordinates.
				// We also fill UV channel > 1 for this primitive if other primitives have it, to avoid gaps.
				UVs[UVIndex].AddZeroed(Prim.VertexCount());
				bDidGenerateTexCoords = true;
			}
		}

		//Now add all vertexInstances
		for (uint32 TriangleIndex = 0; TriangleIndex < TriCount; ++TriangleIndex)
		{
			FVertexInstanceID CornerVertexInstanceIDs[3];
			FVertexID CornerVertexIDs[3];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				uint32 IndiceIndex = TriangleIndex * 3 + Corner;
				int32 VertexIndex = Indices[IndiceIndex];

				FVertexID VertexID = PositionIndexToVertexID_PerPrim[PrimIndex][VertexIndex];
				const FVertexInstanceID& VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);

				VertexInstanceTangents[VertexInstanceID] = Tangents[IndiceIndex];
				VertexInstanceNormals[VertexInstanceID] = ConvertVec3(Normals[IndiceIndex]);
				VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(VertexInstanceTangents[VertexInstanceID].GetSafeNormal(),
																						(VertexInstanceNormals[VertexInstanceID] ^ VertexInstanceTangents[VertexInstanceID]).GetSafeNormal(),
																						VertexInstanceNormals[VertexInstanceID].GetSafeNormal());
				for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex)
				{
					VertexInstanceUVs.Set(VertexInstanceID, UVIndex, UVs[UVIndex][IndiceIndex]);
				}

				CornerVertexInstanceIDs[Corner] = VertexInstanceID;
				CornerVertexIDs[Corner] = VertexID;
			}

			TArray<FMeshDescription::FContourPoint> Contours;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				int32 ContourPointIndex = Contours.AddDefaulted();
				FMeshDescription::FContourPoint& ContourPoint = Contours[ContourPointIndex];
				//Find the matching edge ID
				uint32 CornerIndices[2];
				CornerIndices[0] = (Corner + 0) % 3;
				CornerIndices[1] = (Corner + 1) % 3;

				FVertexID EdgeVertexIDs[2];
				EdgeVertexIDs[0] = CornerVertexIDs[CornerIndices[0]];
				EdgeVertexIDs[1] = CornerVertexIDs[CornerIndices[1]];

				FEdgeID MatchEdgeId = MeshDescription->GetVertexPairEdge(EdgeVertexIDs[0], EdgeVertexIDs[1]);
				if (MatchEdgeId == FEdgeID::Invalid)
				{
					MatchEdgeId = MeshDescription->CreateEdge(EdgeVertexIDs[0], EdgeVertexIDs[1]);
					// Make all faces part of the same smoothing group, so Unreal will combine identical adjacent verts.
					// (Is there a way to set auto-gen smoothing threshold? glTF spec says to generate flat normals if they're not specified.
					//   We want to combine identical verts whether they're smooth neighbors or triangles belonging to the same flat polygon.)
					EdgeHardnesses[MatchEdgeId] = false;
					EdgeCreaseSharpnesses[MatchEdgeId] = 0.0f;
				}
				ContourPoint.EdgeID = MatchEdgeId;
				ContourPoint.VertexInstanceID = CornerVertexInstanceIDs[CornerIndices[0]];
			}
			// Insert a polygon into the mesh
			const FPolygonID NewPolygonID = MeshDescription->CreatePolygon(CurrentPolygonGroupID, Contours);
			//Triangulate the polygon
			FMeshPolygon& Polygon = MeshDescription->GetPolygon(NewPolygonID);
			MeshDescription->ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
		}
	}
	
	// RawMesh.CompactMaterialIndices(); // needed?
	bMeshUsesEmptyMaterial = MaterialIndicesUsed.Contains(INDEX_NONE);

	StaticMesh->CommitOriginalMeshDescription(LODIndex);
	StaticMesh->PostEditChange();

	// Set the dirty flag so this package will get saved later
	AssetPackage->SetDirtyFlag(true);
	FAssetRegistryModule::AssetCreated(StaticMesh);

	// TODO: warn if certain things were "fixed up" during import.

	return StaticMesh;
}

TArray<UStaticMesh*> ImportStaticMeshes(const FAsset& Asset, const TArray<UMaterial*>& Materials, UObject* InParent, FName InName, EObjectFlags Flags)
{
	const uint32 MeshCount = Asset.Meshes.Num();
	TArray<UStaticMesh*> Result;
	Result.Reserve(MeshCount);

	for (uint32 Index = 0; Index < MeshCount; ++Index)
	{
		UStaticMesh* StaticMesh = ImportStaticMesh(Asset, Materials, InParent, InName, Flags, Index);
		if (StaticMesh)
		{
			Result.Add(StaticMesh);
		}
	}

	return Result;
}
