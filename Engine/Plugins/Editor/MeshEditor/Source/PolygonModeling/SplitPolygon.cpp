// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SplitPolygon.h"
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


void USplitPolygonFromVertexCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "SplitPolygonFromVertex", "Split Polygon", "Splits a polygon by clicking on a selected vertex and dragging to create an edge along the surface of a neighboring polygon.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


void USplitPolygonFromEdgeCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "SplitPolygonFromEdge", "Split Polygon", "Splits a polygon by clicking on a selected edge and dragging to create an edge along the surface of a neigboring polygon.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


void USplitPolygonFromPolygonCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "SplitPolygonFromPolygon", "Split", "Splits a polygon by clicking on a selected polygon and dragging to create an edge along the surface of the polygon or it's neighbor.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


bool USplitPolygonCommand::TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	Component = nullptr;
	EditableMesh = nullptr;
	StartingEdgeID = FEdgeID::Invalid;
	StartingVertexID = FVertexID::Invalid;
	EdgeSplit = 0.0f;

	// Figure out what to split
	static TMap<class UEditableMesh*, TArray<FMeshElement>> SelectedMeshesAndEdges;
	MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ SelectedMeshesAndEdges );
	if( SelectedMeshesAndEdges.Num() == 0 )
	{
		// No edges were selected directly, so try selected polygons' edges
		MeshEditorMode.GetSelectedMeshesAndPolygonsPerimeterEdges( /* Out */ SelectedMeshesAndEdges );
	}

	if( SelectedMeshesAndEdges.Num() > 0 )
	{
		for( auto& MeshAndEdges : SelectedMeshesAndEdges )
		{
			UEditableMesh* EdgeEditableMesh = MeshAndEdges.Key;
			const TArray<FMeshElement>& EdgeElements = MeshAndEdges.Value;

			// Figure out where to split
			FEdgeID ClosestEdgeID = FEdgeID::Invalid;
			float Split = 0.0f;
			const bool bFoundSplit = MeshEditorMode.FindEdgeSplitUnderInteractor( ViewportInteractor, EdgeEditableMesh, EdgeElements, /* Out */ ClosestEdgeID, /* Out */ Split );

			if( bFoundSplit )
			{
				// OK, we have an edge position to start dragging from!
				Component = EdgeElements[ 0 ].Component.Get();
				EditableMesh = EdgeEditableMesh;
				StartingEdgeID = ClosestEdgeID;
				EdgeSplit = Split;

				// No need to search other meshes
				break;
			}
		}
	}
	else
	{
		static TMap<class UEditableMesh*, TArray<FMeshElement>> SelectedMeshesAndVertices;
		MeshEditorMode.GetSelectedMeshesAndVertices( /* Out */ SelectedMeshesAndVertices );

		for( auto& MeshAndVertices : SelectedMeshesAndVertices )
		{
			UEditableMesh* VertexEditableMesh = MeshAndVertices.Key;
			const TArray<FMeshElement>& VertexElements = MeshAndVertices.Value;

			for( const FMeshElement& VertexElement : VertexElements )
			{
				// OK, we have a vertex to start dragging from!
				Component = VertexElements[ 0 ].Component.Get();
				EditableMesh = VertexEditableMesh;
				StartingVertexID = FVertexID( VertexElement.ElementAddress.ElementID );
				break;
			}
		}
	}

	return EditableMesh != nullptr;
}


void USplitPolygonCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	if( EditableMesh != nullptr )
	{
		check( Component != nullptr );

 		MeshEditorMode.DeselectAllMeshElements();
 
 		static TArray<FMeshElement> MeshElementsToSelect;
 		MeshElementsToSelect.Reset();
 
		verify( !EditableMesh->AnyChangesToUndo() );

		// We'll always be trying to split one of the polygons that share the starting vertex.  But we need to figure
		// out which one of those polygons the user is hovering over (either the polygon itself, or one of it's edges
		// or vertices.)
		static TArray<FPolygonID> CandidatePolygonIDs;
		CandidatePolygonIDs.Reset();
		if( StartingEdgeID != FEdgeID::Invalid )
		{
			EditableMesh->GetEdgeConnectedPolygons( StartingEdgeID, /* Out */ CandidatePolygonIDs );
		}
		else if( ensure( StartingVertexID != FVertexID::Invalid ) )
		{
			EditableMesh->GetVertexConnectedPolygons( StartingVertexID, /* Out */ CandidatePolygonIDs );
		}

		FPolygonID PolygonToSplit = FPolygonID::Invalid;
		FVertexID ToVertexID = FVertexID::Invalid;


		{
			FVector LaserPointerStart, LaserPointerEnd;
			if( ViewportInteractor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
			{
				const TVertexAttributesRef<FVector> VertexPositions = EditableMesh->GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

				for( const FPolygonID CandidatePolygonID : CandidatePolygonIDs )
				{
					const FTransform ComponentToWorld = Component->GetComponentToWorld();

					FVector SplitStartLocation;
					if( StartingVertexID != FVertexID::Invalid )
					{
						SplitStartLocation = ComponentToWorld.TransformPosition( VertexPositions[ StartingVertexID ] );
					}
					else if( ensure( StartingEdgeID != FEdgeID::Invalid ) )
					{
						FVertexID EdgeVertex0, EdgeVertex1;
						EditableMesh->GetEdgeVertices( StartingEdgeID, /* Out */ EdgeVertex0, /* Out */ EdgeVertex1 );

						const FVector EdgeVertex0Location = ComponentToWorld.TransformPosition( VertexPositions[ EdgeVertex0 ] );
						const FVector EdgeVertex1Location = ComponentToWorld.TransformPosition( VertexPositions[ EdgeVertex1 ] ) ;

						SplitStartLocation = FMath::Lerp( EdgeVertex0Location, EdgeVertex1Location, EdgeSplit );
					}


					const FPlane PolygonPlane = EditableMesh->ComputePolygonPlane( CandidatePolygonID ).TransformBy( ComponentToWorld.ToMatrixWithScale() );
					const FVector LaserImpactOnPolygonPlane = FMath::LinePlaneIntersection( LaserPointerStart, LaserPointerEnd, PolygonPlane );

					// @todo mesheditor splitpolygon: Ideally this would be more "fuzzy", and allow the interactor to extend beyond the range of the polygon.  But it will make figuring out which polygon to split more tricky.
					// @todo mesheditor urgent: Can crash with "Colinear points in FMath::ComputeBaryCentric2D()"  Needs repro.
					FMeshTriangle Triangle;
					FVector TriangleVertexWeights;
					if( EditableMesh->ComputeBarycentricWeightForPointOnPolygon( CandidatePolygonID, ComponentToWorld.InverseTransformPosition( LaserImpactOnPolygonPlane ), /* Out */ Triangle, /* Out */ TriangleVertexWeights ) )
					{
						const FVector SplitDirection = ( LaserImpactOnPolygonPlane - SplitStartLocation ).GetSafeNormal();

						// Trace out within the polygon to figure out where the split should connect to
						float ClosestEdgeDistance = TNumericLimits<float>::Max();
						FEdgeID ClosestEdgeID = FEdgeID::Invalid;

						static TArray<FEdgeID> PolygonPerimeterEdgeIDs;
						EditableMesh->GetPolygonPerimeterEdges( CandidatePolygonID, /* Out */ PolygonPerimeterEdgeIDs );

						for( const FEdgeID TargetEdgeID : PolygonPerimeterEdgeIDs )
						{
							bool bIsDisqualified = false;

							if( StartingEdgeID != FEdgeID::Invalid )
							{
								if( StartingEdgeID == TargetEdgeID )
								{
									// The edge we dragged from is disqualified as a target
									bIsDisqualified = true;
								}
							}
							else if( ensure( StartingVertexID != FVertexID::Invalid ) )
							{
								static TArray<FEdgeID> AdjacentEdgeIDs;
								EditableMesh->GetVertexConnectedEdges( StartingVertexID, /* Out */ AdjacentEdgeIDs );
								if( AdjacentEdgeIDs.Contains( TargetEdgeID ) )
								{
									// Never split an edge that's directly connected to the starting vertex
									bIsDisqualified = true;
								}
							}

							FVertexID EdgeVertex0, EdgeVertex1;
							EditableMesh->GetEdgeVertices( TargetEdgeID, /* Out */ EdgeVertex0, /* Out */ EdgeVertex1 );
							const FVector EdgeVertex0Location = ComponentToWorld.TransformPosition( VertexPositions[ EdgeVertex0 ] );
							const FVector EdgeVertex1Location = ComponentToWorld.TransformPosition( VertexPositions[ EdgeVertex1 ] );

							if( !bIsDisqualified )
							{
								// Don't bother trying to split unless the laser impact point is reasonably close to the target edge.  Otherwise it just feels
								// bad, because our split direction will be unstable as the interactor moves along the polygon
								const float Distance = FMath::PointDistToSegment( LaserImpactOnPolygonPlane, EdgeVertex0Location, EdgeVertex1Location );
								if( Distance > 20.0f )	// @todo mesheditor tweak: Should be based on polygon area or edge size, and probably scale with distance like other fuzzy tests.  Also, consider making this a distance *from* the impact point instead.
								{
									bIsDisqualified = true;
								}
							}

							if( !bIsDisqualified )
							{
								// Don't allow connecting to edges that are either degenerate or colinear to the split direction
								const FVector EdgeDirection = ( EdgeVertex1Location - EdgeVertex0Location ).GetSafeNormal();
								if( FMath::IsNearlyZero( EdgeDirection.SizeSquared() ) || FMath::IsNearlyEqual( FMath::Abs( FVector::DotProduct( SplitDirection, EdgeDirection ) ), 1.0f ) )
								{
									bIsDisqualified = true;
								}

								if( !bIsDisqualified )
								{
									FVector ClosestPointOnSplitLine, ClosestPointOnEdge;
									FMath::SegmentDistToSegmentSafe(
										SplitStartLocation, SplitStartLocation + SplitDirection * 99999.0f,
										EdgeVertex0Location, EdgeVertex1Location,
										/* Out */ ClosestPointOnSplitLine,
										/* Out */ ClosestPointOnEdge );

									// Closest points should be the same if there was actually an intersection
									if( ClosestPointOnSplitLine.Equals( ClosestPointOnEdge ) )
									{
										const float DistanceToEdgeImpact = ( ClosestPointOnEdge - SplitStartLocation ).Size();
										if( DistanceToEdgeImpact < ClosestEdgeDistance )
										{
											ClosestEdgeDistance = DistanceToEdgeImpact;
											ClosestEdgeID = TargetEdgeID;
										}
									}
								}
							}
						}

						if( ClosestEdgeID != FEdgeID::Invalid )
						{
							FVertexID EdgeVertex0, EdgeVertex1;
							EditableMesh->GetEdgeVertices( ClosestEdgeID, /* Out */ EdgeVertex0, /* Out */ EdgeVertex1 );

							const FVector EdgeVertex0Location = ComponentToWorld.TransformPosition( VertexPositions[ EdgeVertex0 ] );
							const FVector EdgeVertex1Location = ComponentToWorld.TransformPosition( VertexPositions[ EdgeVertex1 ] );

							const FVector ImpactOnEdge = SplitStartLocation + SplitDirection * ClosestEdgeDistance;

							const float EdgeLength = ( EdgeVertex1Location - EdgeVertex0Location ).Size();
							const float ImpactProgressAlongEdge = ( ImpactOnEdge - EdgeVertex0Location ).Size() / EdgeLength;

							// If we're really close to one side or the other of an edge, try to connect with an existing vertex instead
							const float EdgeProgressVertexSnapThreshold = 0.075f;	// @todo mesheditor splitpolygon: Should be actual 'fuzzy' distance consistent with MeshEditorMode, in world/screen units, not percentage of edge progress
							const bool bIsCloseToEdgeVertex0 = ImpactProgressAlongEdge < EdgeProgressVertexSnapThreshold;
							const bool bIsCloseToEdgeVertex1 = ImpactProgressAlongEdge > 1.0f - EdgeProgressVertexSnapThreshold;
							if( bIsCloseToEdgeVertex0 || bIsCloseToEdgeVertex1 )
							{
								// Prefer connecting to a vertex
								const FVertexID TargetVertexID = bIsCloseToEdgeVertex0 ? EdgeVertex0 : EdgeVertex1;

								bool bIsDisqualified = false;

								if( StartingEdgeID != FEdgeID::Invalid )
								{
									FVertexID StartingEdgeVertex0, StartingEdgeVertex1;
									EditableMesh->GetEdgeVertices( StartingEdgeID, /* Out */ StartingEdgeVertex0, /* Out */ StartingEdgeVertex1 );

									if( TargetVertexID == StartingEdgeVertex0 || TargetVertexID == StartingEdgeVertex1 )
									{
										// We're dragging from an edge.  We never want to use that edge's vertices as targets.
										bIsDisqualified = true;
									}
								}
								else if( ensure( StartingVertexID != FVertexID::Invalid ) )
								{
									if( TargetVertexID == StartingVertexID )
									{
										// The vertex we dragged from is disqualified as a target
										bIsDisqualified = true;
									}
									else
									{
										// The vertices that share an edge with our starting vertex are disqualified, because we don't want
										// to create an edge that's colinear with an existing edge
										static TArray<FVertexID> AdjacentVertexIDs;
										EditableMesh->GetVertexAdjacentVertices( StartingVertexID, /* Out */ AdjacentVertexIDs );

										if( AdjacentVertexIDs.Contains( TargetVertexID ) )
										{
											bIsDisqualified = true;
										}
									}
								}

								if( !bIsDisqualified )
								{
									// Don't allow connecting to vertices that are colinear to the split direction
									const FVector VertexLocation = ComponentToWorld.TransformPosition( VertexPositions[ TargetVertexID ] );
									const FVector VertexDirection = ( VertexLocation - SplitStartLocation ).GetSafeNormal();
									if( FMath::IsNearlyZero( VertexDirection.SizeSquared() ) || FMath::IsNearlyEqual( FMath::Abs( FVector::DotProduct( SplitDirection, VertexDirection ) ), 1.0f ) )
									{
										bIsDisqualified = true;
									}

									if( !bIsDisqualified )
									{
										// Connect to this vertex!
										PolygonToSplit = CandidatePolygonID;
										ToVertexID = TargetVertexID;
									}
								}
							}

							// If a vertex wasn't eligible, go ahead and connect to the edge
							if( ToVertexID == FVertexID::Invalid )
							{
								// Split the edge to create a new vertex that we'll connect to
								PolygonToSplit = CandidatePolygonID;

								// Go ahead and split the target edge
								static TArray<FVertexID> NewVertexIDs;
								NewVertexIDs.Reset();

								static TArray<float> SplitEdgeSplitList;
								SplitEdgeSplitList.SetNumUninitialized( 1 );
								SplitEdgeSplitList[ 0 ] = ImpactProgressAlongEdge;

								EditableMesh->SplitEdge( ClosestEdgeID, SplitEdgeSplitList, /* Out */ NewVertexIDs );
								check( NewVertexIDs.Num() == 1 );

								// Great!  We now have a vertex to connect with
								ToVertexID = NewVertexIDs[ 0 ];
							}
						}
					}
				}
			}
		}


		if( PolygonToSplit != FPolygonID::Invalid )
		{
			check( ToVertexID != FVertexID::Invalid );

			FVertexID FromVertexID = FVertexID::Invalid;
			if( StartingEdgeID != FEdgeID::Invalid )
			{
				// First, split the edge
				static TArray<FVertexID> NewVertexIDs;
				NewVertexIDs.Reset();

				static TArray<float> SplitEdgeSplitList;
				SplitEdgeSplitList.SetNumUninitialized( 1 );
				SplitEdgeSplitList[ 0 ] = EdgeSplit;

				EditableMesh->SplitEdge( StartingEdgeID, SplitEdgeSplitList, /* Out */ NewVertexIDs );
				check( NewVertexIDs.Num() == 1 );

				FromVertexID = NewVertexIDs[ 0 ];
			}
			else if( ensure( StartingVertexID != FVertexID::Invalid ) )
			{
				FromVertexID = StartingVertexID;
			}
			check( FromVertexID != FVertexID::Invalid );

			static TArray< FPolygonToSplit > PolygonsToSplit;
			PolygonsToSplit.Reset();
			PolygonsToSplit.SetNum( 1, false );

			PolygonsToSplit[ 0 ].PolygonID = PolygonToSplit;
			FVertexPair& VertexPair = *new( PolygonsToSplit[ 0 ].VertexPairsToSplitAt ) FVertexPair();
			VertexPair.VertexID0 = FromVertexID;
			VertexPair.VertexID1 = ToVertexID;

			static TArray<FEdgeID> NewEdgeIDs;
			EditableMesh->SplitPolygons( PolygonsToSplit, /* Out */ NewEdgeIDs );

			// Select the new edges that were created
			{
				for( const FEdgeID NewEdgeID : NewEdgeIDs )
				{
					FMeshElement MeshElementToSelect;
					{
						MeshElementToSelect.Component = Component;
						MeshElementToSelect.ElementAddress.SubMeshAddress = EditableMesh->GetSubMeshAddress();
						MeshElementToSelect.ElementAddress.ElementType = EEditableMeshElementType::Edge;
						MeshElementToSelect.ElementAddress.ElementID = NewEdgeID;
					}
		
					MeshElementsToSelect.Add( MeshElementToSelect );
				}
			}
		}

 
		MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
 
 		MeshEditorMode.SelectMeshElements( MeshElementsToSelect );
 	}
}



#undef LOCTEXT_NAMESPACE

