// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "Containers/SparseArray.h"
#include "RenderResource.h"
#include "PackedNormal.h"
#include "EditableMeshTypes.h"
#include "WireframeMeshComponent.generated.h"

class FPrimitiveSceneProxy;


/** Structure representing a vertex definition for the wireframe mesh */
struct FWireframeMeshVertex
{
	FWireframeMeshVertex() {}

	FWireframeMeshVertex( const FVector& InPosition, const FVector& InNormal, const FVector& InTangent, const FColor& InColor )
		: Position( InPosition ),
		  TangentX( InNormal ),
		  TangentZ( InTangent ),
		  Color( InColor )
	{
		TangentZ.Vector.W = 255;
	}

	FVector Position;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;
};


/** Vertex buffer for the shared mesh */
class FWireframeMeshVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FWireframeMeshVertex> Vertices;

	virtual void InitRHI() override;
};


/** Index buffer for the shared mesh */
class FWireframeMeshIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override;
};


/** Structure representing instance-specific extra data for a vertex, passed in the UV0 channel */
struct FWireframeMeshInstanceVertex
{
	FWireframeMeshInstanceVertex() {}

	FWireframeMeshInstanceVertex( const FVector2D& InUV )
		: UV( InUV )
	{}

	FVector2D UV;
};


/** Vertex buffer for a mesh instance */
class FWireframeMeshInstanceVertexBuffer : public FVertexBuffer
{
public:
	TArray<FWireframeMeshInstanceVertex> Vertices;

	virtual void InitRHI() override;
};


struct FWireframeVertex
{
	FVector Position;
};


struct FWireframePolygon
{
	FVector PolygonNormal;
};


struct FWireframeEdgeInstance
{
	FPolygonID PolygonID;
	FEdgeID EdgeID;
};


struct FWireframeEdge
{
	FVertexID StartVertex;
	FVertexID EndVertex;
	FColor Color;
	TArray<int32> EdgeInstances;
};


UCLASS()
class UWireframeMesh : public UObject
{
	GENERATED_BODY()

public:

	UWireframeMesh()
		: bBoundsDirty( true )
	{}

	virtual void BeginDestroy();

	void Reset();
	void AddVertex( const FVertexID VertexID );
	void SetVertexPosition( const FVertexID VertexID, const FVector& Position );
	void RemoveVertex( const FVertexID VertexID );
	void AddPolygon( const FPolygonID PolygonID );
	void SetPolygonNormal( const FPolygonID PolygonID, const FVector& Normal );
	void RemovePolygon( const FPolygonID PolygonID );
	void AddEdge( const FEdgeID EdgeID );
	void SetEdgeVertices( const FEdgeID EdgeID, const FVertexID Vertex0, const FVertexID Vertex1 );
	void SetEdgeColor( const FEdgeID EdgeID, const FColor Color );
	void RemoveEdge( const FEdgeID EdgeID );
	void AddEdgeInstance( const FEdgeID EdgeID, const FPolygonID PolygonID );
	void RemoveEdgeInstance( const FEdgeID EdgeID, const FPolygonID PolygonID );
	const TArray<int32>& GetEdgeInstanceIDs( const FEdgeID EdgeID ) const;
	int32 GetNumEdgeInstances() const { return EdgeInstances.Num(); }
	FBoxSphereBounds GetBounds() const;

	void InitResources();
	void ReleaseResources();

	// These arrays mirror the editable mesh elements
	TSparseArray<FWireframeVertex> Vertices;
	TSparseArray<FWireframePolygon> Polygons;
	TSparseArray<FWireframeEdge> Edges;

	// This is a packed array of edge instances with no holes.
	// An edge instance represents a unique quadrilaterial which forms part of the wireframe mesh.
	TArray<FWireframeEdgeInstance> EdgeInstances;

	FWireframeMeshVertexBuffer VertexBuffer;
	FWireframeMeshIndexBuffer IndexBuffer;

	mutable FBoxSphereBounds Bounds;
	mutable bool bBoundsDirty;
};


UCLASS()
class UWireframeMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	/**
	 * Default UObject constructor.
	 */
	UWireframeMeshComponent( const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get() );

	/** Gets the common wireframe mesh object used by this component */
	UWireframeMesh* GetWireframeMesh() const { return WireframeMesh; }

	/** Sets the wireframe mesh object used by this component */
	void SetWireframeMesh( UWireframeMesh* InWireframeMesh ) { WireframeMesh = InWireframeMesh; }

	/** Sets all edges as visible */
	void ShowAllEdges();

	/** Changes the visibility of the named edge */
	void SetEdgeVisibility( const FEdgeID EdgeID, const bool bEdgeVisible );

private:

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds( const FTransform& LocalToWorld ) const override;
	//~ Begin USceneComponent Interface.

	UPROPERTY()
	UWireframeMesh* WireframeMesh;

	/** Set of edge IDs not to be rendered for this instance */
	TSet<FEdgeID> HiddenEdgeIDs;

	friend class FWireframeMeshSceneProxy;
};
