// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EditableMeshFactory.h"
#include "Features/IModularFeatures.h"
#include "IEditableMeshFormat.h"


FEditableMeshSubMeshAddress UEditableMeshFactory::MakeSubmeshAddress( UPrimitiveComponent* PrimitiveComponent, const int32 LODIndex )
{
	check( PrimitiveComponent != nullptr );

	FEditableMeshSubMeshAddress SubMeshAddress;

	const int32 NumEditableMeshFormats = IModularFeatures::Get().GetModularFeatureImplementationCount( "EditableMeshFormat" );
	for( int32 EditableMeshFormatIndex = 0; EditableMeshFormatIndex < NumEditableMeshFormats; ++EditableMeshFormatIndex )
	{
		IEditableMeshFormat& EditableMeshFormat = *static_cast<IEditableMeshFormat*>( IModularFeatures::Get().GetModularFeatureImplementation( "EditableMeshFormat", EditableMeshFormatIndex ) );

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

	return SubMeshAddress;
}


FEditableMeshSubMeshAddress UEditableMeshFactory::MakeSubmeshAddress( UStaticMesh& StaticMesh, const int32 LODIndex )
{
	FEditableMeshSubMeshAddress SubMeshAddress;

	const int32 NumEditableMeshFormats = IModularFeatures::Get().GetModularFeatureImplementationCount("EditableMeshFormat");
	
	if(NumEditableMeshFormats > 0)
	{
		IEditableMeshFormat& EditableMeshFormat = *static_cast<IEditableMeshFormat*>(IModularFeatures::Get().GetModularFeatureImplementation("EditableMeshFormat", 0));

		SubMeshAddress = FEditableMeshSubMeshAddress();
		SubMeshAddress.MeshObjectPtr = &StaticMesh;	
		SubMeshAddress.EditableMeshFormat = &EditableMeshFormat;
		SubMeshAddress.LODIndex = LODIndex;
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


