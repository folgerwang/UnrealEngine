// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UnifyNormals.h"
#include "EditableMesh.h"
#include "Framework/Commands/UICommandInfo.h"
#include "IMeshEditorModeEditingContract.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshEditorMode"

namespace UnifyNormalsCommandUtils
{
	/*
	 * Get the direction of a polygon's edge with respect to a given MeshEdge
	 * Returns 1 if the vertices order of the polygon matches the order of the MeshEdge's vertices
	 * Returns -1 if the order is reversed
	 * Returns 0 if the polygon doesn't contain the MeshEdge's vertices
	 */
	int8 GetPolygonEdgeDirection(const FMeshDescription* MeshDescription, const FMeshEdge& Edge, const FPolygonID& PolygonID)
	{
		if (!MeshDescription)
		{
			return 0;
		}

		int8 EdgeDirection = 0;

		// Iterate through the polygon vertices to find vertex 0 of the MeshEdge
		const TArray<FVertexInstanceID>& VertexInstances = MeshDescription->GetPolygonPerimeterVertexInstances(PolygonID);
		int32 NumVertices = VertexInstances.Num();
		for (int32 Index = 0; Index < NumVertices; ++Index)
		{
			const FMeshVertexInstance& VertexInstance = MeshDescription->GetVertexInstance(VertexInstances[Index]);
			if (VertexInstance.VertexID == Edge.VertexIDs[0])
			{
				// Vertex 1 is either the next vertex of the triangle => same direction
				if (MeshDescription->GetVertexInstance(VertexInstances[(Index + 1) % NumVertices]).VertexID == Edge.VertexIDs[1])
				{
					EdgeDirection = 1;
				}
				// Or the previous vertex of the triangle => opposite direction
				else if (MeshDescription->GetVertexInstance(VertexInstances[(Index + NumVertices - 1) % NumVertices]).VertexID == Edge.VertexIDs[1])
				{
					EdgeDirection = -1;
				}
				break;
			}
		}
		return EdgeDirection;
	}
}

void UUnifyNormalsCommand::RegisterUICommand(FBindingContext* BindingContext)
{
	UI_COMMAND_EXT(BindingContext, /* Out */ UICommandInfo, "UnifyNormals", "Unify Normals", "Unify normals of the neighbors of the currently selected polygons.", EUserInterfaceActionType::Button, FInputChord(EKeys::U, EModifierKey::Shift));
}

void UUnifyNormalsCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	static TMap< UEditableMesh*, TArray<FMeshElement> > MeshesAndPolygons;
	MeshEditorMode.GetSelectedMeshesAndPolygons(MeshesAndPolygons);

	if (MeshesAndPolygons.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("UndoUnifyNormals", "Unify Normals"));

	MeshEditorMode.CommitSelectedMeshes();

	// Refresh selection (committing may have created a new mesh instance)
	MeshEditorMode.GetSelectedMeshesAndPolygons(MeshesAndPolygons);

	for (const auto& MeshAndPolygons : MeshesAndPolygons)
	{
		UEditableMesh* EditableMesh = MeshAndPolygons.Key;
		FMeshDescription* MeshDescription = EditableMesh->GetMeshDescription();

		TSet<FPolygonID> PolygonsToCheck;		// set of polygons to check in this pass
		TSet<FPolygonID> PolygonsForNextPass;	// set of polygons to check in the next pass
		TSet<FPolygonID> CheckedPolygons;		// set of polygons already checked
		TSet<FPolygonID> SelectedPolygons;		// set of selected polygons
		TSet<FPolygonID> FlippedPolygons;		// set of polygons that are flipped with respect to the selected polygons
		TSet<FVertexID> BoundaryPolygonVertices;// set of vertices of the polygons at the boundary of the flipped polygons
		TArray<FVertexID> Vertices;

		// Check the neighbor polygons of the selected polygon to see if they are flipped with respect to it
		// They are flipped if the edge shared by the polygons have the same edge direction (because they have opposite winding)
		for (const FMeshElement& PolygonElement : MeshAndPolygons.Value)
		{
			const FPolygonID SelectedPolygonID(PolygonElement.ElementAddress.ElementID);

			bool InitialPolygon = true;
			SelectedPolygons.Add(SelectedPolygonID);
			PolygonsToCheck.Add(SelectedPolygonID);

			while (PolygonsToCheck.Num() > 0)
			{
				PolygonsForNextPass.Reset();

				for (const FPolygonID& PolygonID : PolygonsToCheck)
				{
					CheckedPolygons.Add(PolygonID);

					TArray<FEdgeID> PolygonEdges;
					MeshDescription->GetPolygonEdges(PolygonID, PolygonEdges);
					for (const FEdgeID& EdgeID : PolygonEdges)
					{
						// For each edge of the polygon, check its direction and compare it with the edge direction of its neighbor polygon
						const FMeshEdge& Edge = MeshDescription->GetEdge(EdgeID);
						int8 PolygonEdgeDirection = UnifyNormalsCommandUtils::GetPolygonEdgeDirection(MeshDescription, Edge, PolygonID);

						for (const FPolygonID& NeighborPolygonID : Edge.ConnectedPolygons)
						{
							if (!CheckedPolygons.Contains(NeighborPolygonID))
							{
								CheckedPolygons.Add(NeighborPolygonID);

								int8 ConnectedPolygonEdgeDirection = UnifyNormalsCommandUtils::GetPolygonEdgeDirection(MeshDescription, Edge, NeighborPolygonID);

								// For the initial polygon, its neighbor is considered flipped if the shared edge is in the same direction for both polygons
								// For non-initial polygon, ie. a polygon that's already determined to be flipped, a neighbor polygon is flipped (with respect to the initial polygon)
								// if their shared edge are in opposite direction
								if ((!InitialPolygon && (PolygonEdgeDirection != ConnectedPolygonEdgeDirection)) ||
									(InitialPolygon && (PolygonEdgeDirection == ConnectedPolygonEdgeDirection)))
								{
									PolygonsForNextPass.Add(NeighborPolygonID);
									FlippedPolygons.Add(NeighborPolygonID);
								}
								else
								{
									// Add the vertices of the polygon at the boundary
									Vertices.Reset();
									MeshDescription->GetPolygonPerimeterVertices(NeighborPolygonID, Vertices);
									BoundaryPolygonVertices.Append(Vertices);
								}
							}
						}
					}
				}
				InitialPolygon = false;
				PolygonsToCheck = PolygonsForNextPass;
			}
		}

		// Unselect the initially selected polygons
		FlippedPolygons = FlippedPolygons.Difference(SelectedPolygons);

		if (FlippedPolygons.Num() > 0)
		{
			// Get the set of vertices of all flipped polygons
			TSet<FVertexID> FlippedPolygonVertices;
			for (const FPolygonID& PolygonID : FlippedPolygons)
			{
				Vertices.Reset();
				MeshDescription->GetPolygonPerimeterVertices(PolygonID, Vertices);
				FlippedPolygonVertices.Append(Vertices);
			}

			// Get the vertices that are at the boundary of flipped and unflipped polygons
			BoundaryPolygonVertices = BoundaryPolygonVertices.Intersect(FlippedPolygonVertices);

			// The boundary polygons are those that share the boundary vertices
			TSet<FPolygonID> BoundaryPolygons;
			for (const FVertexID& VertexID : BoundaryPolygonVertices)
			{
				TArray<FPolygonID> Polygons;
				MeshDescription->GetVertexConnectedPolygons(VertexID, Polygons);
				BoundaryPolygons.Append(Polygons);
			}

			EditableMesh->StartModification(EMeshModificationType::Final, EMeshTopologyChange::TopologyChange);

			EditableMesh->FlipPolygons(FlippedPolygons.Array());

			// The selected and boundary polygons are not modified, but need to have their normals/tangents recomputed
			// Also, adding the flipped polygons will force their tangents to be recomputed instead of just being flipped
			EditableMesh->PolygonsPendingNewTangentBasis.Append(FlippedPolygons);
			EditableMesh->PolygonsPendingNewTangentBasis.Append(SelectedPolygons);
			EditableMesh->PolygonsPendingNewTangentBasis.Append(BoundaryPolygons);

			EditableMesh->EndModification();

			MeshEditorMode.TrackUndo(EditableMesh, EditableMesh->MakeUndo());
		}
	}
}

#undef LOCTEXT_NAMESPACE
