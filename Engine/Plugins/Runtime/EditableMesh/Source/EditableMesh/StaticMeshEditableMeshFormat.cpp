// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditableMeshFormat.h"
#include "EditableMesh.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "EditableStaticMeshAdapter.h"


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
	MeshDescription->PolygonGroupAttributes().RegisterAttribute<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );

	UEditableStaticMeshAdapter* EditableStaticMesh = NewObject<UEditableStaticMeshAdapter>( EditableMesh );
	EditableMesh->Adapters.Add( EditableStaticMesh );

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

