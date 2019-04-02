// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SplitMesh.h"
#include "Components/BoxComponent.h"
#include "EditableMesh.h"
#include "EditableMeshFactory.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "IMeshEditorModeEditingContract.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "MeshEditor/MeshEditorMode.h"
#include "PackageTools.h"
#include "Settings/EditorExperimentalSettings.h"
#include "ScopedTransaction.h"
#include "Engine/StaticMeshActor.h"
#include "Layers/ILayers.h"
#include "IMeshBuilderModule.h"
#include "Materials/Material.h"
#include "AssetSelection.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"

static AActor* AddActor(ULevel* InLevel, UClass* Class)
{
	check(Class);

	UWorld* World = InLevel->OwningWorld;
	ULevel* DesiredLevel = InLevel;

	// Transactionally add the actor.
	AActor* Actor = nullptr;

	{
		FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "AddActor", "Add Actor"));

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = DesiredLevel;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags = RF_Transactional;
		const auto Location = FVector(0);
		const auto Rotation = FTransform(FVector(0)).GetRotation().Rotator();
		Actor = World->SpawnActor(Class, &Location, &Rotation, SpawnInfo);

		check(Actor);
		Actor->InvalidateLightingCache();
		Actor->PostEditMove(true);
	}

	// If this actor is part of any layers (set in its default properties), add them into the visible layers list.
	GEditor->Layers->SetLayersVisibility(Actor->Layers, true);

	// Clean up.
	Actor->MarkPackageDirty();
	ULevel::LevelDirtiedEvent.Broadcast();

	return Actor;
}

static TArray<AActor*> GetSelectedActors()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if(Actor)
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

static ULevel* GetSelectedLevel()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<ULevel*> UniqueLevels;
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if(Actor)
		{
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}
	check(UniqueLevels.Num() == 1);
	return UniqueLevels[0];
}

template <typename ElementIDType>
static void CopyAllAttributes(TAttributesSet<ElementIDType>& DestAttributesSet, const TAttributesSet<ElementIDType>& SrcAttributesSet, const ElementIDType ElementID)
{
	SrcAttributesSet.ForEach(
		[&DestAttributesSet, ElementID](const FName AttributeName, auto AttributeArrayRef)
		{
			for(int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index)
			{
				DestAttributesSet.SetAttribute(ElementID, AttributeName, Index, AttributeArrayRef.Get(ElementID, Index));
			}
		}
	);
}

template <typename ElementIDType>
static void CopyAllAttributesToDifferentElement(TAttributesSet<ElementIDType>& DestAttributesSet, const TAttributesSet<ElementIDType>& SrcAttributesSet, const ElementIDType DstElementID, const ElementIDType SrcElementID)
{
	SrcAttributesSet.ForEach(
		[&DestAttributesSet, DstElementID, SrcElementID](const FName AttributeName, auto AttributeArrayRef)
		{
			for(int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index)
			{
				DestAttributesSet.SetAttribute(DstElementID, AttributeName, Index, AttributeArrayRef.Get(SrcElementID, Index));
			}
		}
	);
}

void USplitMeshCommand::RegisterUICommand(FBindingContext* BindingContext)
{
	UI_COMMAND_EXT(BindingContext, /* Out */ UICommandInfo, "SplitMesh", "Split", "Attempts to split the mesh into two meshes based on a selected plane.", EUserInterfaceActionType::Button, FInputChord(EKeys::C, true));
}

void USplitMeshCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	const TArray<UEditableMesh*>& SelectedMeshes = MeshEditorMode.GetSelectedEditableMeshes();
	const TArray<AActor*> SelectedActors = GetSelectedActors();
	check(SelectedMeshes.Num() == SelectedActors.Num());

	// @todo (mlentine): Need to deal with transformed planes appropriately
	const UEditableMesh* CuttingMesh = SelectedMeshes.Last();
	FTransform PlaneTransform = SelectedActors.Last()->GetTransform();
	FVector PlaneCenter;
	FVector PlaneNormal;

	for(const FPolygonID PolygonID : CuttingMesh->GetMeshDescription()->Polygons().GetElementIDs())
	{
		PlaneCenter = PlaneTransform.TransformPosition(CuttingMesh->ComputePolygonCenter(PolygonID));
		PlaneNormal = PlaneTransform.TransformVector(CuttingMesh->ComputePolygonNormal(PolygonID));
		break;
	}

	FScopedTransaction Transaction(LOCTEXT("UndoSplitMesh", "Split Mesh"));

	MeshEditorMode.CommitSelectedMeshes();

	const int32 NumSelectedMeshes = SelectedMeshes.Num();
	for(int32 MeshIndex = 0; MeshIndex < NumSelectedMeshes - 1; ++MeshIndex)
	{
		FTransform WorldToLocal = SelectedActors[MeshIndex]->GetTransform().Inverse();
		FVector TransformedPlaneNormal = WorldToLocal.TransformVector(PlaneNormal);
		FVector TransformedPlaneCenter = WorldToLocal.TransformPosition(PlaneCenter);
		FPlane CuttingPlane = FPlane(TransformedPlaneNormal.X, TransformedPlaneNormal.Y, TransformedPlaneNormal.Z, FVector::DotProduct(TransformedPlaneNormal, TransformedPlaneCenter));

		UEditableMesh* Mesh = SelectedMeshes[MeshIndex];
		TVertexAttributesConstRef<FVector> VertexPositions = Mesh->GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

		Mesh->StartModification(EMeshModificationType::Final, EMeshTopologyChange::TopologyChange);

		// Triangulated polys
		TArray<FPolygonID> PolygonsToTriangulate;
		TArray<FPolygonID> TriangulatedPolygons;
		for(const FPolygonID PolygonId : Mesh->GetMeshDescription()->Polygons().GetElementIDs())
		{
			if(Mesh->GetPolygonPerimeterVertexCount(PolygonId) > 3)
			{
				PolygonsToTriangulate.Add(PolygonId);
			}
		}
		Mesh->TriangulatePolygons(PolygonsToTriangulate, TriangulatedPolygons);

		// Generate split polys
		TArray<FPolygonID> PolygonIds1;
		TArray<FPolygonID> PolygonIds2;
		TArray<FPolygonID> NewPolygonIds;
		TArray<FEdgeID> BoundaryIds;
		Mesh->SplitPolygonalMesh(CuttingPlane, PolygonIds1, PolygonIds2, BoundaryIds);

		// Triangulate split polys
		PolygonsToTriangulate.Reset(); 
		TriangulatedPolygons.Reset();
		for(int32 PolygonIdIndex = 0; PolygonIdIndex < PolygonIds1.Num(); ++PolygonIdIndex)
		{
			const FPolygonID& PolygonID = PolygonIds1[PolygonIdIndex];
			if(Mesh->GetPolygonPerimeterVertexCount(PolygonID) > 3)
			{
				PolygonsToTriangulate.Add(PolygonID);
				// Remove this element from the list
				PolygonIds1[PolygonIdIndex--] = PolygonIds1.Last();
				PolygonIds1.SetNum(PolygonIds1.Num() - 1);
			}
		}
		Mesh->TriangulatePolygons(PolygonsToTriangulate, TriangulatedPolygons);
		PolygonIds1.Append(TriangulatedPolygons);
		NewPolygonIds.Append(PolygonIds1);

		// Fill Hole
		TSet<FVertexID> SeenVerts;
		FVector Center(0, 0, 0);
		int32 Count = 0;
		for(const FEdgeID EdgeId : BoundaryIds)
		{
			FVertexID Vertex0 = Mesh->GetMeshDescription()->GetEdgeVertex(EdgeId, 0);
			FVertexID Vertex1 = Mesh->GetMeshDescription()->GetEdgeVertex(EdgeId, 1);
			if(!SeenVerts.Contains(Vertex0))
			{
				SeenVerts.Add(Vertex0);
				Center += VertexPositions[Vertex0];
				Count++;
			}
			if(!SeenVerts.Contains(Vertex1))
			{
				SeenVerts.Add(Vertex1);
				Center += VertexPositions[Vertex1];
				Count++;
			}
		}
		Center /= Count;
		FVertexID NewVert = Mesh->GetMeshDescription()->CreateVertex();
		Mesh->GetMeshDescription()->VertexAttributes().SetAttribute<FVector>(NewVert, MeshAttribute::Vertex::Position, 0, Center);
		FVertexInstanceID NewVertInstance = Mesh->GetMeshDescription()->CreateVertexInstance(NewVert);

		FPolygonGroupID GroupId = Mesh->GetMeshDescription()->CreatePolygonGroup();
		for(const FEdgeID EdgeId : BoundaryIds)
		{
			FVertexID Vertex0 = Mesh->GetMeshDescription()->GetEdgeVertex(EdgeId, 0);
			const TArray<FVertexInstanceID>& VertexInstances0 = Mesh->GetMeshDescription()->GetVertexVertexInstances(Vertex0);
			check(VertexInstances0.Num() > 0);

			FVertexID Vertex1 = Mesh->GetMeshDescription()->GetEdgeVertex(EdgeId, 1);
			const TArray<FVertexInstanceID>& VertexInstances1 = Mesh->GetMeshDescription()->GetVertexVertexInstances(Vertex1);
			check(VertexInstances1.Num() > 0);

			TArray<FVertexInstanceID> PolygonVertexInstances;
			PolygonVertexInstances.SetNum(3);
			if(FVector::DotProduct(TransformedPlaneNormal, FVector::CrossProduct(VertexPositions[Vertex1] - VertexPositions[Vertex0], Center - VertexPositions[Vertex1])) < 0)
			{
				PolygonVertexInstances[0] = VertexInstances0[0];
				PolygonVertexInstances[1] = VertexInstances1[0];
				PolygonVertexInstances[2] = NewVertInstance;
			}
			else
			{
				PolygonVertexInstances[0] = VertexInstances1[0];
				PolygonVertexInstances[1] = VertexInstances0[0];
				PolygonVertexInstances[2] = NewVertInstance;
			}
			TArray<FEdgeID> NewEdgeIDs;
			const auto NewPolygonID = Mesh->GetMeshDescription()->CreatePolygon(GroupId, PolygonVertexInstances, &NewEdgeIDs);
			check(NewEdgeIDs.Num() == 0);
			NewPolygonIds.Add(NewPolygonID);
		}

		// Create New Mesh
		FString NewMeshName = Mesh->GetName() + "_2";
		FString NewPackageName = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) + TEXT("/") + NewMeshName;
		NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);
		UPackage* NewPackage = CreatePackage(nullptr, *NewPackageName);
		UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(NewPackage, *NewMeshName, RF_Public);
		new (NewStaticMesh->SourceModels) FStaticMeshSourceModel();
		FMeshDescription* NewMeshDescription = NewStaticMesh->CreateMeshDescription(0);
		check(NewMeshDescription);

		// @todo (mlentine): Need to make sure all numbers are the same
		{
			const int32 NumTexCoordIndices = Mesh->GetMeshDescription()->VertexInstanceAttributes().GetAttributeIndexCount<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
			NewMeshDescription->VertexInstanceAttributes().SetAttributeIndexCount<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate, NumTexCoordIndices);
		}

		TSet<FVertexInstanceID> VertexInstanceSet;
		TSet<FVertexID> VertexSet;
		TSet<FEdgeID> EdgeSet;
		TSet<FPolygonGroupID> PolygonGroupSet;
		for(const FPolygonID PolygonId : NewPolygonIds)
		{
			const TArray<FVertexInstanceID>& VertexInstanceIds = Mesh->GetMeshDescription()->GetPolygonPerimeterVertexInstances(PolygonId);
			// @todo (mlentine): Remove this when we don't need duplicate vertex instance ids.
			TArray<FVertexInstanceID> NewVertexInstanceIds;
			for(const FVertexInstanceID& VertexInstanceId : VertexInstanceIds)
			{
				const FVertexID& VertexId = Mesh->GetMeshDescription()->GetVertexInstanceVertex(VertexInstanceId);
				if(!VertexSet.Contains(VertexId))
				{
					NewMeshDescription->CreateVertexWithID(VertexId);
					CopyAllAttributes(NewMeshDescription->VertexAttributes(), Mesh->GetMeshDescription()->VertexAttributes(), VertexId);
					VertexSet.Add(VertexId);
				}
				// @todo (mlentine): This is the ideal way but isn't possible as the mesh building assumes each polygon has a different vert instance id
				/*if (!VertexInstanceSet.Contains(VertexInstanceID))
				{
					NewMeshDescription->CreateVertexInstanceWithID(VertexInstanceID, VertexID);
					CopyAllAttributes(NewMeshDescription->VertexInstanceAttributes(), Mesh->GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceID);
					VertexInstanceSet.Add(VertexInstanceID);
				}*/
				FVertexInstanceID NewVertexInstanceID = NewMeshDescription->CreateVertexInstance(VertexId);
				NewVertexInstanceIds.Add(NewVertexInstanceID);
				CopyAllAttributesToDifferentElement(NewMeshDescription->VertexInstanceAttributes(), Mesh->GetMeshDescription()->VertexInstanceAttributes(), NewVertexInstanceID, VertexInstanceId);
			}
			TArray<FEdgeID> EdgeIds;
			Mesh->GetMeshDescription()->GetPolygonEdges(PolygonId, EdgeIds);
			for(const FEdgeID EdgeId : EdgeIds)
			{
				if(!EdgeSet.Contains(EdgeId))
				{
					NewMeshDescription->CreateEdgeWithID(EdgeId, Mesh->GetMeshDescription()->GetEdgeVertex(EdgeId, 0), Mesh->GetMeshDescription()->GetEdgeVertex(EdgeId, 1));
					CopyAllAttributes(NewMeshDescription->EdgeAttributes(), Mesh->GetMeshDescription()->EdgeAttributes(), EdgeId);
					EdgeSet.Add(EdgeId);
				}
			}
			const FPolygonGroupID PolygonGroupId = Mesh->GetMeshDescription()->GetPolygonPolygonGroup(PolygonId);
			if(!PolygonGroupSet.Contains(PolygonGroupId))
			{
				NewMeshDescription->CreatePolygonGroupWithID(PolygonGroupId);
				CopyAllAttributes(NewMeshDescription->PolygonGroupAttributes(), Mesh->GetMeshDescription()->PolygonGroupAttributes(), PolygonGroupId);
				PolygonGroupSet.Add(PolygonGroupId);
			}

			NewMeshDescription->CreatePolygonWithID(PolygonId, PolygonGroupId, NewVertexInstanceIds);
			CopyAllAttributes(NewMeshDescription->PolygonAttributes(), Mesh->GetMeshDescription()->PolygonAttributes(), PolygonId);
		}

		FElementIDRemappings Remappings;
		NewMeshDescription->Compact(Remappings);
		NewMeshDescription->TriangulateMesh();
		for(int32 PolygonIdIndex = 0; PolygonIdIndex <= GroupId.GetValue(); ++PolygonIdIndex)
		{
			NewStaticMesh->StaticMaterials.Add(UMaterial::GetDefaultMaterial(MD_Surface));
		}
		NewStaticMesh->CommitMeshDescription(0);
		NewStaticMesh->Build();
		NewStaticMesh->PostEditChange();
		AStaticMeshActor* NewMeshActor = Cast<AStaticMeshActor>(AddActor(GetSelectedLevel(), AStaticMeshActor::StaticClass()));
		NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(NewStaticMesh);

		// Delete Part of Old Mesh
		Mesh->DeletePolygons(PolygonIds1, true, true, true, true);

		Mesh->EndModification();

		MeshEditorMode.TrackUndo(Mesh, Mesh->MakeUndo());
	}
}

#undef LOCTEXT_NAMESPACE

