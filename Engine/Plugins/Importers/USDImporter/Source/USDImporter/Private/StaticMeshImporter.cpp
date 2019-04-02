// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshImporter.h"
#include "USDImporter.h"
#include "USDConversionUtils.h"
#include "RawMesh.h"
#include "MeshUtilities.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "USDAssetImportData.h"
#include "Factories/Factory.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "IMeshBuilderModule.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "USDImportPlugin"

struct FMeshDescriptionWrapper
{
	struct FVertexAttributes
	{
		FVertexAttributes(FMeshDescription* MeshDescription) :
			Positions(MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position)),
			Normals(MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal)),
			Tangents(MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent)),
			BinormalSigns(MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign)),
			Colors(MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color)),
			UVs(MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate))
		{
		}
		TVertexAttributesRef<FVector> Positions;
		TVertexInstanceAttributesRef<FVector> Normals;
		TVertexInstanceAttributesRef<FVector> Tangents;
		TVertexInstanceAttributesRef<float> BinormalSigns;
		TVertexInstanceAttributesRef<FVector4> Colors;
		TVertexInstanceAttributesRef<FVector2D> UVs;
	};

	FMeshDescriptionWrapper(FMeshDescription* MeshDescription) :
		Vertex(MeshDescription),
		EdgeHardnesses(MeshDescription->EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard)),
		EdgeCreaseSharpnesses(MeshDescription->EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness)),
		PolygonGroupImportedMaterialSlotNames(MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName))
	{
	}

	FVertexAttributes Vertex;
	TEdgeAttributesRef<bool> EdgeHardnesses;
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses;
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames;
};

struct FUSDImportMaterialInfo
{
	FUSDImportMaterialInfo() :
		UnrealMaterial(nullptr)
	{

	}
	FString Name;
	UMaterialInterface* UnrealMaterial;
};

struct FUSDStaticMeshImportState
{
public:
	FUSDStaticMeshImportState(FUsdImportContext& InImportContext, TArray<FUSDImportMaterialInfo>& InMaterials) :
		ImportContext(InImportContext),
		Materials(InMaterials),
		MeshDescription(nullptr)
	{
	}

	FUsdImportContext& ImportContext;
	TArray<FUSDImportMaterialInfo>& Materials;
	FTransform FinalTransform;
	FMatrix FinalTransformIT;
	FMeshDescription* MeshDescription;
	UUSDImportOptions* ImportOptions;
	UStaticMesh* NewMesh;
	bool bFlip;

private:
	int32 VertexOffset;
	int32 VertexInstanceOffset;
	int32 PolygonOffset;
	int32 MaterialIndexOffset;

public:
	void ProcessStaticUSDGeometry(IUsdPrim* GeomPrim, int32 LODIndex);
	void ProcessMaterials(int32 LODIndex);

private:
	void AddVertexPositions(FMeshDescriptionWrapper& DestMeshWrapper, const FUsdGeomData& GeomData);
	bool AddPolygons(FMeshDescriptionWrapper& DestMeshWrapper, const FUsdGeomData& GeomData);
};

void FUSDStaticMeshImportState::ProcessStaticUSDGeometry(IUsdPrim* GeomPrim, int32 LODIndex)
{
	const FUsdGeomData* GeomDataPtr = GeomPrim->GetGeometryData();
	const FUsdGeomData& GeomData = *GeomDataPtr;

	FMeshDescriptionWrapper DestMeshWrapper(MeshDescription);

	VertexOffset = MeshDescription->Vertices().Num();
	VertexInstanceOffset = MeshDescription->VertexInstances().Num();
	PolygonOffset = MeshDescription->Polygons().Num();
	MaterialIndexOffset = Materials.Num();
	Materials.AddDefaulted(GeomData.MaterialNames.size());

	AddVertexPositions(DestMeshWrapper, GeomData);
	AddPolygons(DestMeshWrapper, GeomData);
}

void FUSDStaticMeshImportState::AddVertexPositions(FMeshDescriptionWrapper& DestMeshWrapper, const FUsdGeomData& GeomData)
{
	for (int32 LocalPointIndex = 0; LocalPointIndex < GeomData.Points.size(); ++LocalPointIndex)
	{
		const FUsdVectorData& Point = GeomData.Points[LocalPointIndex];
		FVector Pos = FVector(-Point.X, Point.Y, Point.Z);
		Pos = FinalTransform.TransformPosition(Pos);

		FVertexID AddedVertexId = MeshDescription->CreateVertex();
		DestMeshWrapper.Vertex.Positions[AddedVertexId] = Pos;
	}
}

bool FUSDStaticMeshImportState::AddPolygons(FMeshDescriptionWrapper& DestMeshWrapper, const FUsdGeomData& GeomData)
{
	// When importing multiple mesh pieces to the same static mesh.  Ensure each mesh piece has the same number of Uv's
	{
		int32 ExistingUVCount = DestMeshWrapper.Vertex.UVs.GetNumIndices();
		int32 NumUVs = FMath::Max(GeomData.NumUVs, ExistingUVCount);
		NumUVs = FMath::Min<int32>(MAX_MESH_TEXTURE_COORDS, NumUVs);
		// At least one UV set must exist.  
		NumUVs = FMath::Max<int32>(1, NumUVs);

		//Make sure all Vertex instance have the correct number of UVs
		DestMeshWrapper.Vertex.UVs.SetNumIndices(NumUVs);
	}

	TMap<int32, FPolygonGroupID> PolygonGroupMapping;
	TArray<FVertexInstanceID> CornerInstanceIDs;
	TArray<FVertexID> CornerVerticesIDs;
	int32 CurrentVertexInstanceIndex = 0;

	bool bFlipThisGeometry = bFlip;
	if (GeomData.Orientation == EUsdGeomOrientation::LeftHanded)
	{
		bFlipThisGeometry = !bFlip;
	}

	for (int32 PolygonIndex = 0; PolygonIndex < GeomData.FaceVertexCounts.size(); ++PolygonIndex)
	{
		int32 PolygonVertexCount = GeomData.FaceVertexCounts[PolygonIndex];
		CornerInstanceIDs.Reset();
		CornerInstanceIDs.AddUninitialized(PolygonVertexCount);
		CornerVerticesIDs.Reset();
		CornerVerticesIDs.AddUninitialized(PolygonVertexCount);

		for (int32 CornerIndex = 0; CornerIndex < PolygonVertexCount; ++CornerIndex, ++CurrentVertexInstanceIndex)
		{
			int32 VertexInstanceIndex = VertexInstanceOffset + CurrentVertexInstanceIndex;
			const FVertexInstanceID VertexInstanceID(VertexInstanceIndex);
			CornerInstanceIDs[CornerIndex] = VertexInstanceID;
			const int32 ControlPointIndex = GeomData.FaceIndices[CurrentVertexInstanceIndex];
			const FVertexID VertexID(VertexOffset + ControlPointIndex);
			const FVector VertexPosition = DestMeshWrapper.Vertex.Positions[VertexID];
			CornerVerticesIDs[CornerIndex] = VertexID;

			FVertexInstanceID AddedVertexInstanceId = MeshDescription->CreateVertexInstance(VertexID);

			if (GeomData.Normals.size() > 0)
			{
				const int32 NormalIndex = GeomData.Normals.size() != GeomData.FaceIndices.size() ? GeomData.FaceIndices[CurrentVertexInstanceIndex] : CurrentVertexInstanceIndex;
				check(NormalIndex < GeomData.Normals.size());
				const FUsdVectorData& Normal = GeomData.Normals[NormalIndex];
				//FVector TransformedNormal = ConversionMatrixIT.TransformVector(PrimToWorldIT.TransformVector(FVector(Normal.X, Normal.Y, Normal.Z)));
				FVector TransformedNormal = FinalTransformIT.TransformVector(FVector(-Normal.X, Normal.Y, Normal.Z));

				DestMeshWrapper.Vertex.Normals[AddedVertexInstanceId] = TransformedNormal.GetSafeNormal();
			}

			for (int32 UVLayerIndex = 0; UVLayerIndex < GeomData.NumUVs; ++UVLayerIndex)
			{
				EUsdInterpolationMethod UVInterpMethod = GeomData.UVs[UVLayerIndex].UVInterpMethod;

				// Get the index into the point array for this wedge
				const int32 PointIndex = UVInterpMethod != EUsdInterpolationMethod::FaceVarying ? GeomData.FaceIndices[CurrentVertexInstanceIndex] : CurrentVertexInstanceIndex;

				// In this mode there is a single vertex per vertex so 
				// the point index should match up
				check(PointIndex < GeomData.UVs[UVLayerIndex].Coords.size());
				const FUsdVector2Data& UV = GeomData.UVs[UVLayerIndex].Coords[PointIndex];

				// Flip V for Unreal uv's which match directx
				FVector2D FinalUVVector(UV.X, 1.f - UV.Y);
				DestMeshWrapper.Vertex.UVs.Set(AddedVertexInstanceId, UVLayerIndex, FinalUVVector);
			}
		}

		int32 MaterialIndex = 0;
		if (PolygonIndex >= 0 && PolygonIndex < GeomData.FaceMaterialIndices.size())
		{
			MaterialIndex = GeomData.FaceMaterialIndices[PolygonIndex];
			if (MaterialIndex < 0 || MaterialIndex > GeomData.MaterialNames.size())
			{
				MaterialIndex = 0;
			}
		}

		int32 RealMaterialIndex = MaterialIndexOffset + MaterialIndex;
		if (!PolygonGroupMapping.Contains(RealMaterialIndex))
		{
			FName ImportedMaterialSlotName;
			if (MaterialIndex >= 0 && MaterialIndex < GeomData.MaterialNames.size())
			{
				FString MaterialName = USDToUnreal::ConvertString(GeomData.MaterialNames[MaterialIndex]);
				ImportedMaterialSlotName = FName(*MaterialName);
				Materials[RealMaterialIndex].Name = MaterialName;
			}

			FPolygonGroupID ExistingPolygonGroup = FPolygonGroupID::Invalid;
			for (const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs())
			{
				if (DestMeshWrapper.PolygonGroupImportedMaterialSlotNames[PolygonGroupID] == ImportedMaterialSlotName)
				{
					ExistingPolygonGroup = PolygonGroupID;
					break;
				}
			}
			if (ExistingPolygonGroup == FPolygonGroupID::Invalid)
			{
				ExistingPolygonGroup = MeshDescription->CreatePolygonGroup();
				DestMeshWrapper.PolygonGroupImportedMaterialSlotNames[ExistingPolygonGroup] = ImportedMaterialSlotName;
			}
			PolygonGroupMapping.Add(RealMaterialIndex, ExistingPolygonGroup);
		}

		FPolygonGroupID PolygonGroupID = PolygonGroupMapping[RealMaterialIndex];
		// Insert a polygon into the mesh
		const FPolygonID NewPolygonID = MeshDescription->CreatePolygon(PolygonGroupID, CornerInstanceIDs);
		if (bFlipThisGeometry)
		{
			MeshDescription->ReversePolygonFacing(NewPolygonID);
		}
		else
		{
			FMeshPolygon& Polygon = MeshDescription->GetPolygon(NewPolygonID);
			MeshDescription->ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
		}
	}

	return true;
}

void FUSDStaticMeshImportState::ProcessMaterials(int32 LODIndex)
{
	const FString BasePackageName = FPackageName::GetLongPackagePath(NewMesh->GetOutermost()->GetName());

	FMeshDescriptionWrapper DestMeshWrapper(MeshDescription);
	TArray<FStaticMaterial> MaterialToAdd;
	for (const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs())
	{
		const FName& ImportedMaterialSlotName = DestMeshWrapper.PolygonGroupImportedMaterialSlotNames[PolygonGroupID];
		const FString ImportedMaterialSlotNameString = ImportedMaterialSlotName.ToString();
		const FName MaterialSlotName = ImportedMaterialSlotName;
		int32 MaterialIndex = INDEX_NONE;
		for (int32 MeshMaterialIndex = 0; MeshMaterialIndex < Materials.Num(); ++MeshMaterialIndex)
		{
			FUSDImportMaterialInfo& MeshMaterial = Materials[MeshMaterialIndex];
			if (MeshMaterial.Name.Equals(ImportedMaterialSlotNameString))
			{
				MaterialIndex = MeshMaterialIndex;
				break;
			}
		}
		if (MaterialIndex == INDEX_NONE)
		{
			MaterialIndex = PolygonGroupID.GetValue();
		}

		UMaterialInterface* Material = nullptr;
		if (Materials.IsValidIndex(MaterialIndex))
		{
			Material = Materials[MaterialIndex].UnrealMaterial;
			if (Material == nullptr)
			{
				const FString& MaterialFullName = Materials[MaterialIndex].Name;
				FString MaterialBasePackageName = BasePackageName;
				MaterialBasePackageName += TEXT("/");
				MaterialBasePackageName += MaterialFullName;
				MaterialBasePackageName = UPackageTools::SanitizePackageName(MaterialBasePackageName);

				// The material could already exist in the project
				//FName ObjectPath = *(MaterialBasePackageName + TEXT(".") + MaterialFullName);

				FText Error;
				Material = UMaterialImportHelpers::FindExistingMaterialFromSearchLocation(MaterialFullName, MaterialBasePackageName, ImportOptions->MaterialSearchLocation, Error);
				if (Material)
				{
					Materials[MaterialIndex].UnrealMaterial = Material;
				}
				else
				{
					Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}
			}
		}
		if (Material == nullptr)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		FStaticMaterial StaticMaterial(Material, MaterialSlotName, ImportedMaterialSlotName);
		if (LODIndex > 0)
		{
			MaterialToAdd.Add(StaticMaterial);
		}
		else
		{
			NewMesh->StaticMaterials.Add(StaticMaterial);
		}
	}
	if (LODIndex > 0)
	{
		//Insert the new materials in the static mesh
		// TODO
	}
}

UStaticMesh* FUSDStaticMeshImporter::ImportStaticMesh(FUsdImportContext& ImportContext, const FUsdAssetPrimToImport& PrimToImport)
{
	IUsdPrim* Prim = PrimToImport.Prim;

	const FTransform& ConversionTransform = ImportContext.ConversionTransform;
	const FMatrix PrimToWorld = ImportContext.bApplyWorldTransformToGeometry ? USDToUnreal::ConvertMatrix(Prim->GetLocalToWorldTransform()) : FMatrix::Identity;
	FTransform FinalTransform = FTransform(PrimToWorld) * ConversionTransform;
	if (ImportContext.ImportOptions->Scale != 1.0)
	{
		FVector Scale3D = FinalTransform.GetScale3D() * ImportContext.ImportOptions->Scale;
		FinalTransform.SetScale3D(Scale3D);
	}
	FMatrix FinalTransformIT = FinalTransform.ToInverseMatrixWithScale().GetTransposed();
	bool bFlip = FinalTransform.GetDeterminant() < 0.0f;


	int32 NumLODs = PrimToImport.NumLODs;

	UUSDImportOptions* ImportOptions = nullptr;
	UStaticMesh* NewMesh = USDUtils::FindOrCreateObject<UStaticMesh>(ImportContext.Parent, ImportContext.ObjectName, ImportContext.ImportObjectFlags);
	check(NewMesh);
	UUSDAssetImportData* ImportData = Cast<UUSDAssetImportData>(NewMesh->AssetImportData);
	if (!ImportData)
	{
		ImportData = NewObject<UUSDAssetImportData>(NewMesh);
		ImportData->ImportOptions = DuplicateObject<UUSDImportOptions>(ImportContext.ImportOptions, ImportData);
		NewMesh->AssetImportData = ImportData;
	}
	else if (!ImportData->ImportOptions)
	{
		ImportData->ImportOptions = DuplicateObject<UUSDImportOptions>(ImportContext.ImportOptions, ImportData);
	}
	ImportOptions = CastChecked<UUSDAssetImportData>(NewMesh->AssetImportData)->ImportOptions;
	check(ImportOptions);

	FString CurrentFilename = UFactory::GetCurrentFilename();
	if (!CurrentFilename.IsEmpty())
	{
		NewMesh->AssetImportData->Update(UFactory::GetCurrentFilename());
	}

	NewMesh->StaticMaterials.Empty();

	TArray<FUSDImportMaterialInfo> Materials;
	FUSDStaticMeshImportState State(ImportContext, Materials);
	State.FinalTransform = FinalTransform;
	State.FinalTransformIT = FinalTransformIT;
	State.bFlip = bFlip;
	State.ImportOptions = ImportOptions;
	State.NewMesh = NewMesh;

	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		if (NewMesh->SourceModels.Num() < LODIndex + 1)
		{
			// Add one LOD 
			NewMesh->AddSourceModel();

			if (NewMesh->SourceModels.Num() < LODIndex + 1)
			{
				LODIndex = NewMesh->SourceModels.Num() - 1;
			}
		}

		TArray<IUsdPrim*> PrimsWithGeometry;
		for (IUsdPrim* MeshPrim : PrimToImport.MeshPrims)
		{
			if (MeshPrim->GetNumLODs() > LODIndex)
			{
				// If the mesh has LOD children at this index then use that as the geom prim
				MeshPrim->SetActiveLODIndex(LODIndex);

				ImportContext.PrimResolver->FindMeshChildren(ImportContext, MeshPrim, false, PrimsWithGeometry);
			}
			else if (LODIndex == 0)
			{
				// If a mesh has no lods then it should only contribute to the base LOD
				PrimsWithGeometry.Add(MeshPrim);
			}
		}

		//Create private asset in the same package as the StaticMesh, and make sure reference are set to avoid GC
		State.MeshDescription = NewMesh->CreateMeshDescription(LODIndex);
		check(State.MeshDescription != nullptr);
		NewMesh->RegisterMeshAttributes(*State.MeshDescription);

		bool bRecomputeNormals = false;

		for (IUsdPrim* GeomPrim : PrimsWithGeometry)
		{
			// If we dont have a geom prim this might not be an error so dont message it.  The geom prim may not contribute to the LOD for whatever reason
			if (GeomPrim)
			{
				const FUsdGeomData* GeomDataPtr = GeomPrim->GetGeometryData();
				if (GeomDataPtr)
				{
					if (GeomDataPtr->Normals.size() == 0 && GeomDataPtr->Points.size() > 0)
					{
						bRecomputeNormals = true;
					}

					State.ProcessStaticUSDGeometry(GeomPrim, LODIndex);
				}
				else
				{
					ImportContext.AddErrorMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("StaticMeshesMustBeTriangulated", "{0} is not a triangle mesh. Static meshes must be triangulated to import"), FText::FromString(ImportContext.ObjectName)));

					if(NewMesh)
					{
						NewMesh->ClearFlags(RF_Standalone);
						NewMesh = nullptr;
					}
					break;
				}
			}
		}

		if (!NewMesh)
		{
			break;
		}

		if (!NewMesh->SourceModels.IsValidIndex(LODIndex))
		{
			// Add one LOD 
			NewMesh->AddSourceModel();
		}

		State.ProcessMaterials(LODIndex);

		NewMesh->CommitMeshDescription(LODIndex);

		FStaticMeshSourceModel& SrcModel = NewMesh->SourceModels[LODIndex];
		SrcModel.BuildSettings.bGenerateLightmapUVs = false;
		SrcModel.BuildSettings.bRecomputeNormals = bRecomputeNormals;
		SrcModel.BuildSettings.bRecomputeTangents = true;
		SrcModel.BuildSettings.bBuildAdjacencyBuffer = false;

		NewMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	}

	if(NewMesh)
	{
		NewMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		NewMesh->CreateBodySetup();

		NewMesh->SetLightingGuid();

		NewMesh->PostEditChange();
	}

	return NewMesh;
}

#undef LOCTEXT_NAMESPACE
