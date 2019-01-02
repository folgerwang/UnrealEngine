// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditableMeshTypes.h"
#include "EditableMeshAdapter.h"
#include "StaticMeshResources.h"
#include "MeshEditorSubdividedStaticMeshAdapter.generated.h"


UCLASS(MinimalAPI)
class UMeshEditorSubdividedStaticMeshAdapter : public UEditableMeshAdapter
{
	GENERATED_BODY()

public:

	/** Default constructor that initializes good defaults for UMeshEditorSubdividedStaticMeshAdapter */
	UMeshEditorSubdividedStaticMeshAdapter();

	/** Creates a editable static mesh from the specified component and sub-mesh address */
	void Initialize( UEditableMesh* EditableMesh, class UWireframeMesh* WireframeMesh );

	virtual void InitializeFromEditableMesh( const UEditableMesh* EditableMesh ) override {}
	virtual void OnRebuildRenderMeshStart( const UEditableMesh* EditableMesh, const bool bInvalidateLighting ) override;
	virtual void OnRebuildRenderMesh( const UEditableMesh* EditableMesh ) override;
	virtual void OnRebuildRenderMeshFinish( const UEditableMesh* EditableMesh, const bool bRebuildBoundsAndCollision, const bool bIsPreviewRollback ) override;
	virtual void OnStartModification( const UEditableMesh* EditableMesh, const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange ) override;
	virtual void OnEndModification( const UEditableMesh* EditableMesh ) override;
	virtual void OnReindexElements( const UEditableMesh* EditableMesh, const FElementIDRemappings& Remappings ) override;
	virtual bool IsCommitted( const UEditableMesh* EditableMesh ) const override;
	virtual bool IsCommittedAsInstance( const UEditableMesh* EditableMesh ) const override;
	virtual void OnCommit( UEditableMesh* EditableMesh ) override;
	virtual UEditableMesh* OnCommitInstance( UEditableMesh* EditableMesh, UPrimitiveComponent* ComponentToInstanceTo ) override;
	virtual void OnRevert( UEditableMesh* EditableMesh ) override;
	virtual UEditableMesh* OnRevertInstance( UEditableMesh* EditableMesh ) override;
	virtual void OnPropagateInstanceChanges( UEditableMesh* EditableMesh ) override;

	virtual void OnDeleteVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs ) override;
	virtual void OnDeleteOrphanVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) override;
	virtual void OnCreateEmptyVertexRange( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) override;
	virtual void OnCreateVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) override;
	virtual void OnCreateVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs ) override;
	virtual void OnSetVertexAttribute( const UEditableMesh* EditableMesh, const FVertexID VertexID, const FMeshElementAttributeData& Attribute ) override {}
	virtual void OnSetVertexInstanceAttribute( const UEditableMesh* EditableMesh, const FVertexInstanceID VertexInstanceID, const FMeshElementAttributeData& Attribute ) override {}
	virtual void OnCreateEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) override;
	virtual void OnDeleteEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) override;
	virtual void OnSetEdgesVertices( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) override;
	virtual void OnSetEdgeAttribute( const UEditableMesh* EditableMesh, const FEdgeID EdgeID, const FMeshElementAttributeData& Attribute ) override {}
	virtual void OnCreatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;
	virtual void OnDeletePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;
	virtual void OnChangePolygonVertexInstances( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;
	virtual void OnSetPolygonAttribute( const UEditableMesh* EditableMesh, const FPolygonID PolygonID, const FMeshElementAttributeData& Attribute ) override {}
	virtual void OnCreatePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs ) override;
	virtual void OnDeletePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs ) override;
	virtual void OnSetPolygonGroupAttribute( const UEditableMesh* EditableMesh, const FPolygonGroupID PolygonGroupID, const FMeshElementAttributeData& Attribute ) override {}
	virtual void OnAssignPolygonsToPolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupForPolygon>& PolygonGroupForPolygons ) override {}
	virtual void OnRetriangulatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;


private:

	/** The wireframe mesh asset we're representing */
	UPROPERTY()
	UWireframeMesh* WireframeMesh;

	UPROPERTY()
	int32 StaticMeshLODIndex;
};
