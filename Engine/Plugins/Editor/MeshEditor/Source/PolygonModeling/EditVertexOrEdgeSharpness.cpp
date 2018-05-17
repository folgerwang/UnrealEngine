// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EditVertexOrEdgeSharpness.h"
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


namespace VertexOrEdgeSharpnessHelpers
{
	/** Where the active interactor's impact point was when the "edit sharpness" action started */
	FVector EditSharpnessStartLocation( FVector::ZeroVector );

	// @todo mesheditor extensibility: Get rid of all of the static stuff ideally and CDOs with state.  Have MeshEditorMode construct instances of commands.  Don't use TObjectIterator except at startup.

	/** Figures out how much we should change the sharpness amount by looking at the interactor aim delta */
	static float ComputeSharpnessChangeDelta( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
	{
		// @todo mesheditor subdiv: Hard coded tweakables; ideally should be sized in screen space
		const float DragScaleFactor = 5.0f;
		const float ProgressBarHeight = 1000.0f;

		// Figure out how much to either increase or decrease sharpness based on how far the user has dragged up or down in world space.
		float DragDeltaZ = 0.0f;

		FVector LaserStart, LaserEnd;
		const bool bLaserIsValid = ViewportInteractor->GetLaserPointer( /* Out */ LaserStart, /* Out */ LaserEnd );
		FSphere GrabberSphere;
		const bool bGrabberSphereIsValid = ViewportInteractor->GetGrabberSphere( /* Out */ GrabberSphere );
		if( bLaserIsValid || bGrabberSphereIsValid )
		{
			const FVector ProgressBarStart( EditSharpnessStartLocation + FVector( 0.0f, 0.0f, -ProgressBarHeight * 0.5f ) );
			const FVector ProgressBarEnd( EditSharpnessStartLocation + FVector( 0.0f, 0.0f, ProgressBarHeight * 0.5f ) );

			// First check to see if we're within grabber sphere range.  Otherwise, we'll use the laser.
			bool bIsInRange = false;
			FVector ClosestPointOnProgressBar;
			if( bGrabberSphereIsValid )
			{
				if( FMath::PointDistToSegment( GrabberSphere.Center, ProgressBarStart, ProgressBarEnd ) <= GrabberSphere.W )
				{
					bIsInRange = true;
					ClosestPointOnProgressBar = FMath::ClosestPointOnSegment(
						GrabberSphere.Center,
						ProgressBarStart, ProgressBarEnd );
				}
			}

			if( !bIsInRange && ensure( bLaserIsValid ) )
			{
				bIsInRange = true;
				FVector ClosestPointOnRay;
				FMath::SegmentDistToSegment(
					ProgressBarStart, ProgressBarEnd,
					LaserStart, LaserEnd,
					/* Out */ ClosestPointOnProgressBar,
					/* Out */ ClosestPointOnRay );
			}

			if( bIsInRange )
			{
				// Generate a drag value between -1.0 and 1.0 based on the interaction of the ray and the progress bar line segment.
				const float ProgressBarLength = ( ProgressBarEnd - ProgressBarStart ).Size();
				DragDeltaZ = -1.0f + 2.0f * ( ( ClosestPointOnProgressBar - ProgressBarStart ).Size() / ProgressBarLength );
			}
		}

		const float ScaledDragDelta = DragDeltaZ * DragScaleFactor;
		return ScaledDragDelta;
	}

}


void UEditVertexCornerSharpnessCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "EditVertexCornerSharpness", "Edit Corner Sharpness", "Change the subdivision vertex corner sharpness of a vertex by clicking and dragging up and down.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


bool UEditVertexCornerSharpnessCommand::TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	VertexOrEdgeSharpnessHelpers::EditSharpnessStartLocation = ViewportInteractor->GetHoverLocation();

	return true;
}


void UEditVertexCornerSharpnessCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesWithSelectedVertices;
	MeshEditorMode.GetSelectedMeshesAndVertices( /* Out */ MeshesWithSelectedVertices );
	if( MeshesWithSelectedVertices.Num() > 0 )
	{
		const float ScaledDragDelta = VertexOrEdgeSharpnessHelpers::ComputeSharpnessChangeDelta( MeshEditorMode, ViewportInteractor );

		for( auto& MeshAndSelectedVertices : MeshesWithSelectedVertices )
		{
			UEditableMesh* EditableMesh = MeshAndSelectedVertices.Key;
			const TArray<FMeshElement>& VertexElements = MeshAndSelectedVertices.Value;

			static TArray<FVertexID> VertexIDs;
			VertexIDs.Reset();

			static TArray<float> NewSharpnessValues;
			NewSharpnessValues.Reset();

			const TVertexAttributeArray<float>& VertexSharpnesses = EditableMesh->GetMeshDescription()->VertexAttributes().GetAttributes<float>( MeshAttribute::Vertex::CornerSharpness );

			for( const FMeshElement& VertexElement : VertexElements )
			{
				const FVertexID VertexID( VertexElement.ElementAddress.ElementID );

				const float CurrentSharpnessValue = VertexSharpnesses[ VertexID ];
				const float NewSharpnessValue = FMath::Clamp( CurrentSharpnessValue + ScaledDragDelta, 0.0f, 1.0f );

				VertexIDs.Add( VertexID );
				NewSharpnessValues.Add( NewSharpnessValue );
			}


			verify( !EditableMesh->AnyChangesToUndo() );

			EditableMesh->SetVerticesCornerSharpness( VertexIDs, NewSharpnessValues );

			MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
		}
	}
}


void UEditEdgeCreaseSharpnessCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "EditEdgeCreaseSharpness", "Edit Crease Sharpness", "Change the subdivision edge crase sharpness of an edge by clicking and dragging up and down.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


bool UEditEdgeCreaseSharpnessCommand::TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	VertexOrEdgeSharpnessHelpers::EditSharpnessStartLocation = ViewportInteractor->GetHoverLocation();

	return true;
}


void UEditEdgeCreaseSharpnessCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor )
{
	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesWithSelectedEdges;
	MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ MeshesWithSelectedEdges );
	if( MeshesWithSelectedEdges.Num() > 0 )
	{
		const float ScaledDragDelta = VertexOrEdgeSharpnessHelpers::ComputeSharpnessChangeDelta( MeshEditorMode, ViewportInteractor );

		for( auto& MeshAndSelectedEdges : MeshesWithSelectedEdges )
		{
			UEditableMesh* EditableMesh = MeshAndSelectedEdges.Key;
			const TArray<FMeshElement>& EdgeElements = MeshAndSelectedEdges.Value;

			const TEdgeAttributeArray<float>& EdgeSharpnesses = EditableMesh->GetMeshDescription()->EdgeAttributes().GetAttributes<float>( MeshAttribute::Edge::CreaseSharpness );

			static TArray<FEdgeID> EdgeIDs;
			EdgeIDs.Reset();

			static TArray<float> NewSharpnessValues;
			NewSharpnessValues.Reset();

			for( const FMeshElement& EdgeElement : EdgeElements )
			{
				const FEdgeID EdgeID( EdgeElement.ElementAddress.ElementID );

				const float CurrentSharpnessValue = EdgeSharpnesses[ EdgeID ];
				const float NewSharpnessValue = FMath::Clamp( CurrentSharpnessValue + ScaledDragDelta, 0.0f, 1.0f );

				EdgeIDs.Add( EdgeID );
				NewSharpnessValues.Add( NewSharpnessValue );
			}

			verify( !EditableMesh->AnyChangesToUndo() );

			EditableMesh->SetEdgesCreaseSharpness( EdgeIDs, NewSharpnessValues );

			MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
		}
	}
}


#undef LOCTEXT_NAMESPACE

