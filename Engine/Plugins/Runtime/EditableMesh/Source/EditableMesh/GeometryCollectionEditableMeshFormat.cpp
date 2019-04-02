// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionEditableMeshFormat.h"
#include "EditableMesh.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "EditableGeometryCollectionAdapter.h"
#include "MeshAttributes.h"

bool FGeometryCollectionEditableMeshFormat::HandlesComponentType(class UPrimitiveComponent& Component)
{
	return (Cast<const UGeometryCollectionComponent>(&Component) != nullptr);
}

bool FGeometryCollectionEditableMeshFormat::HandlesBones()
{
	return true;
}

void FGeometryCollectionEditableMeshFormat::FillMeshObjectPtr( UPrimitiveComponent& Component, FEditableMeshSubMeshAddress& SubMeshAddress )
{
	SubMeshAddress.MeshObjectPtr = nullptr;

	UGeometryCollectionComponent* GeometryCollectionComponentPtr = Cast<UGeometryCollectionComponent>( &Component );
	if( GeometryCollectionComponentPtr != nullptr )
	{
		UGeometryCollectionComponent& GeometryCollectionComponent = *GeometryCollectionComponentPtr;

		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent.EditRestCollection(false);
		UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();
		if( GeometryCollection != nullptr)
		{
			SubMeshAddress.MeshObjectPtr = dynamic_cast<void *>(GeometryCollection);
		}
	}

	check(SubMeshAddress.MeshObjectPtr);
}


UEditableMesh* FGeometryCollectionEditableMeshFormat::MakeEditableMesh( UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& SubMeshAddress )
{
	// If the static mesh already has an attached UEditableGeometryCollectionAdapter, use that in preference to creating a new one
	const UGeometryCollectionComponent* GeometryCollectionComponentPtr = Cast<const UGeometryCollectionComponent>( &Component );
	if( GeometryCollectionComponentPtr != nullptr )
	{
		const UGeometryCollectionComponent& GeometryCollectionComponent = *GeometryCollectionComponentPtr;
		if( GeometryCollectionComponent.GetRestCollection() != nullptr )
		{
			UEditableMesh* EditableMesh = Cast<UEditableMesh>( GeometryCollectionComponent.GetRestCollection()->EditableMesh );
			if( EditableMesh )
			{
				EditableMesh->SetSubMeshAddress( SubMeshAddress );
				return EditableMesh;
			}
		}
	}

	UEditableMesh* EditableMesh = NewObject<UEditableMesh>();
	FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();
	EditableMesh->MeshDescription = MeshDescription;
	RegisterMeshAttributes( MeshDescription );

	// Register additional attributes required by EditableMesh
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );

	UEditableGeometryCollectionAdapter* EditableGeometryCollection = NewObject<UEditableGeometryCollectionAdapter>( EditableMesh );
	EditableMesh->Adapters.Add( EditableGeometryCollection);
	EditableMesh->PrimaryAdapter = EditableGeometryCollection;

	EditableGeometryCollection->InitEditableGeometryCollection( EditableMesh, Component, SubMeshAddress );

	// Don't bother returning a new mesh if it has no geometry
	if( EditableMesh->GetVertexCount() == 0 )
	{
		EditableMesh->Adapters.Remove( EditableGeometryCollection);
		EditableGeometryCollection->MarkPendingKill();
		EditableMesh->MarkPendingKill();
		EditableMesh = nullptr;
	}

	return EditableMesh;
}

void FGeometryCollectionEditableMeshFormat::RefreshEditableMesh(UEditableMesh* EditableMesh, UPrimitiveComponent& Component)
{
	// #todo: we might be able to speed this up by detecting changes in the geometry collection rather than doing complete reset
	EditableMesh->MeshDescription->Empty();
	//EditableMesh->MeshDescription->PolygonGroupAttributes().Initialize(0);
	//RegisterMeshAttributes(EditableMesh->MeshDescription);

	// located the UEditableGeometryCollectionAdapter
	check(EditableMesh->PrimaryAdapter);
	UEditableGeometryCollectionAdapter* EditableGeometryCollection = Cast<UEditableGeometryCollectionAdapter>(EditableMesh->PrimaryAdapter);
	check(EditableGeometryCollection);

	// this function
	EditableGeometryCollection->InitEditableGeometryCollection(EditableMesh, Component, EditableMesh->GetSubMeshAddress());

	// Don't bother returning a new mesh if it has no geometry
	if (EditableMesh->GetVertexCount() == 0)
	{
		EditableMesh->Adapters.Remove(EditableGeometryCollection);
		EditableGeometryCollection->MarkPendingKill();
		EditableMesh->MarkPendingKill();
		EditableMesh = nullptr;
	}

}

void FGeometryCollectionEditableMeshFormat::RegisterMeshAttributes(FMeshDescription* MeshDescription)
{
	check(MeshDescription);
	// Add basic vertex attributes
	MeshDescription->VertexAttributes().RegisterAttribute<FVector>(MeshAttribute::Vertex::Position, 1, FVector::ZeroVector, EMeshAttributeFlags::Lerpable);
	MeshDescription->VertexAttributes().RegisterAttribute<float>(MeshAttribute::Vertex::CornerSharpness, 1, 0.0f, EMeshAttributeFlags::Lerpable);

	// Add basic vertex instance attributes
	MeshDescription->VertexInstanceAttributes().RegisterAttribute<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate, 1, FVector2D::ZeroVector, EMeshAttributeFlags::Lerpable);
	MeshDescription->VertexInstanceAttributes().RegisterAttribute<FVector>(MeshAttribute::VertexInstance::Normal, 1, FVector::ZeroVector, EMeshAttributeFlags::AutoGenerated);
	MeshDescription->VertexInstanceAttributes().RegisterAttribute<FVector>(MeshAttribute::VertexInstance::Tangent, 1, FVector::ZeroVector, EMeshAttributeFlags::AutoGenerated);
	MeshDescription->VertexInstanceAttributes().RegisterAttribute<float>(MeshAttribute::VertexInstance::BinormalSign, 1, 0.0f, EMeshAttributeFlags::AutoGenerated);
	MeshDescription->VertexInstanceAttributes().RegisterAttribute<FVector4>(MeshAttribute::VertexInstance::Color, 1, FVector4(1.0f), EMeshAttributeFlags::Lerpable);

	// Add basic edge attributes
	MeshDescription->EdgeAttributes().RegisterAttribute<bool>(MeshAttribute::Edge::IsHard, 1, false);
	MeshDescription->EdgeAttributes().RegisterAttribute<bool>(MeshAttribute::Edge::IsUVSeam, 1, false);
	MeshDescription->EdgeAttributes().RegisterAttribute<float>(MeshAttribute::Edge::CreaseSharpness, 1, 0.0f, EMeshAttributeFlags::Lerpable);

	// Add basic polygon attributes
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Normal, 1, FVector::ZeroVector, EMeshAttributeFlags::AutoGenerated);
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Tangent, 1, FVector::ZeroVector, EMeshAttributeFlags::AutoGenerated);
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Binormal, 1, FVector::ZeroVector, EMeshAttributeFlags::AutoGenerated);
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>(MeshAttribute::Polygon::Center, 1, FVector::ZeroVector, EMeshAttributeFlags::AutoGenerated);

	// Add basic polygon group attributes
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName); //The unique key to match the mesh material slot
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<bool>(MeshAttribute::PolygonGroup::EnableCollision); //Deprecated
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<bool>(MeshAttribute::PolygonGroup::CastShadow); //Deprecated
}

