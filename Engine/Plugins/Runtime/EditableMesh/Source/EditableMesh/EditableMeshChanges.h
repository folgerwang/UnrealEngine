// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Change.h"
#include "EditableMeshTypes.h"


// @todo mesheditor: Comment these classes and enums!

struct FDeleteOrphanVerticesChangeInput
{
	/** The vertex IDs to get rid of */
	TArray<FVertexID> VertexIDsToDelete;

	/** Default constructor */
	FDeleteOrphanVerticesChangeInput()
		: VertexIDsToDelete()
	{
	}
};


class FDeleteOrphanVerticesChange : public FChange
{

public:

	/** Constructor */
	FDeleteOrphanVerticesChange( const FDeleteOrphanVerticesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FDeleteOrphanVerticesChange( FDeleteOrphanVerticesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;

private:
	
	/** The data we need to make this change */
	FDeleteOrphanVerticesChangeInput Input;

};


struct FDeleteVertexInstancesChangeInput
{
	/** The vertex instance IDs to delete */
	TArray<FVertexInstanceID> VertexInstanceIDsToDelete;

	/** Whether we should also delete any vertices if we delete their only instance */
	bool bDeleteOrphanedVertices;

	/** Default constructor */
	FDeleteVertexInstancesChangeInput()
		: VertexInstanceIDsToDelete(),
		  bDeleteOrphanedVertices( true )
	{
	}
};


class FDeleteVertexInstancesChange : public FChange
{

public:

	/** Constructor */
	FDeleteVertexInstancesChange( const FDeleteVertexInstancesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FDeleteVertexInstancesChange( FDeleteVertexInstancesChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;

private:

	/** The data we need to make this change */
	FDeleteVertexInstancesChangeInput Input;
};


struct FDeleteEdgesChangeInput
{
	/** The edge IDs to get rid of */
	TArray<FEdgeID> EdgeIDsToDelete;

	/** Whether we should also delete any vertices that are left orphaned after deleting this edge */
	bool bDeleteOrphanedVertices;

	/** Default constructor */
	FDeleteEdgesChangeInput()
		: EdgeIDsToDelete(),
		  bDeleteOrphanedVertices( true )
	{
	}
};


class FDeleteEdgesChange : public FChange
{

public:

	/** Constructor */
	FDeleteEdgesChange( const FDeleteEdgesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FDeleteEdgesChange( FDeleteEdgesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FDeleteEdgesChangeInput Input;

};


struct FCreateVerticesChangeInput
{
	/** Information about each vertex that will be created */
	TArray<FVertexToCreate> VerticesToCreate;

	/** Default constructor */
	FCreateVerticesChangeInput()
		: VerticesToCreate()
	{
	}
};


class FCreateVerticesChange : public FChange
{

public:

	/** Constructor */
	FCreateVerticesChange( const FCreateVerticesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FCreateVerticesChange( FCreateVerticesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:
	
	/** The data we need to make this change */
	FCreateVerticesChangeInput Input;

};


struct FCreateVertexInstancesChangeInput
{
	/** Information about each vertex instance that will be created */
	TArray<FVertexInstanceToCreate> VertexInstancesToCreate;

	/** Default constructor */
	FCreateVertexInstancesChangeInput()
		: VertexInstancesToCreate()
	{
	}
};


class FCreateVertexInstancesChange : public FChange
{

public:

	/** Constructor */
	FCreateVertexInstancesChange( const FCreateVertexInstancesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FCreateVertexInstancesChange( FCreateVertexInstancesChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FCreateVertexInstancesChangeInput Input;

};


struct FCreateEdgesChangeInput
{
	/** Information about each edge that will be created */
	TArray<FEdgeToCreate> EdgesToCreate;

	/** Default constructor */
	FCreateEdgesChangeInput()
		: EdgesToCreate()
	{
	}
};


class FCreateEdgesChange : public FChange
{

public:

	/** Constructor */
	FCreateEdgesChange( const FCreateEdgesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FCreateEdgesChange( FCreateEdgesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FCreateEdgesChangeInput Input;

};


struct FCreatePolygonsChangeInput
{
	/** Information about each polygon that will be created */
	TArray<FPolygonToCreate> PolygonsToCreate;

	/** Default constructor */
	FCreatePolygonsChangeInput()
		: PolygonsToCreate()
	{
	}
};


class FCreatePolygonsChange : public FChange
{

public:

	/** Constructor */
	FCreatePolygonsChange( const FCreatePolygonsChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FCreatePolygonsChange( FCreatePolygonsChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FCreatePolygonsChangeInput Input;

};


struct FDeletePolygonsChangeInput
{
	/** The polygons to get rid of */
	TArray<FPolygonID> PolygonIDsToDelete;

	/** Whether we should also delete any edges that are left orphaned after deleting this polygon */
	bool bDeleteOrphanedEdges;

	/** Whether we should also delete any vertices that are left orphaned after deleting this polygon */
	bool bDeleteOrphanedVertices;

	/** Whether we should also delete any vertex instances that are left orphaned after deleting this polygon */
	bool bDeleteOrphanedVertexInstances;

	/** Whether we should also delete any sections that are left empty after deleting this polygon */
	bool bDeleteEmptySections;

	/** Default constructor */
	FDeletePolygonsChangeInput()
		: PolygonIDsToDelete(),
		  bDeleteOrphanedEdges( true ),
		  bDeleteOrphanedVertices( true ),
		  bDeleteOrphanedVertexInstances( true ),
		  bDeleteEmptySections( false )
	{
	}
};


class FDeletePolygonsChange : public FChange
{

public:

	/** Constructor */
	FDeletePolygonsChange( const FDeletePolygonsChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FDeletePolygonsChange( FDeletePolygonsChangeInput&& InitInput )
 		: Input( MoveTemp ( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FDeletePolygonsChangeInput Input;

};

struct FFlipPolygonsChangeInput
{
	/** The polygons to flip */
	TArray<FPolygonID> PolygonIDsToFlip;

	FFlipPolygonsChangeInput()
		: PolygonIDsToFlip()
	{
	}
};

class FFlipPolygonsChange : public FChange
{
public:
	FFlipPolygonsChange( const FFlipPolygonsChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;

private:

	/** The data we need to make this change */
	FFlipPolygonsChangeInput Input;
};

struct FSetVerticesAttributesChangeInput
{
	TArray<FAttributesForVertex> AttributesForVertices;
};


class FSetVerticesAttributesChange : public FChange
{

public:

	/** Constructor */
	FSetVerticesAttributesChange( const FSetVerticesAttributesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FSetVerticesAttributesChange( FSetVerticesAttributesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FSetVerticesAttributesChangeInput Input;

};


struct FSetVertexInstancesAttributesChangeInput
{
	TArray<FAttributesForVertexInstance> AttributesForVertexInstances;
};


class FSetVertexInstancesAttributesChange : public FChange
{

public:

	/** Constructor */
	FSetVertexInstancesAttributesChange( const FSetVertexInstancesAttributesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FSetVertexInstancesAttributesChange( FSetVertexInstancesAttributesChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FSetVertexInstancesAttributesChangeInput Input;

};


struct FSetEdgesAttributesChangeInput
{
	/** Which edges we'll be setting attributes for, along with the attribute data to set */
	TArray<FAttributesForEdge> AttributesForEdges;

	/** Default constructor */
	FSetEdgesAttributesChangeInput()
		: AttributesForEdges()
	{
	}
};


class FSetEdgesAttributesChange : public FChange
{

public:

	/** Constructor */
	FSetEdgesAttributesChange( const FSetEdgesAttributesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FSetEdgesAttributesChange( FSetEdgesAttributesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FSetEdgesAttributesChangeInput Input;

};


struct FSetPolygonsVertexAttributesChangeInput
{
	/** Which polygons we'll be setting vertex attributes for, along with the attribute data to set */
	TArray<FVertexAttributesForPolygon> VertexAttributesForPolygons;

	/** Default constructor */
	FSetPolygonsVertexAttributesChangeInput()
		: VertexAttributesForPolygons()
	{
	}
};


class FSetPolygonsVertexAttributesChange : public FChange
{

public:

	/** Constructor */
	FSetPolygonsVertexAttributesChange( const FSetPolygonsVertexAttributesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FSetPolygonsVertexAttributesChange( FSetPolygonsVertexAttributesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FSetPolygonsVertexAttributesChangeInput Input;

};


struct FChangePolygonsVertexInstancesChangeInput
{
	/** Which polygons we'll be setting vertex instances for, along with the vertex instances to set */
	TArray<FChangeVertexInstancesForPolygon> VertexInstancesForPolygons;

	/** Default constructor */
	FChangePolygonsVertexInstancesChangeInput()
		: VertexInstancesForPolygons()
	{
	}
};


class FChangePolygonsVertexInstancesChange : public FChange
{

public:

	/** Constructor */
	FChangePolygonsVertexInstancesChange( const FChangePolygonsVertexInstancesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FChangePolygonsVertexInstancesChange( FChangePolygonsVertexInstancesChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FChangePolygonsVertexInstancesChangeInput Input;

};


struct FSetEdgesVerticesChangeInput
{
	/** The edge to set new vertices for */
	TArray<FVerticesForEdge> VerticesForEdges;

	/** Default constructor */
	FSetEdgesVerticesChangeInput()
		: VerticesForEdges()
	{
	}
};


class FSetEdgesVerticesChange : public FChange
{

public:

	/** Constructor */
	FSetEdgesVerticesChange( const FSetEdgesVerticesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FSetEdgesVerticesChange( FSetEdgesVerticesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FSetEdgesVerticesChangeInput Input;

};


struct FInsertPolygonPerimeterVerticesChangeInput
{
	/** The polygon we'll be inserting vertices into */
	FPolygonID PolygonID;

	/** The first polygon perimeter vertex number to insert indices before */
	uint32 InsertBeforeVertexNumber;

	/** The vertices to insert, along with their polygon perimeter vertex attributes */
	TArray<FVertexAndAttributes> VerticesToInsert;

	
	/** Default constructor */
	FInsertPolygonPerimeterVerticesChangeInput()
		: PolygonID( FPolygonID::Invalid ),
		  InsertBeforeVertexNumber( 0 ),
		  VerticesToInsert()
	{
	}
};


class FInsertPolygonPerimeterVerticesChange : public FChange
{

public:

	/** Constructor */
	FInsertPolygonPerimeterVerticesChange( const FInsertPolygonPerimeterVerticesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

 	FInsertPolygonPerimeterVerticesChange( FInsertPolygonPerimeterVerticesChangeInput&& InitInput )
 		: Input( MoveTemp( InitInput ) )
 	{
 	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FInsertPolygonPerimeterVerticesChangeInput Input;

};


struct FRemovePolygonPerimeterVerticesChangeInput
{
	/** The polygon we'll be removing vertices from */
	FPolygonID PolygonID;

	/** The first polygon perimeter vertex number to remove */
	uint32 FirstVertexNumberToRemove;

	/** The number of vertices to remove */
	uint32 NumVerticesToRemove;

	/** Whether orphaned vertex instances should be deleted or not */
	bool bDeleteOrphanedVertexInstances;

	
	/** Default constructor */
	FRemovePolygonPerimeterVerticesChangeInput()
		: PolygonID( FPolygonID::Invalid ),
		  FirstVertexNumberToRemove( 0 ),
		  NumVerticesToRemove( 0 ),
		  bDeleteOrphanedVertexInstances( false )
	{
	}
};


class FRemovePolygonPerimeterVerticesChange : public FChange
{

public:

	/** Constructor */
	FRemovePolygonPerimeterVerticesChange( const FRemovePolygonPerimeterVerticesChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FRemovePolygonPerimeterVerticesChangeInput Input;

};


struct FStartOrEndModificationChangeInput
{
	/** True if we should start modifying the mesh, or false if we should end modifying the mesh.  This will be reversed every time an undo/redo happens */
	bool bStartModification;

	/** The type of modification we're doing here */
	EMeshModificationType MeshModificationType;
	
	/** Whether the mesh's topology can change during this modification */
	EMeshTopologyChange MeshTopologyChange;

	
	/** Default constructor */
	FStartOrEndModificationChangeInput()
		: bStartModification( true ),
		  MeshModificationType( EMeshModificationType::Final ),
		  MeshTopologyChange( EMeshTopologyChange::TopologyChange )
	{
	}
};


class FStartOrEndModificationChange : public FChange
{

public:

	/** Constructor */
	FStartOrEndModificationChange( const FStartOrEndModificationChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FStartOrEndModificationChangeInput Input;

};


struct FSetSubdivisionCountChangeInput
{
	/** The new number of subdivisions to use */
	int32 NewSubdivisionCount;

	/** Default constructor */
	FSetSubdivisionCountChangeInput()
		: NewSubdivisionCount( 0 )
	{
	}
};


class FSetSubdivisionCountChange : public FChange
{

public:

	/** Constructor */
	FSetSubdivisionCountChange( const FSetSubdivisionCountChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FSetSubdivisionCountChangeInput Input;

};


struct FCreatePolygonGroupsChangeInput
{
	/** Information about the polygon groups to create */
	TArray<FPolygonGroupToCreate> PolygonGroupsToCreate;

	/** Default constructor */
	FCreatePolygonGroupsChangeInput()
		: PolygonGroupsToCreate()
	{
	}
};


class FCreatePolygonGroupsChange : public FChange
{

public:

	/** Constructor */
	FCreatePolygonGroupsChange( const FCreatePolygonGroupsChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FCreatePolygonGroupsChange( FCreatePolygonGroupsChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FCreatePolygonGroupsChangeInput Input;
};


struct FDeletePolygonGroupsChangeInput
{
	/** The polygon group IDs to delete */
	TArray<FPolygonGroupID> PolygonGroupIDs;

	/** Default constructor */
	FDeletePolygonGroupsChangeInput()
		: PolygonGroupIDs()
	{
	}
};


class FDeletePolygonGroupsChange : public FChange
{

public:

	/** Constructor */
	FDeletePolygonGroupsChange( const FDeletePolygonGroupsChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FDeletePolygonGroupsChange( FDeletePolygonGroupsChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FDeletePolygonGroupsChangeInput Input;
};


struct FAssignPolygonsToPolygonGroupChangeInput
{
	/** The polygon group IDs to delete */
	TArray<FPolygonGroupForPolygon> PolygonGroupForPolygons;

	/** Whether we should delete a polygon group if it becomes orphaned */
	bool bDeleteOrphanedPolygonGroups;

	/** Default constructor */
	FAssignPolygonsToPolygonGroupChangeInput()
		: PolygonGroupForPolygons()
		, bDeleteOrphanedPolygonGroups( false )
	{
	}
};


class FAssignPolygonsToPolygonGroupChange : public FChange
{

public:

	/** Constructor */
	FAssignPolygonsToPolygonGroupChange( const FAssignPolygonsToPolygonGroupChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FAssignPolygonsToPolygonGroupChange( FAssignPolygonsToPolygonGroupChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override;
	virtual FString ToString() const override;


private:

	/** The data we need to make this change */
	FAssignPolygonsToPolygonGroupChangeInput Input;
};
