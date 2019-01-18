// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFStaticMeshImporter.h"

#include "GLTFAsset.h"

#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "MeshAttributeArray.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "PackageTools.h"

namespace GLTF
{
	class FStaticMeshImporterImpl : public GLTF::FBaseLogger
	{
	public:
		FStaticMeshImporterImpl();

		const TArray<UStaticMesh*>& ImportMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags, bool bApplyPostEditChange);

		void CleanUp();

	private:
		UStaticMesh* CreateMesh(const GLTF::FMesh& Mesh, UObject* ParentPackage, EObjectFlags Flags);

		void SetupMeshBuildSettings(int32 NumUVs, bool bMeshHasTagents, UStaticMesh& StaticMesh, FStaticMeshSourceModel& SourceModel);

		void ImportPrimitive(const GLTF::FPrimitive&                        Primitive,  //
		                     int32                                          PrimitiveIndex,
		                     int32                                          NumUVs,
		                     bool                                           bMeshHasTagents,
		                     bool                                           bMeshHasColors,
		                     const TVertexInstanceAttributesRef<FVector>&   VertexInstanceNormals,
		                     const TVertexInstanceAttributesRef<FVector>&   VertexInstanceTangents,
		                     const TVertexInstanceAttributesRef<float>&     VertexInstanceBinormalSigns,
		                     const TVertexInstanceAttributesRef<FVector2D>& VertexInstanceUVs,
		                     const TVertexInstanceAttributesRef<FVector4>&  VertexInstanceColors,
		                     const TEdgeAttributesRef<bool>&                EdgeHardnesses,
		                     const TEdgeAttributesRef<float>&               EdgeCreaseSharpnesses);

		inline TArray<FVector4>& GetVector4dBuffer(int32 Index)
		{
			check(Index < (sizeof(Vector4dBuffers) / sizeof(Vector4dBuffers[0])));
			uint32 ReserveSize = Vector4dBuffers[Index].Num() + Vector4dBuffers[Index].GetSlack();
			Vector4dBuffers[Index].Empty(ReserveSize);
			return Vector4dBuffers[Index];
		}

		inline TArray<FVector>& GetVectorBuffer(int32 Index)
		{
			check(Index < (sizeof(VectorBuffers) / sizeof(VectorBuffers[0])));
			uint32 ReserveSize = VectorBuffers[Index].Num() + VectorBuffers[Index].GetSlack();
			VectorBuffers[Index].Empty(ReserveSize);
			return VectorBuffers[Index];
		}

		inline TArray<FVector2D>& GetVector2dBuffer(int32 Index)
		{
			check(Index < (sizeof(Vector2dBuffers) / sizeof(Vector2dBuffers[0])));
			uint32 ReserveSize = Vector2dBuffers[Index].Num() + Vector2dBuffers[Index].GetSlack();
			Vector2dBuffers[Index].Empty(ReserveSize);
			return Vector2dBuffers[Index];
		}

		inline TArray<uint32>& GetIntBuffer()
		{
			uint32 ReserveSize = IntBuffer.Num() + IntBuffer.GetSlack();
			IntBuffer.Empty(ReserveSize);
			return IntBuffer;
		}

	private:
		enum
		{
			NormalBufferIndex    = 0,
			TangentBufferIndex   = 1,
			PositionBufferIndex  = 2,
			ReindexBufferIndex   = 3,
			VectorBufferCount    = 4,
			UvReindexBufferIndex = MAX_MESH_TEXTURE_COORDS_MD,
			ColorBufferIndex     = 0,
			Reindex4dBufferIndex = 1,
			Vector4dBufferCount  = 2,
		};

		using FIndexVertexIdMap = TMap<int32, FVertexID>;

		float                ImportUniformScale;
		bool                 bGenerateLightmapUVs;
		TArray<UStaticMesh*> StaticMeshes;

		TSet<int32>                  MaterialIndicesUsed;
		TMap<int32, FPolygonGroupID> MaterialIndexToPolygonGroupID;
		TArray<FIndexVertexIdMap>    PositionIndexToVertexIdPerPrim;
		FMeshDescription*            MeshDescription;

		TArray<FVector2D>                       Vector2dBuffers[MAX_MESH_TEXTURE_COORDS_MD + 1];
		TArray<FVector>                         VectorBuffers[VectorBufferCount];
		TArray<FVector4>                        Vector4dBuffers[Vector4dBufferCount];
		TArray<uint32>                          IntBuffer;
		TArray<FVertexInstanceID>				CornerVertexInstanceIDs;

		friend class FStaticMeshImporter;
	};

	namespace
	{
		template <typename T>
		void ReIndex(const TArray<T>& Source, const TArray<uint32>& Indices, TArray<T>& Dst)
		{
			check(&Source != &Dst);

			Dst.Reserve(Indices.Num());
			for (uint32 Index : Indices)
			{
				Dst.Add(Source[Index]);
			}
		}

		void GenerateFlatNormals(const TArray<FVector>& Positions, const TArray<uint32>& Indices, TArray<FVector>& Normals)
		{
			Normals.Empty();

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
				Normals[i]     = Normal;
				Normals[i + 1] = Normal;
				Normals[i + 2] = Normal;
			}
		}

		int32 GetNumUVs(const GLTF::FMesh& Mesh)
		{
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
			return NumUVs;
		}
	}

	FStaticMeshImporterImpl::FStaticMeshImporterImpl()
	    : ImportUniformScale(1.f)
	    , bGenerateLightmapUVs(false)
	    , MeshDescription(nullptr)
	{
		CornerVertexInstanceIDs.SetNum(3);
	}

	UStaticMesh* FStaticMeshImporterImpl::CreateMesh(const FMesh& Mesh, UObject* ParentPackage, EObjectFlags Flags)
	{
		check(!Mesh.Name.IsEmpty());

		const FString PackageName  = UPackageTools::SanitizePackageName(FPaths::Combine(ParentPackage->GetName(), Mesh.Name));
		UPackage*     AssetPackage = CreatePackage(nullptr, *PackageName);
		UStaticMesh*  StaticMesh   = NewObject<UStaticMesh>(AssetPackage, *FPaths::GetBaseFilename(PackageName), Flags);

		FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();

		const int32 NumUVs = FMath::Max(1, GetNumUVs(Mesh));
		SetupMeshBuildSettings(NumUVs, Mesh.HasTangents(), *StaticMesh, SourceModel);

		// GLTF currently only supports LODs via MSFT_lod, for now use always 0
		const int32 LODIndex = 0;
		MeshDescription      = StaticMesh->CreateMeshDescription(LODIndex);
		StaticMesh->RegisterMeshAttributes(*MeshDescription);

		TVertexAttributesRef<FVector> VertexPositions =
		    MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		TEdgeAttributesRef<bool>  EdgeHardnesses = MeshDescription->EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
		TEdgeAttributesRef<float> EdgeCreaseSharpnesses =
		    MeshDescription->EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
		TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
		    MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
		TVertexInstanceAttributesRef<FVector> VertexInstanceNormals =
		    MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
		TVertexInstanceAttributesRef<FVector> VertexInstanceTangents =
		    MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
		TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns =
		    MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
		TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs =
		    MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		TVertexInstanceAttributesRef<FVector4> VertexInstanceColors =
		    MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		VertexInstanceUVs.SetNumIndices(NumUVs);

		MaterialIndicesUsed.Empty(10);
		// Add the vertex position
		PositionIndexToVertexIdPerPrim.SetNum(FMath::Max(Mesh.Primitives.Num(), PositionIndexToVertexIdPerPrim.Num()));
		for (int32 Index = 0; Index < Mesh.Primitives.Num(); ++Index)
		{
			const FPrimitive& Primitive = Mesh.Primitives[Index];
			// Remember which primitives use which materials.
			MaterialIndicesUsed.Add(Primitive.MaterialIndex);

			TArray<FVector>& Positions = GetVectorBuffer(PositionBufferIndex);
			Primitive.GetPositions(Positions);

			FIndexVertexIdMap& PositionIndexToVertexId = PositionIndexToVertexIdPerPrim[Index];
			PositionIndexToVertexId.Empty(Positions.Num());
			for (int32 PositionIndex = 0; PositionIndex < Positions.Num(); ++PositionIndex)
			{
				const FVertexID& VertexID = MeshDescription->CreateVertex();
				VertexPositions[VertexID] = Positions[PositionIndex] * ImportUniformScale;
				PositionIndexToVertexId.Add(PositionIndex, VertexID);
			}

			const FName SlotName(*FString::FromInt(Primitive.MaterialIndex));
			const int32 MeshSlot = StaticMesh->StaticMaterials.Emplace(nullptr, SlotName, SlotName);
			StaticMesh->SectionInfoMap.Set(0, MeshSlot, FMeshSectionInfo(MeshSlot));
		}

		// Add the PolygonGroup
		MaterialIndexToPolygonGroupID.Empty(10);
		for (int32 MaterialIndex : MaterialIndicesUsed)
		{
			const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
			MaterialIndexToPolygonGroupID.Add(MaterialIndex, PolygonGroupID);

			const FName ImportedSlotName(*FString::FromInt(MaterialIndex));
			PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = ImportedSlotName;
		}

		// Add the VertexInstance
		bool bMeshUsesEmptyMaterial = false;
		bool bDidGenerateTexCoords  = false;
		for (int32 Index = 0; Index < Mesh.Primitives.Num(); ++Index)
		{
			const FPrimitive& Primitive = Mesh.Primitives[Index];
			ImportPrimitive(Primitive, Index, NumUVs, Mesh.HasTangents(), Mesh.HasColors(),                                                       //
			                VertexInstanceNormals, VertexInstanceTangents, VertexInstanceBinormalSigns, VertexInstanceUVs, VertexInstanceColors,  //
			                EdgeHardnesses, EdgeCreaseSharpnesses);

			bMeshUsesEmptyMaterial |= Primitive.MaterialIndex == INDEX_NONE;
			for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex)
			{
				if (!Primitive.HasTexCoords(UVIndex))
				{
					bDidGenerateTexCoords = true;
					break;
				}
			}
		}

		if (bDidGenerateTexCoords)
		{
			Messages.Emplace(EMessageSeverity::Warning, TEXT("Mesh has primitives with no UVs generated: ") + Mesh.Name);
		}
		if (bMeshUsesEmptyMaterial)
		{
			Messages.Emplace(EMessageSeverity::Warning, TEXT("Mesh has primitives with no materials assigned: ") + Mesh.Name);
		}
		if (Mesh.HasJointWeights())
		{
			Messages.Emplace(EMessageSeverity::Warning, TEXT("Mesh has joint weights which are not supported: ") + Mesh.Name);
		}

		const bool bRecomputeTangents = !Mesh.HasTangents();
		if (bRecomputeTangents)
		{
			FMeshBuildSettings& Settings = SourceModel.BuildSettings;
			Settings.bRecomputeTangents  = true;
		}

		StaticMesh->CommitMeshDescription(LODIndex);

		return StaticMesh;
	}

	void FStaticMeshImporterImpl::SetupMeshBuildSettings(int32 NumUVs, bool bMeshHasTagents, UStaticMesh& StaticMesh,
	                                                     FStaticMeshSourceModel& SourceModel)
	{
		FMeshBuildSettings& Settings = SourceModel.BuildSettings;
		if (bGenerateLightmapUVs)
		{
			// Generate a new UV set based off the highest index UV set in the mesh
			StaticMesh.LightMapCoordinateIndex             = NumUVs;
			SourceModel.BuildSettings.SrcLightmapIndex     = NumUVs - 1;
			SourceModel.BuildSettings.DstLightmapIndex     = NumUVs;
			SourceModel.BuildSettings.bGenerateLightmapUVs = true;
		}
		else
		{
			StaticMesh.LightMapCoordinateIndex             = NumUVs - 1;
			SourceModel.BuildSettings.bGenerateLightmapUVs = false;
		}

		Settings.bRecomputeNormals  = false;
		Settings.bRecomputeTangents = !bMeshHasTagents;
		Settings.bUseMikkTSpace     = true;  // glTF spec defines that MikkTSpace algorithms should be used when tangents aren't defined

		Settings.bRemoveDegenerates        = false;
		Settings.bBuildAdjacencyBuffer     = false;
		Settings.bBuildReversedIndexBuffer = false;

		Settings.bUseHighPrecisionTangentBasis = false;
		Settings.bUseFullPrecisionUVs          = false;
	}

	void FStaticMeshImporterImpl::ImportPrimitive(const GLTF::FPrimitive&                        Primitive,  //
	                                              int32                                          PrimitiveIndex,
	                                              int32                                          NumUVs,
	                                              bool                                           bMeshHasTagents,
	                                              bool                                           bMeshHasColors,
	                                              const TVertexInstanceAttributesRef<FVector>&   VertexInstanceNormals,
	                                              const TVertexInstanceAttributesRef<FVector>&   VertexInstanceTangents,
	                                              const TVertexInstanceAttributesRef<float>&     VertexInstanceBinormalSigns,
	                                              const TVertexInstanceAttributesRef<FVector2D>& VertexInstanceUVs,
	                                              const TVertexInstanceAttributesRef<FVector4>&  VertexInstanceColors,
	                                              const TEdgeAttributesRef<bool>&                EdgeHardnesses,
	                                              const TEdgeAttributesRef<float>&               EdgeCreaseSharpnesses)
	{
		check(MeshDescription);

		const FPolygonGroupID CurrentPolygonGroupID = MaterialIndexToPolygonGroupID[Primitive.MaterialIndex];
		const uint32          TriCount              = Primitive.TriangleCount();

		TArray<uint32>& Indices = GetIntBuffer();
		Primitive.GetTriangleIndices(Indices);

		TArray<FVector>& Normals = GetVectorBuffer(NormalBufferIndex);
		// glTF does not guarantee each primitive within a mesh has the same attributes.
		// Fill in gaps as needed:
		// - missing normals will be flat, based on triangle orientation
		// - missing UVs will be (0,0)
		// - missing tangents will be (0,0,1)
		if (Primitive.HasNormals())
		{
			TArray<FVector>& ReindexBuffer = GetVectorBuffer(ReindexBufferIndex);
			Primitive.GetNormals(Normals);
			ReIndex(Normals, Indices, ReindexBuffer);
			Swap(Normals, ReindexBuffer);
		}
		else
		{
			TArray<FVector>& Positions = GetVectorBuffer(PositionBufferIndex);
			Primitive.GetPositions(Positions);
			GenerateFlatNormals(Positions, Indices, Normals);
		}

		TArray<FVector>& Tangents = GetVectorBuffer(TangentBufferIndex);
		if (Primitive.HasTangents())
		{
			TArray<FVector>& ReindexBuffer = GetVectorBuffer(ReindexBufferIndex);
			Primitive.GetTangents(Tangents);
			ReIndex(Tangents, Indices, ReindexBuffer);
			Swap(Tangents, ReindexBuffer);
		}
		else if (bMeshHasTagents)
		{
			// If other primitives in this mesh have tangents, generate filler ones for this primitive, to avoid gaps.
			Tangents.Init(FVector(0.0f, 0.0f, 1.0f), Primitive.VertexCount());
		}

		TArray<FVector4>& Colors = GetVector4dBuffer(ColorBufferIndex);
		if (Primitive.HasColors())
		{
			TArray<FVector4>& ReindexBuffer = GetVector4dBuffer(Reindex4dBufferIndex);
			Primitive.GetColors(Colors);
			ReIndex(Colors, Indices, ReindexBuffer);
			Swap(Colors, ReindexBuffer);
		}
		else if (bMeshHasColors)
		{
			// If other primitives in this mesh have colors, generate filler ones for this primitive, to avoid gaps.
			Colors.Init(FVector4(1.0f), Primitive.VertexCount());
		}

		int32_t            AvailableBufferIndex = 0;
		TArray<FVector2D>* UVs[MAX_MESH_TEXTURE_COORDS_MD];
		for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex)
		{
			UVs[UVIndex] = &GetVector2dBuffer(AvailableBufferIndex++);
			if (Primitive.HasTexCoords(UVIndex))
			{
				TArray<FVector2D>& ReindexBuffer = GetVector2dBuffer(UvReindexBufferIndex);
				Primitive.GetTexCoords(UVIndex, *UVs[UVIndex]);
				ReIndex(*UVs[UVIndex], Indices, ReindexBuffer);
				Swap(*UVs[UVIndex], ReindexBuffer);
			}
			else
			{
				// Unreal StaticMesh must have UV channel 0.
				// glTF doesn't require this since not all materials need texture coordinates.
				// We also fill UV channel > 1 for this primitive if other primitives have it, to avoid gaps.
				(*UVs[UVIndex]).AddZeroed(Primitive.VertexCount());
			}
		}

		// Now add all vertexInstances
		FVertexID         CornerVertexIDs[3];
		for (uint32 TriangleIndex = 0; TriangleIndex < TriCount; ++TriangleIndex)
		{
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 IndiceIndex = TriangleIndex * 3 + Corner;
				const int32  VertexIndex = Indices[IndiceIndex];

				const FVertexID          VertexID         = PositionIndexToVertexIdPerPrim[PrimitiveIndex][VertexIndex];
				const FVertexInstanceID& VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);

				if (Tangents.Num() > 0)
					VertexInstanceTangents[VertexInstanceID] = Tangents[IndiceIndex];
				VertexInstanceNormals[VertexInstanceID] = Normals[IndiceIndex];
				VertexInstanceBinormalSigns[VertexInstanceID] =
				    GetBasisDeterminantSign(VertexInstanceTangents[VertexInstanceID].GetSafeNormal(),
				                            (VertexInstanceNormals[VertexInstanceID] ^ VertexInstanceTangents[VertexInstanceID]).GetSafeNormal(),
				                            VertexInstanceNormals[VertexInstanceID].GetSafeNormal());

				for (int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex)
				{
					VertexInstanceUVs.Set(VertexInstanceID, UVIndex, (*UVs[UVIndex])[IndiceIndex]);
				}
				if (Colors.Num() > 0)
					VertexInstanceColors[VertexInstanceID] = Colors[IndiceIndex];

				CornerVertexInstanceIDs[Corner] = VertexInstanceID;
				CornerVertexIDs[Corner]         = VertexID;
			}

			// Insert a polygon into the mesh
			TArray<FEdgeID> NewEdgeIDs;
			const FPolygonID NewPolygonID = MeshDescription->CreatePolygon(CurrentPolygonGroupID, CornerVertexInstanceIDs, &NewEdgeIDs);
			for (const FEdgeID NewEdgeID : NewEdgeIDs)
			{
				// Make all faces part of the same smoothing group, so Unreal will combine identical adjacent verts.
				// (Is there a way to set auto-gen smoothing threshold? glTF spec says to generate flat normals if they're not specified.
				//   We want to combine identical verts whether they're smooth neighbors or triangles belonging to the same flat polygon.)
				EdgeHardnesses[NewEdgeID]        = false;
				EdgeCreaseSharpnesses[NewEdgeID] = 0.0f;
			}

			// Triangulate the polygon
			FMeshPolygon& Polygon = MeshDescription->GetPolygon(NewPolygonID);
			MeshDescription->ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
		}
	}

	inline const TArray<UStaticMesh*>& FStaticMeshImporterImpl::ImportMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags,
	                                                                         bool bApplyPostEditChange)
	{
		const uint32 MeshCount = Asset.Meshes.Num();
		StaticMeshes.Empty();
		StaticMeshes.Reserve(MeshCount);

		Messages.Empty();
		for (const GLTF::FMesh& Mesh : Asset.Meshes)
		{
			UStaticMesh* StaticMesh = CreateMesh(Mesh, ParentPackage, Flags);
			if (!StaticMesh)
			{
				continue;
			}

			if (PositionIndexToVertexIdPerPrim.Num() > 10)
				PositionIndexToVertexIdPerPrim.SetNum(10);  // free some memory

			StaticMeshes.Add(StaticMesh);
			if (bApplyPostEditChange)
			{
				StaticMesh->MarkPackageDirty();
				StaticMesh->PostEditChange();
				FAssetRegistryModule::AssetCreated(StaticMesh);
			}
		}

		return StaticMeshes;
	}

	inline void FStaticMeshImporterImpl::CleanUp()
	{
		StaticMeshes.Empty();

		const uint32 ReserveSize = FMath::Min(IntBuffer.Num() + IntBuffer.GetSlack(), 100000);  // cap reserved size
		IntBuffer.Empty(ReserveSize);
		Vector2dBuffers[0].Empty(ReserveSize);
		for (TArray<FVector>& Array : VectorBuffers)
		{
			Array.Empty(ReserveSize);
		}
		for (int32 Index = 1; Index < MAX_MESH_TEXTURE_COORDS_MD + 1; ++Index)
		{
			TArray<FVector2D>& Array = Vector2dBuffers[Index];
			Array.Empty();
		}
		for (TArray<FVector4>& Array : Vector4dBuffers)
		{
			Array.Empty();
		}
	}

	//

	FStaticMeshImporter::FStaticMeshImporter()
	    : Impl(new FStaticMeshImporterImpl())
	{
	}

	FStaticMeshImporter::~FStaticMeshImporter() {}

	const TArray<UStaticMesh*>& FStaticMeshImporter::ImportMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags,
	                                                              bool bApplyPostEditChange)
	{
		return Impl->ImportMeshes(Asset, ParentPackage, Flags, bApplyPostEditChange);
	}

	const TArray<UStaticMesh*>& FStaticMeshImporter::GetMeshes() const
	{
		return Impl->StaticMeshes;
	}

	const TArray<FLogMessage>& FStaticMeshImporter::GetLogMessages() const
	{
		return Impl->GetLogMessages();
	}

	float FStaticMeshImporter::GetUniformScale() const
	{
		return Impl->ImportUniformScale;
	}

	void FStaticMeshImporter::SetUniformScale(float Scale)
	{
		Impl->ImportUniformScale = Scale;
	}

	bool FStaticMeshImporter::GetGenerateLightmapUVs() const
	{
		return Impl->bGenerateLightmapUVs;
	}

	void FStaticMeshImporter::SetGenerateLightmapUVs(bool bInGenerateLightmapUVs)
	{
		Impl->bGenerateLightmapUVs = bInGenerateLightmapUVs;
	}

	void FStaticMeshImporter::CleanUp()
	{
		Impl->CleanUp();
	}

}  //  namespace GLTF
