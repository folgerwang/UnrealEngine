// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ProceduralMeshComponentDetails.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Application/SlateWindowHelper.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Engine/StaticMesh.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "ProceduralMeshComponent.h"

#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"
#include "MeshDescriptionOperations.h"

#include "Dialogs/DlgPickAssetPath.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "ProceduralMeshComponentDetails"

TSharedRef<IDetailCustomization> FProceduralMeshComponentDetails::MakeInstance()
{
	return MakeShareable(new FProceduralMeshComponentDetails);
}

void FProceduralMeshComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& ProcMeshCategory = DetailBuilder.EditCategory("ProceduralMesh");

	const FText ConvertToStaticMeshText = LOCTEXT("ConvertToStaticMesh", "Create StaticMesh");

	// Cache set of selected things
	SelectedObjectsList = DetailBuilder.GetSelectedObjects();

	ProcMeshCategory.AddCustomRow(ConvertToStaticMeshText, false)
	.NameContent()
	[
		SNullWidget::NullWidget
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	.MaxDesiredWidth(250)
	[
		SNew(SButton)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("ConvertToStaticMeshTooltip", "Create a new StaticMesh asset using current geometry from this ProceduralMeshComponent. Does not modify instance."))
		.OnClicked(this, &FProceduralMeshComponentDetails::ClickedOnConvertToStaticMesh)
		.IsEnabled(this, &FProceduralMeshComponentDetails::ConvertToStaticMeshEnabled)
		.Content()
		[
			SNew(STextBlock)
			.Text(ConvertToStaticMeshText)
		]
	];
}

UProceduralMeshComponent* FProceduralMeshComponentDetails::GetFirstSelectedProcMeshComp() const
{
	// Find first selected valid ProcMeshComp
	UProceduralMeshComponent* ProcMeshComp = nullptr;
	for (const TWeakObjectPtr<UObject>& Object : SelectedObjectsList)
	{
		UProceduralMeshComponent* TestProcComp = Cast<UProceduralMeshComponent>(Object.Get());
		// See if this one is good
		if (TestProcComp != nullptr && !TestProcComp->IsTemplate())
		{
			ProcMeshComp = TestProcComp;
			break;
		}
	}

	return ProcMeshComp;
}


bool FProceduralMeshComponentDetails::ConvertToStaticMeshEnabled() const
{
	return GetFirstSelectedProcMeshComp() != nullptr;
}


FReply FProceduralMeshComponentDetails::ClickedOnConvertToStaticMesh()
{
	// Find first selected ProcMeshComp
	UProceduralMeshComponent* ProcMeshComp = GetFirstSelectedProcMeshComp();
	if (ProcMeshComp != nullptr)
	{
		FString NewNameSuggestion = FString(TEXT("ProcMesh"));
		FString PackageName = FString(TEXT("/Game/Meshes/")) + NewNameSuggestion;
		FString Name;
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

		TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("ConvertToStaticMeshPickName", "Choose New StaticMesh Location"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (PickAssetPathWidget->ShowModal() == EAppReturnType::Ok)
		{
			// Get the full name of where we want to create the physics asset.
			FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
			FName MeshName(*FPackageName::GetLongPackageAssetName(UserPackageName));

			// Check if the user inputed a valid asset name, if they did not, give it the generated default name
			if (MeshName == NAME_None)
			{
				// Use the defaults that were already generated.
				UserPackageName = PackageName;
				MeshName = *Name;
			}


			FMeshDescription MeshDescription;
			UStaticMesh::RegisterMeshAttributes(MeshDescription);
			FStaticMeshDescriptionAttributeGetter AttributeGetter(&MeshDescription);
			TPolygonGroupAttributesRef<FName> PolygonGroupNames = AttributeGetter.GetPolygonGroupImportedMaterialSlotNames();
			TVertexAttributesRef<FVector> VertexPositions = AttributeGetter.GetPositions();
			TVertexInstanceAttributesRef<FVector> Tangents = AttributeGetter.GetTangents();
			TVertexInstanceAttributesRef<float> BinormalSigns = AttributeGetter.GetBinormalSigns();
			TVertexInstanceAttributesRef<FVector> Normals = AttributeGetter.GetNormals();
			TVertexInstanceAttributesRef<FVector4> Colors = AttributeGetter.GetColors();
			TVertexInstanceAttributesRef<FVector2D> UVs = AttributeGetter.GetUVs();
			TEdgeAttributesRef<bool> EdgeHardnesses = AttributeGetter.GetEdgeHardnesses();
			TEdgeAttributesRef<float> EdgeCreaseSharpnesses = AttributeGetter.GetEdgeCreaseSharpnesses();

			// Materials to apply to new mesh
			const int32 NumSections = ProcMeshComp->GetNumSections();
			int32 VertexCount = 0;
			int32 VertexInstanceCount = 0;
			int32 PolygonCount = 0;
			TMap<UMaterialInterface*, FPolygonGroupID> UniqueMaterials;
			UniqueMaterials.Reserve(NumSections);
			TArray<FPolygonGroupID> MaterialRemap;
			MaterialRemap.Reserve(NumSections);
			//Get all the info we need to create the MeshDescription
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				FProcMeshSection* ProcSection = ProcMeshComp->GetProcMeshSection(SectionIdx);
				VertexCount += ProcSection->ProcVertexBuffer.Num();
				VertexInstanceCount += ProcSection->ProcIndexBuffer.Num();
				PolygonCount += ProcSection->ProcIndexBuffer.Num() / 3;
				UMaterialInterface*Material = ProcMeshComp->GetMaterial(SectionIdx);
				if (!UniqueMaterials.Contains(Material))
				{
					FPolygonGroupID NewPolygonGroup = MeshDescription.CreatePolygonGroup();
					UniqueMaterials.Add(Material, NewPolygonGroup);
					PolygonGroupNames[NewPolygonGroup] = Material->GetFName();
				}
				FPolygonGroupID* PolygonGroupID = UniqueMaterials.Find(Material);
				check(PolygonGroupID != nullptr);
				MaterialRemap.Add(*PolygonGroupID);
			}
			MeshDescription.ReserveNewVertices(VertexCount);
			MeshDescription.ReserveNewVertexInstances(VertexInstanceCount);
			MeshDescription.ReserveNewPolygons(PolygonCount);
			MeshDescription.ReserveNewEdges(PolygonCount * 2);
			UVs.SetNumIndices(4);
			//Add Vertex and VertexInstance and polygon for each section
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				FProcMeshSection* ProcSection = ProcMeshComp->GetProcMeshSection(SectionIdx);
				FPolygonGroupID PolygonGroupID = MaterialRemap[SectionIdx];
				//Create the vertex
				int32 NumVertex = ProcSection->ProcVertexBuffer.Num();
				TMap<int32, FVertexID> VertexIndexToVertexID;
				VertexIndexToVertexID.Reserve(NumVertex); 
				for (int32 VertexIndex = 0; VertexIndex < NumVertex; ++VertexIndex)
				{
					FProcMeshVertex& Vert = ProcSection->ProcVertexBuffer[VertexIndex];
					const FVertexID VertexID = MeshDescription.CreateVertex();
					VertexPositions[VertexID] = Vert.Position;
					VertexIndexToVertexID.Add(VertexIndex, VertexID);
				}
				//Create the VertexInstance
				int32 NumIndices = ProcSection->ProcIndexBuffer.Num();
				int32 NumTri = NumIndices / 3;
				TMap<int32, FVertexInstanceID> IndiceIndexToVertexInstanceID;
				IndiceIndexToVertexInstanceID.Reserve(NumVertex);
				for (int32 IndiceIndex = 0; IndiceIndex < NumIndices; IndiceIndex++)
				{
					const int32 VertexIndex = ProcSection->ProcIndexBuffer[IndiceIndex];
					const FVertexID VertexID = VertexIndexToVertexID[VertexIndex];
					const FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
					IndiceIndexToVertexInstanceID.Add(IndiceIndex, VertexInstanceID);

					FProcMeshVertex& ProcVertex = ProcSection->ProcVertexBuffer[VertexIndex];

					Tangents[VertexInstanceID] = ProcVertex.Tangent.TangentX;
					Normals[VertexInstanceID] = ProcVertex.Normal;
					BinormalSigns[VertexInstanceID] = ProcVertex.Tangent.bFlipTangentY ? -1.f : 1.f;

					Colors[VertexInstanceID] = FLinearColor(ProcVertex.Color);

					UVs.Set(VertexInstanceID, 0, ProcVertex.UV0);
					UVs.Set(VertexInstanceID, 1, ProcVertex.UV1);
					UVs.Set(VertexInstanceID, 2, ProcVertex.UV2);
					UVs.Set(VertexInstanceID, 3, ProcVertex.UV3);
				}
				
				//Create the polygons for this section
				for (int32 TriIdx = 0; TriIdx < NumTri; TriIdx++)
				{
					FVertexID VertexIndexes[3];
					TArray<FVertexInstanceID> VertexInstanceIDs;
					VertexInstanceIDs.SetNum(3);

					for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
					{
						const int32 IndiceIndex = (TriIdx * 3) + CornerIndex;
						const int32 VertexIndex = ProcSection->ProcIndexBuffer[IndiceIndex];
						VertexIndexes[CornerIndex] = VertexIndexToVertexID[VertexIndex];
						VertexInstanceIDs[CornerIndex] = IndiceIndexToVertexInstanceID[IndiceIndex];
					}

					// Insert a polygon into the mesh
					const FPolygonID NewPolygonID = MeshDescription.CreatePolygon(PolygonGroupID, VertexInstanceIDs);
					//Triangulate the polygon
					FMeshPolygon& Polygon = MeshDescription.GetPolygon(NewPolygonID);
					MeshDescription.ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
				}
			}

			// If we got some valid data.
			if (MeshDescription.Polygons().Num() > 0)
			{
				// Then find/create it.
				UPackage* Package = CreatePackage(NULL, *UserPackageName);
				check(Package);

				// Create StaticMesh object
				UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone);
				StaticMesh->InitResources();

				StaticMesh->LightingGuid = FGuid::NewGuid();

				// Add source to new StaticMesh
				FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
				SrcModel.BuildSettings.bRecomputeNormals = false;
				SrcModel.BuildSettings.bRecomputeTangents = false;
				SrcModel.BuildSettings.bRemoveDegenerates = false;
				SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
				SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
				SrcModel.BuildSettings.bGenerateLightmapUVs = true;
				SrcModel.BuildSettings.SrcLightmapIndex = 0;
				SrcModel.BuildSettings.DstLightmapIndex = 1;
				FMeshDescription* OriginalMeshDescription = StaticMesh->GetMeshDescription(0);
				if (OriginalMeshDescription == nullptr)
				{
					OriginalMeshDescription = StaticMesh->CreateMeshDescription(0);
				}
				*OriginalMeshDescription = MeshDescription;
				StaticMesh->CommitMeshDescription(0);

				// Copy materials to new mesh
				for (auto Kvp : UniqueMaterials)
				{
					UMaterialInterface* Material = Kvp.Key;
					StaticMesh->StaticMaterials.Add(FStaticMaterial(Material, Material->GetFName(), Material->GetFName()));
				}

				//Set the Imported version before calling the build
				StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

				// Build mesh from source
				StaticMesh->Build(false);
				StaticMesh->PostEditChange();

				// Notify asset registry of new asset
				FAssetRegistryModule::AssetCreated(StaticMesh);
			}
		}
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
