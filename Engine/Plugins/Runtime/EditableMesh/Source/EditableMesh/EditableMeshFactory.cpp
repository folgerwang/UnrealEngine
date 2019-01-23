// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditableMeshFactory.h"
#include "Features/IModularFeatures.h"
#include "IEditableMeshFormat.h"
#include "EditableMesh.h"

FEditableMeshSubMeshAddress UEditableMeshFactory::MakeSubmeshAddress( UPrimitiveComponent* PrimitiveComponent, const int32 LODIndex )
{
	check( PrimitiveComponent != nullptr );

	FEditableMeshSubMeshAddress SubMeshAddress;

	const int32 NumEditableMeshFormats = IModularFeatures::Get().GetModularFeatureImplementationCount( "EditableMeshFormat" );
	for( int32 EditableMeshFormatIndex = 0; EditableMeshFormatIndex < NumEditableMeshFormats; ++EditableMeshFormatIndex )
	{
		IEditableMeshFormat& EditableMeshFormat = *static_cast<IEditableMeshFormat*>( IModularFeatures::Get().GetModularFeatureImplementation( "EditableMeshFormat", EditableMeshFormatIndex ) );

		if (EditableMeshFormat.HandlesComponentType(*PrimitiveComponent))
		{
			SubMeshAddress = FEditableMeshSubMeshAddress();
			SubMeshAddress.MeshObjectPtr = nullptr;	// This will be filled in below (FillMeshObjectPtr)
			SubMeshAddress.EditableMeshFormat = &EditableMeshFormat;
			SubMeshAddress.LODIndex = LODIndex;
			EditableMeshFormat.FillMeshObjectPtr( *PrimitiveComponent, SubMeshAddress );	// @todo mesheditor: This stuff is a bit clunky, would like to refactor it
			if( SubMeshAddress.MeshObjectPtr != nullptr )
			{
				break;
			}
			else
			{
				SubMeshAddress = FEditableMeshSubMeshAddress();
			}
		}
	}

	return SubMeshAddress;
}


UEditableMesh* UEditableMeshFactory::MakeEditableMesh( UPrimitiveComponent* PrimitiveComponent, const int32 LODIndex )
{
	check( PrimitiveComponent != nullptr );

	FEditableMeshSubMeshAddress SubMeshAddress = MakeSubmeshAddress( PrimitiveComponent, LODIndex );
	return MakeEditableMesh( PrimitiveComponent, SubMeshAddress );
}


UEditableMesh* UEditableMeshFactory::MakeEditableMesh( UPrimitiveComponent* PrimitiveComponent, const FEditableMeshSubMeshAddress& SubMeshAddress )
{
	check( PrimitiveComponent != nullptr );
	UEditableMesh* EditableMesh = nullptr;

	if( SubMeshAddress.EditableMeshFormat != nullptr &&
		SubMeshAddress.MeshObjectPtr != nullptr )
	{
		// @todo mesheditor perf: This is going to HITCH
		EditableMesh = SubMeshAddress.EditableMeshFormat->MakeEditableMesh( *PrimitiveComponent, SubMeshAddress );
	}

	return EditableMesh;
}

void UEditableMeshFactory::RefreshEditableMesh(UEditableMesh* EditableMesh, UPrimitiveComponent& PrimitiveComponent)
{
	check(EditableMesh != nullptr);

	const FEditableMeshSubMeshAddress& SubMeshAddress = EditableMesh->GetSubMeshAddress();

	if (SubMeshAddress.EditableMeshFormat != nullptr &&
		SubMeshAddress.MeshObjectPtr != nullptr)
	{
		// @todo mesheditor perf: This is going to HITCH
		SubMeshAddress.EditableMeshFormat->RefreshEditableMesh(EditableMesh, PrimitiveComponent);
	}
}


