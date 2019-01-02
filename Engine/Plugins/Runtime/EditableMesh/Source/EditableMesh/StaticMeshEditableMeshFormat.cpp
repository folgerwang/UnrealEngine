// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditableMeshFormat.h"
#include "EditableMesh.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "EditableStaticMeshAdapter.h"

bool FStaticMeshEditableMeshFormat::HandlesComponentType(class UPrimitiveComponent& Component)
{
	return (Cast<const UStaticMeshComponent>(&Component) != nullptr);
}

bool FStaticMeshEditableMeshFormat::HandlesBones()
{
	return false;
}


void FStaticMeshEditableMeshFormat::FillMeshObjectPtr( UPrimitiveComponent& Component, FEditableMeshSubMeshAddress& SubMeshAddress )
{
	SubMeshAddress.MeshObjectPtr = nullptr;

	const UStaticMeshComponent* StaticMeshComponentPtr = Cast<const UStaticMeshComponent>( &Component );
	if( StaticMeshComponentPtr != nullptr )
	{
		const UStaticMeshComponent& StaticMeshComponent = *StaticMeshComponentPtr;

		UStaticMesh* ComponentStaticMesh = StaticMeshComponent.GetStaticMesh();
		if( ComponentStaticMesh != nullptr && ComponentStaticMesh->HasValidRenderData() )
		{
			SubMeshAddress.MeshObjectPtr = ComponentStaticMesh;
		}
	}
}


UEditableMesh* FStaticMeshEditableMeshFormat::MakeEditableMesh( UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& SubMeshAddress )
{
	// If the static mesh already has an attached UEditableStaticMeshAdapter, use that in preference to creating a new one
	const UStaticMeshComponent* StaticMeshComponentPtr = Cast<const UStaticMeshComponent>( &Component );
	if( StaticMeshComponentPtr != nullptr )
	{
		const UStaticMeshComponent& StaticMeshComponent = *StaticMeshComponentPtr;
		if( StaticMeshComponent.GetStaticMesh() != nullptr )
		{
			UEditableMesh* EditableMesh = Cast<UEditableMesh>( StaticMeshComponent.GetStaticMesh()->EditableMesh );
			if( EditableMesh )
			{
				EditableMesh->SetSubMeshAddress( SubMeshAddress );
				return EditableMesh;
			}
		}
	}

	UEditableMesh* EditableMesh = NewObject<UEditableMesh>();
	FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();
	UStaticMesh::RegisterMeshAttributes( *MeshDescription );

	// Register additional attributes required by EditableMesh
	MeshDescription->EdgeAttributes().RegisterAttribute<bool>( MeshAttribute::Edge::IsUVSeam, 1, false );
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>( MeshAttribute::Polygon::Normal, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient );
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>( MeshAttribute::Polygon::Tangent, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient );
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>( MeshAttribute::Polygon::Binormal, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient );
	MeshDescription->PolygonAttributes().RegisterAttribute<FVector>( MeshAttribute::Polygon::Center, 1, FVector::ZeroVector, EMeshAttributeFlags::Transient );
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<bool>( MeshAttribute::PolygonGroup::EnableCollision );
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<bool>( MeshAttribute::PolygonGroup::CastShadow );

	UEditableStaticMeshAdapter* EditableStaticMesh = NewObject<UEditableStaticMeshAdapter>( EditableMesh );
	EditableMesh->Adapters.Add( EditableStaticMesh );
	EditableMesh->PrimaryAdapter = EditableStaticMesh;

	EditableStaticMesh->InitEditableStaticMesh( EditableMesh, Component, SubMeshAddress );

	// Don't bother returning a new mesh if it has no geometry
	if( EditableMesh->GetVertexCount() == 0 )
	{
		EditableMesh->Adapters.Remove( EditableStaticMesh );
		EditableStaticMesh->MarkPendingKill();
		EditableMesh->MarkPendingKill();
		EditableMesh = nullptr;
	}

	return EditableMesh;
}

