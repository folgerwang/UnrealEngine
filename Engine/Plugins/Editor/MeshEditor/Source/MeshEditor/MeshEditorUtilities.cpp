// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorUtilities.h"

#include "EditableMesh.h"
#include "MeshAttributes.h"
#include "MeshElement.h"
#include "Materials/Material.h"

bool FMeshEditorUtilities::AssignMaterialToPolygons( UMaterialInterface* SelectedMaterial, UEditableMesh* EditableMesh, const TArray< FMeshElement >& PolygonElements )
{
	EditableMesh->StartModification( EMeshModificationType::Final, EMeshTopologyChange::TopologyChange );
	{
		const FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

		UPrimitiveComponent* Component = nullptr;
		for( const FMeshElement& PolygonElement : PolygonElements )
		{
			Component = PolygonElement.Component.Get();
			break;
		}
		check( Component != nullptr );

		// See if there's a polygon group using this material, and if not create one.
		// @todo mesheditor: This currently imposes the limitation that each polygon group has a unique material.
		// Eventually we will need to be able to specify PolygonGroup properties in the editor, and ask the user for
		// further details if there is more than one polygon group which matches the material.
		FPolygonGroupID PolygonGroupToAssign = FPolygonGroupID::Invalid;

		if ( SelectedMaterial != nullptr )
		{
			const TPolygonGroupAttributeArray<FName>& PolygonGroupMaterialAssetNames = MeshDescription->PolygonGroupAttributes().GetAttributes<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );

			for( const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs() )
			{
				const FName PolygonGroupMaterialName = PolygonGroupMaterialAssetNames[ PolygonGroupID ];
				if( SelectedMaterial->GetPathName() == PolygonGroupMaterialName.ToString() )
				{
					// We only expect to find one polygon group containing this material at the moment.
					// We need to provide a way of distinguishing different polygon groups with the same material.
					ensure( PolygonGroupToAssign == FPolygonGroupID::Invalid );
					PolygonGroupToAssign = PolygonGroupID;
				}
			}
		}
		else
		{
			// Use default material as asset to create new polygon group
			SelectedMaterial = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
		}


		// If we didn't find the material being used anywhere, create a new polygon group
		if( PolygonGroupToAssign == FPolygonGroupID::Invalid )
		{
			// Helper function which returns a unique FName for the material slot name, based on the material's asset name,
			// and adding a unique suffix if there are other polygon groups with the same material slot name.
			auto MakeUniqueSlotName = [ MeshDescription ]( FName Name ) -> FName
			{
				const TPolygonGroupAttributeArray<FName>& MaterialSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributes<FName>( MeshAttribute::PolygonGroup::MaterialAssetName );
				for( const FPolygonGroupID PolygonGroupID : MeshDescription->PolygonGroups().GetElementIDs() )
				{
					const FName ExistingName = MaterialSlotNames[ PolygonGroupID ];
					if( ExistingName.GetComparisonIndex() == Name.GetComparisonIndex() )
					{
						Name = FName( Name, FMath::Max( Name.GetNumber(), ExistingName.GetNumber() + 1 ) );
					}
				}
				return Name;
			};

			const FName UniqueSlotName = MakeUniqueSlotName( SelectedMaterial->GetFName() );

			static TArray<FPolygonGroupToCreate> PolygonGroupsToCreate;
			PolygonGroupsToCreate.Reset( 1 );
			PolygonGroupsToCreate.Emplace();

			FPolygonGroupToCreate& PolygonGroupToCreate = PolygonGroupsToCreate.Last();
			PolygonGroupToCreate.PolygonGroupAttributes.Attributes.Emplace( MeshAttribute::PolygonGroup::MaterialAssetName, 0, FMeshElementAttributeValue( FName( *SelectedMaterial->GetPathName() ) ) );
			PolygonGroupToCreate.PolygonGroupAttributes.Attributes.Emplace( MeshAttribute::PolygonGroup::ImportedMaterialSlotName, 0, FMeshElementAttributeValue( UniqueSlotName ));
			static TArray<FPolygonGroupID> NewPolygonGroupIDs;
			EditableMesh->CreatePolygonGroups( PolygonGroupsToCreate, NewPolygonGroupIDs );
			PolygonGroupToAssign = NewPolygonGroupIDs[ 0 ];
		}

		static TArray<FPolygonGroupForPolygon> PolygonsToAssign;
		PolygonsToAssign.Reset();

		for( const FMeshElement& PolygonElement : PolygonElements )
		{
			const FPolygonID PolygonID( PolygonElement.ElementAddress.ElementID );

			PolygonsToAssign.Emplace();
			FPolygonGroupForPolygon& PolygonGroupForPolygon = PolygonsToAssign.Last();
			PolygonGroupForPolygon.PolygonID = PolygonID;
			PolygonGroupForPolygon.PolygonGroupID = PolygonGroupToAssign;
		}

		const bool bDeleteOrphanedPolygonGroups = true;
		EditableMesh->AssignPolygonsToPolygonGroups( PolygonsToAssign, bDeleteOrphanedPolygonGroups );
	}
	EditableMesh->EndModification();

	return true;
}
