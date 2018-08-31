// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorSelectionModifiers.h"

#include "EditableMesh.h"
#include "IMeshEditorModeUIContract.h"
#include "MeshAttributes.h"
#include "MeshEditorStyle.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "MeshEditorSelectionModifiers"

namespace MeshEditorSelectionModifiers
{
	const TArray< UMeshEditorSelectionModifier* >& Get()
	{
		static UMeshEditorSelectionModifiersList* MeshEditorSelectionModifiersList = nullptr;
		if( MeshEditorSelectionModifiersList == nullptr )
		{
			MeshEditorSelectionModifiersList = NewObject< UMeshEditorSelectionModifiersList >();
			MeshEditorSelectionModifiersList->AddToRoot();

			MeshEditorSelectionModifiersList->HarvestSelectionModifiers();
		}

		return MeshEditorSelectionModifiersList->SelectionModifiers;
	}
}

void UMeshEditorSelectionModifiersList::HarvestSelectionModifiers()
{
	SelectionModifiers.Reset();
	for ( TObjectIterator< UMeshEditorSelectionModifier > SelectionModifierCDOIter( RF_NoFlags ); SelectionModifierCDOIter; ++SelectionModifierCDOIter )
	{
		UMeshEditorSelectionModifier* SelectionModifierCDO = *SelectionModifierCDOIter;
		if ( !( SelectionModifierCDO->GetClass()->GetClassFlags() & CLASS_Abstract ) )
		{
			SelectionModifiers.Add( NewObject< UMeshEditorSelectionModifier >( this, SelectionModifierCDO->GetClass() ) );
		}
	}
}

FMeshEditorSelectionModifiers::FMeshEditorSelectionModifiers()
	: TCommands< FMeshEditorSelectionModifiers >(
		"MeshEditorSelectionModifiers",
		LOCTEXT("MeshEditorSelectionModifiers", "Mesh Editor Selection Modifiers"),
		"MeshEditorCommon",
		FMeshEditorStyle::GetStyleSetName() )
{
}

void FMeshEditorSelectionModifiers::RegisterCommands()
{
	for ( UMeshEditorSelectionModifier* SelectionModifier : MeshEditorSelectionModifiers::Get() )
	{
		SelectionModifier->RegisterUICommand( this );
	}
}

void USelectSingleMeshElement::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "SingleElement", "Single", "", EUserInterfaceActionType::RadioButton, FInputChord() );
}

bool USelectPolygonsByGroup::ModifySelection( TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection )
{
	if ( InOutSelection.Num() == 0 )
	{
		return false;
	}

	TMap< UEditableMesh*, TArray< FMeshElement > > MeshElementsToSelect;
	TSet< FPolygonID > PolygonsToSelect;

	for ( const auto& MeshAndPolygons : InOutSelection )
	{
		UEditableMesh* EditableMesh = MeshAndPolygons.Key;
		MeshElementsToSelect.Add( EditableMesh );

		const TArray<FMeshElement>& Polygons = MeshAndPolygons.Value;

		for ( const FMeshElement& PolygonElement : Polygons )
		{
			const FPolygonID PolygonID( PolygonElement.ElementAddress.ElementID );

			if ( PolygonsToSelect.Contains( PolygonID ) )
			{
				// This polygon was already processed, we can skip it
				continue;
			}

			FPolygonGroupID SelectedPolygonGroupID = EditableMesh->GetGroupForPolygon( PolygonID );

			for ( int32 PolygonNumber = 0; PolygonNumber < EditableMesh->GetPolygonCountInGroup( SelectedPolygonGroupID ); ++PolygonNumber )
			{
				FPolygonID PolygonIDInGroup( EditableMesh->GetPolygonInGroup( SelectedPolygonGroupID, PolygonNumber ) );

				if ( !PolygonsToSelect.Contains( PolygonIDInGroup ) )
				{
					MeshElementsToSelect[ EditableMesh ].Emplace( PolygonElement.Component.Get(), EditableMesh->GetSubMeshAddress(), PolygonIDInGroup );
					PolygonsToSelect.Add( PolygonIDInGroup );
				}
			}
		}
	}

	InOutSelection = MeshElementsToSelect;

	return true;
}

void USelectPolygonsByGroup::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "PolygonsByGroup", "Material", "", EUserInterfaceActionType::RadioButton, FInputChord() );
}

bool USelectPolygonsByConnectivity::ModifySelection( TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection )
{
	if ( InOutSelection.Num() == 0 )
	{
		return false;
	}

	TMap< UEditableMesh*, TArray< FMeshElement > > MeshElementsToSelect;

	for ( const auto& MeshAndPolygons : InOutSelection )
	{
		UEditableMesh* EditableMesh = MeshAndPolygons.Key;
		MeshElementsToSelect.Add( EditableMesh );

		const TArray<FMeshElement>& Polygons = MeshAndPolygons.Value;

		TSet< FPolygonID > FilledPolygons;

		for( const FMeshElement& PolygonElement : Polygons )
		{
			FPolygonID PolygonID( PolygonElement.ElementAddress.ElementID );

			TSet< FPolygonID > ConnectedPolygons;

			if ( !FilledPolygons.Contains( PolygonID ) )
			{
				ConnectedPolygons.Add( PolygonID );
				FilledPolygons.Add( PolygonID );
			}

			FPolygonID ConnectedPolygonID = FPolygonID::Invalid;

			while ( ConnectedPolygons.Num() > 0 )
			{
				ConnectedPolygonID = *ConnectedPolygons.CreateIterator();

				TArray< FEdgeID > PolygonEdges;
				EditableMesh->GetPolygonPerimeterEdges( ConnectedPolygonID, PolygonEdges );

				for ( FEdgeID EdgeID : PolygonEdges )
				{
					TArray< FPolygonID > EdgeConnectedPolygons;
					EditableMesh->GetEdgeConnectedPolygons( EdgeID, EdgeConnectedPolygons );

					for ( FPolygonID EdgeConnectedPolygonID : EdgeConnectedPolygons )
					{
						if ( !FilledPolygons.Contains( EdgeConnectedPolygonID ) )
						{
							ConnectedPolygons.Add( EdgeConnectedPolygonID );
						}
					}

					FilledPolygons.Append( EdgeConnectedPolygons );
				}

				MeshElementsToSelect[ EditableMesh ].Emplace( PolygonElement.Component.Get(), EditableMesh->GetSubMeshAddress(), ConnectedPolygonID );
				ConnectedPolygons.Remove( ConnectedPolygonID );
			}
		}
	}

	InOutSelection = MeshElementsToSelect;

	return true;
}

void USelectPolygonsByConnectivity::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "PolygonsByConnectivity", "Element", "", EUserInterfaceActionType::RadioButton, FInputChord() );
}

bool USelectPolygonsBySmoothingGroup::ModifySelection(TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection)
{
	if (InOutSelection.Num() == 0)
	{
		return false;
	}

	TMap< UEditableMesh*, TArray< FMeshElement > > MeshElementsToSelect;

	for (const auto& MeshAndPolygons : InOutSelection)
	{
		UEditableMesh* EditableMesh = MeshAndPolygons.Key;
		FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();
		TArray< FMeshElement >& SelectedMeshElements = MeshElementsToSelect.Add(EditableMesh);

		const TArray<FMeshElement>& Polygons = MeshAndPolygons.Value;

		TSet< FPolygonID > CheckedPolygons;		// set of polygons that have already been checked

		TEdgeAttributesConstRef<bool> EdgeHardnesses = MeshDescription->EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
		for (const FMeshElement& PolygonElement : Polygons)
		{
			FPolygonID PolygonID(PolygonElement.ElementAddress.ElementID);

			TSet< FPolygonID > ConnectedPolygons;	// set of adjacent polygons that share the same smoothing group
			TSet< FPolygonID > PolygonsToCheck;		// set of polygons to check for same smoothing group

			ConnectedPolygons.Add(PolygonID);
			PolygonsToCheck.Add(PolygonID);

			while (PolygonsToCheck.Num() > 0)
			{
				const FPolygonID PolygonIDToCheck = *PolygonsToCheck.CreateConstIterator();

				// For each polygon, check its neighboring polygons if they share a soft edge (different smoothing groups are delimited by hard edges)
				PolygonsToCheck.Remove(PolygonIDToCheck);
				if (!CheckedPolygons.Contains(PolygonIDToCheck))
				{
					CheckedPolygons.Add(PolygonIDToCheck);

					TArray<FEdgeID> PolygonEdges;
					EditableMesh->GetPolygonPerimeterEdges(PolygonIDToCheck, PolygonEdges);

					for (const FEdgeID& EdgeID : PolygonEdges)
					{
						if (!EdgeHardnesses[EdgeID])
						{
							for (const FPolygonID ConnectedPolygonID : MeshDescription->GetEdgeConnectedPolygons(EdgeID))
							{
								if (!CheckedPolygons.Contains(ConnectedPolygonID))
								{
									PolygonsToCheck.Add(ConnectedPolygonID);
									ConnectedPolygons.Add(ConnectedPolygonID);
								}
							}
						}
					}
				}
			}

			for (const FPolygonID& ConnectedPolygonID : ConnectedPolygons)
			{
				SelectedMeshElements.Emplace(PolygonElement.Component.Get(), EditableMesh->GetSubMeshAddress(), ConnectedPolygonID);
			}
		}
	}

	InOutSelection = MeshElementsToSelect;

	return true;
}

void USelectPolygonsBySmoothingGroup::RegisterUICommand(FBindingContext* BindingContext)
{
	UI_COMMAND_EXT(BindingContext, /* Out */ UICommandInfo, "PolygonsBySmoothingGroup", "Smoothing Group", "", EUserInterfaceActionType::RadioButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
