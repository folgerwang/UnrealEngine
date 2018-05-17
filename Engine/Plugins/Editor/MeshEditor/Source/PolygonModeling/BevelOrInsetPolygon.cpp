// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BevelOrInsetPolygon.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshAttributes.h"
#include "MeshElement.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "ViewportInteractor.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"


namespace BevelOrInsetPolygonHelpers
{
	/** The selected polygon we clicked on to start the inset action */
	FMeshElement InsetUsingPolygonElement;

	enum class EBevelOrInset
	{
		Bevel,
		Inset
	};


	// Figures out how far we should inset on a polygon by figuring out the maximum distance from all of the polygon's perimeter
	// vertices to the center of the polygon, then subtracting the distance between the hovered impact point and polygon center.
	static void FindInsetAmount(
		IMeshEditorModeEditingContract& MeshEditorMode,
		UViewportInteractor* ViewportInteractor,
		const FPolygonID PolygonID,
		UPrimitiveComponent& Component,
		const UEditableMesh* EditableMesh,
		float& OutInsetFixedDistance,
		float& OutInsetProgressTowardCenter )
	{
		// @todo grabber: Glitches out when using grabber sphere near the corner of an inset polygon.
		check( ViewportInteractor != nullptr );

		OutInsetFixedDistance = 0.0f;
		OutInsetProgressTowardCenter = 0.0f;

		// @todo mesheditor extensibility: Need to decide whether to expose FMeshEditorInteractorData (currently we go straight to the ViewportInteractor for LaserStart, LasertEnd, bLaserIsValid)
		// @todo mesheditor grabber: Needs grabber sphere support
		FVector LaserStart, LaserEnd;
		const bool bLaserIsValid = ViewportInteractor->GetLaserPointer( /* Out */ LaserStart, /* Out */ LaserEnd );
		if( bLaserIsValid )
		{
			const FMatrix ComponentToWorldMatrix = Component.GetRenderMatrix();

			const FPlane PolygonPlane = EditableMesh->ComputePolygonPlane( PolygonID );
			const FVector PolygonCenter = EditableMesh->ComputePolygonCenter( PolygonID );

			const FVector ComponentSpaceRayStart = ComponentToWorldMatrix.InverseTransformPosition( LaserStart );
			const FVector ComponentSpaceRayEnd = ComponentToWorldMatrix.InverseTransformPosition( LaserEnd );

			FVector ComponentSpaceRayIntersectionWithPolygonPlane;
			if( FMath::SegmentPlaneIntersection( ComponentSpaceRayStart, ComponentSpaceRayEnd, PolygonPlane, /* Out */ ComponentSpaceRayIntersectionWithPolygonPlane ) )
			{
				// @todo mesheditor debug
				// DrawDebugSphere( This->GetWorld(), ComponentToWorldMatrix.TransformPosition( ComponentSpaceRayIntersectionWithPolygonPlane ), 1.5f, 32, FColor::Red, false, 0.0f );

				bool bFoundTriangle = false;
				float CenterWeight = 0.0f;
				FVector BestEdgeVertex0Position, BestEdgeVertex1Position;
				{
					const TVertexAttributeArray<FVector>& VertexPositions = EditableMesh->GetMeshDescription()->VertexAttributes().GetAttributes<FVector>( MeshAttribute::Vertex::Position );

					static TArray<FVertexID> PerimeterVertexIDs;
					EditableMesh->GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

					for( int32 VertexNumber = 0; VertexNumber < PerimeterVertexIDs.Num(); ++VertexNumber )
					{
						const int32 NextVertexNumber = ( VertexNumber + 1 ) % PerimeterVertexIDs.Num();


						const FVertexID EdgeVertex0 = PerimeterVertexIDs[ VertexNumber ];
						const FVertexID EdgeVertex1 = PerimeterVertexIDs[ NextVertexNumber ];

						const FVector EdgeVertex0Position = VertexPositions[ EdgeVertex0 ];
						const FVector EdgeVertex1Position = VertexPositions[ EdgeVertex1 ];

						BestEdgeVertex0Position = EdgeVertex0Position;
						BestEdgeVertex1Position = EdgeVertex1Position;

						const FVector VertexWeights = FMath::ComputeBaryCentric2D( ComponentSpaceRayIntersectionWithPolygonPlane, EdgeVertex0Position, EdgeVertex1Position, PolygonCenter );
						if( VertexWeights.X >= 0.0f && VertexWeights.Y >= 0.0f && VertexWeights.Z >= 0.0f )
						{
							CenterWeight = VertexWeights.Z;

							bFoundTriangle = true;
							break;
						}
					}
				}

				if( bFoundTriangle )
				{
					// @todo mesheditor debug
					// DrawDebugLine( This->GetWorld(), ComponentToWorldMatrix.TransformPosition( BestEdgeVertex0Position ), ComponentToWorldMatrix.TransformPosition( BestEdgeVertex1Position ), FColor::Yellow, false, 0.0f, 0, 2.0f );

					OutInsetProgressTowardCenter = CenterWeight;
				}
			}
		}
	}


	static bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
	{
		bool bCanStartDragging = false;

		// Figure out which polygon to inset
		InsetUsingPolygonElement = FMeshElement();
		{
			const FMeshElement& PolygonElement = MeshEditorMode.GetHoveredMeshElement( ViewportInteractor );
			if( PolygonElement.IsValidMeshElement() &&
				PolygonElement.ElementAddress.ElementType == EEditableMeshElementType::Polygon &&
				MeshEditorMode.IsMeshElementSelected( PolygonElement ) )
			{
				InsetUsingPolygonElement = PolygonElement;
				bCanStartDragging = true;
			}
		}

		return bCanStartDragging;
	}


	static void ApplyDuringDrag( const EBevelOrInset BevelOrInset, IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor )
	{
		static TMap< UEditableMesh*, TArray< FMeshElement > > MeshesWithPolygonsToInset;
		MeshEditorMode.GetSelectedMeshesAndPolygons( /* Out */ MeshesWithPolygonsToInset );

		if( MeshesWithPolygonsToInset.Num() > 0 )
		{
			if( InsetUsingPolygonElement.IsValidMeshElement() &&
				InsetUsingPolygonElement.ElementAddress.ElementType == EEditableMeshElementType::Polygon &&
				MeshEditorMode.IsMeshElementSelected( InsetUsingPolygonElement ) )
			{
				UPrimitiveComponent* InsetUsingComponent = InsetUsingPolygonElement.Component.Get();
				check( InsetUsingComponent != nullptr );

				MeshEditorMode.DeselectAllMeshElements();

				static TArray<FMeshElement> MeshElementsToSelect;
				MeshElementsToSelect.Reset();

				const UEditableMesh* InsetUsingEditableMesh = MeshEditorMode.FindEditableMesh( *InsetUsingComponent, InsetUsingPolygonElement.ElementAddress.SubMeshAddress );
				if( ensure( InsetUsingEditableMesh != nullptr ) )
				{
					const FPolygonID InsetUsingPolygonID( InsetUsingPolygonElement.ElementAddress.ElementID );

					// Figure out how far to inset the polygon
					float InsetFixedDistance = 0.0f;
					float InsetProgressTowardCenter = 0.0f;
					FindInsetAmount(
						MeshEditorMode,
						ViewportInteractor,
						InsetUsingPolygonID,
						*InsetUsingComponent,
						InsetUsingEditableMesh,
						/* Out */ InsetFixedDistance,
						/* Out */ InsetProgressTowardCenter );

					if( InsetFixedDistance > SMALL_NUMBER ||
						InsetProgressTowardCenter > SMALL_NUMBER )
					{
						for( auto& MeshAndPolygons : MeshesWithPolygonsToInset )
						{
							UEditableMesh* EditableMesh = MeshAndPolygons.Key;
							const TArray<FMeshElement>& PolygonsToInset = MeshAndPolygons.Value;

							UPrimitiveComponent* Component = PolygonsToInset[ 0 ].Component.Get();	// NOTE: All polygons in this array belong to the same mesh/component, so we just need the first element
							check( Component != nullptr );


							static TArray<FPolygonID> PolygonIDsToInset;
							PolygonIDsToInset.Reset();
							for( const FMeshElement& PolygonToInset : PolygonsToInset )
							{
								const FPolygonID PolygonID( PolygonToInset.ElementAddress.ElementID );
								PolygonIDsToInset.Add( PolygonID );
							}

							{
								verify( !EditableMesh->AnyChangesToUndo() );

								// Inset time!!
								static TArray<FPolygonID> NewCenterInsetPolygons;
								static TArray<FPolygonID> NewSideInsetPolygons;
								if( BevelOrInset == EBevelOrInset::Inset )
								{
									const EInsetPolygonsMode InsetPolygonsMode = EInsetPolygonsMode::All;	// @todo mesheditor inset: Make configurable?
																											// @todo mesheditor inset: Add options for Fixed distance (instead of Percentage distance, like now.)

									EditableMesh->InsetPolygons( PolygonIDsToInset, InsetFixedDistance, InsetProgressTowardCenter, InsetPolygonsMode, /* Out */ NewCenterInsetPolygons, /* Out */ NewSideInsetPolygons );
								}
								else if( ensure( BevelOrInset == EBevelOrInset::Bevel ) )
								{
									EditableMesh->BevelPolygons( PolygonIDsToInset, InsetFixedDistance, InsetProgressTowardCenter, /* Out */ NewCenterInsetPolygons, /* Out */ NewSideInsetPolygons );
								}

								// Make sure the new polygons are selected.  The old polygon was deleted and will become deselected automatically.
								if( NewCenterInsetPolygons.Num() > 0 )
								{
									for( int32 NewPolygonNumber = 0; NewPolygonNumber < NewCenterInsetPolygons.Num(); ++NewPolygonNumber )
									{
										const FPolygonID NewInsetCenterPolygon = NewCenterInsetPolygons[ NewPolygonNumber ];

										const FMeshElement& MeshElement = PolygonsToInset[ NewPolygonNumber ];

										FMeshElement PolygonMeshElement;
										{
											PolygonMeshElement.Component = MeshElement.Component;
											PolygonMeshElement.ElementAddress = MeshElement.ElementAddress;
											PolygonMeshElement.ElementAddress.ElementID = NewInsetCenterPolygon;
										}

										// Queue selection of this new element.  We don't want it to be part of the current action.
										MeshElementsToSelect.Add( PolygonMeshElement );
									}
								}
								else if( ensure( NewSideInsetPolygons.Num() > 0 ) )
								{
									for( int32 NewPolygonNumber = 0; NewPolygonNumber < NewSideInsetPolygons.Num(); ++NewPolygonNumber )
									{
										const FPolygonID NewInsetSidePolygon = NewSideInsetPolygons[ NewPolygonNumber ];

										const FMeshElement& MeshElement = PolygonsToInset[ NewPolygonNumber ];

										FMeshElement PolygonMeshElement;
										{
											PolygonMeshElement.Component = MeshElement.Component;
											PolygonMeshElement.ElementAddress = MeshElement.ElementAddress;
											PolygonMeshElement.ElementAddress.ElementID = NewInsetSidePolygon;
										}

										// Queue selection of this new element.  We don't want it to be part of the current action.
										MeshElementsToSelect.Add( PolygonMeshElement );
									}
								}
							}

							MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
						}
					}
				}

				MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
			}
		}
	}
}


void UBevelPolygonCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "BevelPolygon", "Bevel", "Bevels selected polygons as you click and drag.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


bool UBevelPolygonCommand::TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	return BevelOrInsetPolygonHelpers::TryStartingToDrag( MeshEditorMode, ViewportInteractor );
}


void UBevelPolygonCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	BevelOrInsetPolygonHelpers::ApplyDuringDrag( BevelOrInsetPolygonHelpers::EBevelOrInset::Bevel, MeshEditorMode, ViewportInteractor );
}


void UBevelPolygonCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Polygon )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "VRBevelPolygon", "Bevel" ),
			FText(),
			FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.PolyBevel" ),
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
}



void UInsetPolygonCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "InsetPolygon", "Inset", "Insets selected polygons as you click and drag on the interior of a polygon.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


bool UInsetPolygonCommand::TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	return BevelOrInsetPolygonHelpers::TryStartingToDrag( MeshEditorMode, ViewportInteractor );
}


void UInsetPolygonCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	BevelOrInsetPolygonHelpers::ApplyDuringDrag( BevelOrInsetPolygonHelpers::EBevelOrInset::Inset, MeshEditorMode, ViewportInteractor );
}


void UInsetPolygonCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Polygon )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "VRInsetPolygon", "Inset" ),
			FText(),
			FSlateIcon( TEMPHACK_StyleSetName, "MeshEditorMode.PolyInsert" ),
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
}


#undef LOCTEXT_NAMESPACE

