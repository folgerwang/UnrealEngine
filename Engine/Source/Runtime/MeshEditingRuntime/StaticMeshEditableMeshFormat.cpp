// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditableMeshFormat.h"
#include "EditableMesh.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "EditableStaticMesh.h"


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
	// If the static mesh already has an attached UEditableStaticMesh, use that in preference to creating a new one
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

	UEditableStaticMesh* EditableStaticMesh = NewObject<UEditableStaticMesh>();

	EditableStaticMesh->InitEditableStaticMesh( Component, SubMeshAddress );

	// Don't bother returning a new mesh if it has no geometry
	if( EditableStaticMesh->GetVertexCount() == 0 )
	{
		EditableStaticMesh->MarkPendingKill();
		EditableStaticMesh = nullptr;
	}

	return EditableStaticMesh;
}

