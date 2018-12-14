// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EditableMesh.h"
#include "EditableMeshTypes.h"
#include "GeometryHitTest.h"
#include "EditableMeshAdapter.generated.h"

UCLASS(Abstract)
class EDITABLEMESH_API UEditableMeshAdapter : public UObject
{
	GENERATED_BODY()

public:

	virtual void InitializeFromEditableMesh( const UEditableMesh* EditableMesh ) PURE_VIRTUAL(,);
	virtual void OnRebuildRenderMeshStart( const UEditableMesh* EditableMesh, const bool bInvalidateLighting ) PURE_VIRTUAL(,);
	virtual void OnRebuildRenderMesh( const UEditableMesh* EditableMesh ) PURE_VIRTUAL(,);
	virtual void OnRebuildRenderMeshFinish( const UEditableMesh* EditableMesh, const bool bRebuildBoundsAndCollision, const bool bIsPreviewRollback ) PURE_VIRTUAL(,);
	virtual void OnStartModification( const UEditableMesh* EditableMesh, const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange ) PURE_VIRTUAL(,);
	virtual void OnEndModification( const UEditableMesh* EditableMesh ) PURE_VIRTUAL(,);
	virtual void OnReindexElements( const UEditableMesh* EditableMesh, const FElementIDRemappings& Remappings ) PURE_VIRTUAL(,);
	virtual bool IsCommitted( const UEditableMesh* EditableMesh ) const PURE_VIRTUAL(,return false;);
	virtual bool IsCommittedAsInstance( const UEditableMesh* EditableMesh ) const PURE_VIRTUAL(,return false;);
	virtual void OnCommit( UEditableMesh* EditableMesh ) PURE_VIRTUAL(,);
	virtual UEditableMesh* OnCommitInstance( UEditableMesh* EditableMesh, UPrimitiveComponent* ComponentToInstanceTo ) PURE_VIRTUAL(,return nullptr;);
	virtual void OnRevert( UEditableMesh* EditableMesh ) PURE_VIRTUAL(,);
	virtual UEditableMesh* OnRevertInstance( UEditableMesh* EditableMesh ) PURE_VIRTUAL(,return nullptr;);
	virtual void OnPropagateInstanceChanges( UEditableMesh* EditableMesh ) PURE_VIRTUAL(,);

	virtual void OnDeleteVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs ) PURE_VIRTUAL(,);
	virtual void OnDeleteOrphanVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) PURE_VIRTUAL(,);
	virtual void OnCreateEmptyVertexRange( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) PURE_VIRTUAL(,);
	virtual void OnCreateVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) PURE_VIRTUAL(,);
	virtual void OnCreateVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs ) PURE_VIRTUAL(,);
	virtual void OnSetVertexAttribute( const UEditableMesh* EditableMesh, const FVertexID VertexID, const FMeshElementAttributeData& Attribute ) PURE_VIRTUAL(,);
	virtual void OnSetVertexInstanceAttribute( const UEditableMesh* EditableMesh, const FVertexInstanceID VertexInstanceID, const FMeshElementAttributeData& Attribute ) PURE_VIRTUAL(,);
	virtual void OnCreateEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) PURE_VIRTUAL(,);
	virtual void OnDeleteEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) PURE_VIRTUAL(,);
	virtual void OnSetEdgesVertices( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) PURE_VIRTUAL(,);
	virtual void OnSetEdgeAttribute( const UEditableMesh* EditableMesh, const FEdgeID EdgeID, const FMeshElementAttributeData& Attribute ) PURE_VIRTUAL(,);
	virtual void OnCreatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) PURE_VIRTUAL(,);
	virtual void OnDeletePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) PURE_VIRTUAL(,);
	virtual void OnChangePolygonVertexInstances( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) PURE_VIRTUAL(,);
	virtual void OnSetPolygonAttribute( const UEditableMesh* EditableMesh, const FPolygonID PolygonID, const FMeshElementAttributeData& Attribute ) PURE_VIRTUAL(,);
	virtual void OnCreatePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs ) PURE_VIRTUAL(,);
	virtual void OnDeletePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs ) PURE_VIRTUAL(,);
	virtual void OnSetPolygonGroupAttribute( const UEditableMesh* EditableMesh, const FPolygonGroupID PolygonGroupID, const FMeshElementAttributeData& Attribute ) PURE_VIRTUAL(,);
	virtual void OnAssignPolygonsToPolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupForPolygon>& PolygonGroupForPolygons ) PURE_VIRTUAL(,);
	virtual void OnRetriangulatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) PURE_VIRTUAL(,);

#if WITH_EDITOR
	virtual void GeometryHitTest(const FHitParamsIn& InParams, FHitParamsOut& OutParams) PURE_VIRTUAL(, );
#endif // WITH_EDITOR

};
