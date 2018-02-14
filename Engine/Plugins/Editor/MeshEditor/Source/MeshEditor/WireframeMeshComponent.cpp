// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved. 

#include "WireframeMeshComponent.h"
#include "RenderingThread.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"


void UWireframeMesh::BeginDestroy()
{
	Super::BeginDestroy();

	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		ReleaseResources();
	}
}


void UWireframeMesh::Reset()
{
	Vertices.Reset();
	Polygons.Reset();
	Edges.Reset();
	EdgeInstances.Reset();
	bBoundsDirty = true;
}


void UWireframeMesh::AddVertex( const FVertexID VertexID )
{
	Vertices.Insert( VertexID.GetValue(), FWireframeVertex() );
}


void UWireframeMesh::SetVertexPosition( const FVertexID VertexID, const FVector& Position )
{
	Vertices[ VertexID.GetValue() ].Position = Position;
}


void UWireframeMesh::RemoveVertex( const FVertexID VertexID )
{
	Vertices.RemoveAt( VertexID.GetValue() );
	bBoundsDirty = true;
}


void UWireframeMesh::AddPolygon( const FPolygonID PolygonID )
{
	Polygons.Insert( PolygonID.GetValue(), FWireframePolygon() );
}


void UWireframeMesh::SetPolygonNormal( const FPolygonID PolygonID, const FVector& Normal )
{
	Polygons[ PolygonID.GetValue() ].PolygonNormal = Normal;
}


void UWireframeMesh::RemovePolygon( const FPolygonID PolygonID )
{
	Polygons.RemoveAt( PolygonID.GetValue() );
}


void UWireframeMesh::AddEdge( const FEdgeID EdgeID )
{
	Edges.Insert( EdgeID.GetValue(), FWireframeEdge() );
}


void UWireframeMesh::SetEdgeVertices( const FEdgeID EdgeID, const FVertexID Vertex0, const FVertexID Vertex1 )
{
	Edges[ EdgeID.GetValue() ].StartVertex = Vertex0;
	Edges[ EdgeID.GetValue() ].EndVertex = Vertex1;
}


void UWireframeMesh::SetEdgeColor( const FEdgeID EdgeID, const FColor Color )
{
	Edges[ EdgeID.GetValue() ].Color = Color;
}


void UWireframeMesh::RemoveEdge( const FEdgeID EdgeID )
{
	check( Edges[ EdgeID.GetValue() ].EdgeInstances.Num() == 0 );
	Edges.RemoveAt( EdgeID.GetValue() );
}


void UWireframeMesh::AddEdgeInstance( const FEdgeID EdgeID, const FPolygonID PolygonID )
{
	const int32 Index = EdgeInstances.Add( FWireframeEdgeInstance() );
	EdgeInstances[ Index ].PolygonID = PolygonID;
	EdgeInstances[ Index ].EdgeID = EdgeID;
	FWireframeEdge& Edge = Edges[ EdgeID.GetValue() ];
	check( !Edge.EdgeInstances.Contains( Index ) );
	Edge.EdgeInstances.Add( Index );
}


void UWireframeMesh::RemoveEdgeInstance( const FEdgeID EdgeID, const FPolygonID PolygonID )
{
	FWireframeEdge& Edge = Edges[ EdgeID.GetValue() ];

	// Find edge instance index corresponding to an instance of this edge with the given polygon ID
	const int32* EdgeInstanceIndexPtr = Edge.EdgeInstances.FindByPredicate( 
		[ this, PolygonID ]( const int32 Index ) { return this->EdgeInstances[ Index ].PolygonID == PolygonID; }
	);
	check( EdgeInstanceIndexPtr != nullptr );
	const int32 EdgeInstanceIndex = *EdgeInstanceIndexPtr;

	// Remove the instance from the edge array of instances
	const int32 NumberOfItemsRemoved = Edge.EdgeInstances.RemoveSwap( EdgeInstanceIndex );
	verify( NumberOfItemsRemoved == 1 );

	// If we are not removing the last element, we need to move the last element into the space vacated by the removed element
	// and patch up the reference to it in the Edge.
	const int32 IndexOfLastInstance = EdgeInstances.Num() - 1;
	if( EdgeInstanceIndex != IndexOfLastInstance )
	{
		// First, get the last instance in the array, and the edge it is instancing
		const FWireframeEdgeInstance& LastInstance = EdgeInstances[ IndexOfLastInstance ];
		FWireframeEdge& EdgeForLastInstance = Edges[ LastInstance.EdgeID.GetValue() ];

		// Now find the index of the last edge instance inside the array of instances for that edge.
		// Edge instances are owned by exactly one edge.
		const int32 EdgeLastInstanceIndex = EdgeForLastInstance.EdgeInstances.Find( IndexOfLastInstance );
		check( EdgeLastInstanceIndex != INDEX_NONE );

		// Fix up the index of the edge instance which has now changed
		check( !EdgeForLastInstance.EdgeInstances.Contains( EdgeInstanceIndex ) );
		EdgeForLastInstance.EdgeInstances[ EdgeLastInstanceIndex ] = EdgeInstanceIndex;

		// Copy the last edge instance into the element vacated by the removed edge instance
		EdgeInstances[ EdgeInstanceIndex ] = LastInstance;
	}

	// Reduce the array size by one
	EdgeInstances.SetNum( IndexOfLastInstance );
}


const TArray<int32>& UWireframeMesh::GetEdgeInstanceIDs( const FEdgeID EdgeID ) const
{
	return Edges[ EdgeID.GetValue() ].EdgeInstances;
}


FBoxSphereBounds UWireframeMesh::GetBounds() const
{
	if( bBoundsDirty )
	{
		FBox Box( ForceInit );
		for( const FWireframeVertex& Vertex : Vertices )
		{
			Box += Vertex.Position;
		}

		Bounds = FBoxSphereBounds( Box );
		bBoundsDirty = false;
	}

	return Bounds;
}


void UWireframeMesh::InitResources()
{
	const int32 EdgeInstanceCount = EdgeInstances.Num();

	VertexBuffer.Vertices.Reset();
	IndexBuffer.Indices.Reset();

	if( EdgeInstanceCount > 0 )
	{
		// An edge instance is a concrete instance of an edge on a particular polygon. Each edge instance is expanded into two triangles which form a camera facing quad.
		// The quad has a thickness of zero: the material is responsible for giving it finite thickness, according to the edge and the camera direction.

		// The polygons for the wireframe mesh uses a specific vertex format:
		// The vertex normal is used to represent the edge direction (we use the normal rather than the tangent as it must not be modified by orthonormalization, when building the tangent basis).
		// The vertex tangent is used to represent the edge normal, which is used for optional backface culling.
		// The UV0 channel is set on a per-component basis, and contains various overrides per edge, to control the opacity and highlighting.

		int32 VertexBufferIndex = VertexBuffer.Vertices.AddUninitialized( EdgeInstanceCount * 4 );
		int32 IndexBufferIndex = IndexBuffer.Indices.AddUninitialized( EdgeInstanceCount * 6 );

		for( const FWireframeEdgeInstance& EdgeInstance : EdgeInstances )
		{
			const FWireframeEdge& Edge = Edges[ EdgeInstance.EdgeID.GetValue() ];

			const FVector& PolygonNormal = Polygons[ EdgeInstance.PolygonID.GetValue() ].PolygonNormal;
			const FVector& StartVertex = Vertices[ Edge.StartVertex.GetValue() ].Position;
			const FVector& EndVertex = Vertices[ Edge.EndVertex.GetValue() ].Position;
			const FVector EdgeDirection = ( EndVertex - StartVertex ).GetSafeNormal();

			VertexBuffer.Vertices[ VertexBufferIndex + 0 ] = FWireframeMeshVertex( StartVertex, PolygonNormal, -EdgeDirection, Edge.Color );
			VertexBuffer.Vertices[ VertexBufferIndex + 1 ] = FWireframeMeshVertex( EndVertex, PolygonNormal, -EdgeDirection, Edge.Color );
			VertexBuffer.Vertices[ VertexBufferIndex + 2 ] = FWireframeMeshVertex( EndVertex, PolygonNormal, EdgeDirection, Edge.Color );
			VertexBuffer.Vertices[ VertexBufferIndex + 3 ] = FWireframeMeshVertex( StartVertex, PolygonNormal, EdgeDirection, Edge.Color );

			IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
			IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
			IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;
			IndexBuffer.Indices[ IndexBufferIndex + 3 ] = VertexBufferIndex + 2;
			IndexBuffer.Indices[ IndexBufferIndex + 4 ] = VertexBufferIndex + 3;
			IndexBuffer.Indices[ IndexBufferIndex + 5 ] = VertexBufferIndex + 0;

			VertexBufferIndex += 4;
			IndexBufferIndex += 6;
		}

		// Enqueue initialization of render resource
		BeginInitResource( &VertexBuffer );
		BeginInitResource( &IndexBuffer );
	}
}


void UWireframeMesh::ReleaseResources()
{
	if( IndexBuffer.Indices.Num() > 0 )
	{
		BeginReleaseResource( &VertexBuffer );
		BeginReleaseResource( &IndexBuffer );
	}
}


void FWireframeMeshVertexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* VertexBufferData = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer( Vertices.Num() * sizeof( FWireframeMeshVertex ), BUF_Static, CreateInfo, VertexBufferData );

	// Copy the vertex data into the vertex buffer.		
	FMemory::Memcpy( VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof( FWireframeMeshVertex ) );
	RHIUnlockVertexBuffer( VertexBufferRHI );
}


void FWireframeMeshIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* Buffer = nullptr;
	IndexBufferRHI = RHICreateAndLockIndexBuffer( sizeof( int32 ), Indices.Num() * sizeof( int32 ), BUF_Static, CreateInfo, Buffer );

	// Write the indices to the index buffer.		
	FMemory::Memcpy( Buffer, Indices.GetData(), Indices.Num() * sizeof( int32 ) );
	RHIUnlockIndexBuffer( IndexBufferRHI );
}


void FWireframeMeshInstanceVertexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* VertexBufferData = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer( Vertices.Num() * sizeof( FWireframeMeshInstanceVertex ), BUF_Static, CreateInfo, VertexBufferData );

	// Copy the vertex data into the vertex buffer.		
	FMemory::Memcpy( VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof( FWireframeMeshInstanceVertex ) );
	RHIUnlockVertexBuffer( VertexBufferRHI );
}


/** Vertex Factory */
class FWireframeMeshVertexFactory : public FLocalVertexFactory
{
public:

	FWireframeMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FWireframeMeshVertexFactory")
	{}

	/** Init function that should only be called on render thread. */
	void Init_RenderThread( const FWireframeMeshVertexBuffer* VertexBuffer, const FWireframeMeshInstanceVertexBuffer* InstanceVertexBuffer )
	{
		check( IsInRenderingThread() );

		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FWireframeMeshVertex, Position, VET_Float3 );
		NewData.TextureCoordinates.Add( FVertexStreamComponent( InstanceVertexBuffer, STRUCT_OFFSET( FWireframeMeshInstanceVertex, UV ), sizeof( FWireframeMeshInstanceVertex ), VET_Float2 ) );
		NewData.TangentBasisComponents[ 0 ] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FWireframeMeshVertex, TangentX, VET_PackedNormal );
		NewData.TangentBasisComponents[ 1 ] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FWireframeMeshVertex, TangentZ, VET_PackedNormal );
		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FWireframeMeshVertex, Color, VET_Color );

		SetData( NewData );
	}

	/** Initialization */
	void Init( const FWireframeMeshVertexBuffer* VertexBuffer, const FWireframeMeshInstanceVertexBuffer* InstanceVertexBuffer )
	{
		if( IsInRenderingThread() )
		{
			Init_RenderThread( VertexBuffer, InstanceVertexBuffer );
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				InitWireframeMeshVertexFactory,
				FWireframeMeshVertexFactory*, VertexFactory, this,
				const FWireframeMeshVertexBuffer*, VertexBuffer, VertexBuffer,
				const FWireframeMeshInstanceVertexBuffer*, InstanceVertexBuffer, InstanceVertexBuffer,
				{
					VertexFactory->Init_RenderThread( VertexBuffer, InstanceVertexBuffer );
				});
		}	
	}
};


/** Scene proxy */
class FWireframeMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FWireframeMeshSceneProxy( UWireframeMeshComponent* Component )
		: FPrimitiveSceneProxy( Component )
		, MaterialRelevance( Component->GetMaterialRelevance( GetScene().GetFeatureLevel() ) )
		, VertexFactory(GetScene().GetFeatureLevel())
		
	{
		check( Component->GetWireframeMesh() );

		VertexBuffer = &Component->GetWireframeMesh()->VertexBuffer;
		IndexBuffer = &Component->GetWireframeMesh()->IndexBuffer;

		// Init vertex factory
		VertexFactory.Init( &Component->GetWireframeMesh()->VertexBuffer, &InstanceVertexBuffer );
		BeginInitResource( &VertexFactory );

		// Init instance data
		InstanceVertexBuffer.Vertices.AddZeroed( Component->GetWireframeMesh()->GetNumEdgeInstances() * 4 );

		for( const FEdgeID HiddenEdgeID : Component->HiddenEdgeIDs )
		{
			const TArray<int32>& EdgeInstanceIndices = Component->GetWireframeMesh()->GetEdgeInstanceIDs( HiddenEdgeID );
			for( const int32 EdgeInstanceIndex : EdgeInstanceIndices )
			{
				// Hide an edge by setting U of all of its instance vertices to 1.0
				InstanceVertexBuffer.Vertices[ EdgeInstanceIndex * 4 + 0 ].UV = FVector2D( 1.0f, 0.0f );
				InstanceVertexBuffer.Vertices[ EdgeInstanceIndex * 4 + 1 ].UV = FVector2D( 1.0f, 0.0f );
				InstanceVertexBuffer.Vertices[ EdgeInstanceIndex * 4 + 2 ].UV = FVector2D( 1.0f, 0.0f );
				InstanceVertexBuffer.Vertices[ EdgeInstanceIndex * 4 + 3 ].UV = FVector2D( 1.0f, 0.0f );
			}
		}

		BeginInitResource( &InstanceVertexBuffer );

		// Grab material
		Material = Component->GetMaterial( 0 );
		if( Material == nullptr )
		{
			Material = UMaterial::GetDefaultMaterial( MD_Surface );
		}
	}

	virtual ~FWireframeMeshSceneProxy()
	{
		InstanceVertexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements( const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector ) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_WireframeMeshSceneProxy_GetDynamicMeshElements );

		FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy( IsSelected() );

		for( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
		{
			if( VisibilityMap & ( 1 << ViewIndex ) )
			{
				const FSceneView* View = Views[ ViewIndex ];
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[ 0 ];
				BatchElement.IndexBuffer = IndexBuffer;
				Mesh.bWireframe = false;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;
				BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate( GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest() );
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = IndexBuffer->Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = VertexBuffer->Vertices.Num() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh( ViewIndex, Mesh );
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance( const FSceneView* View ) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown( View );
		Result.bShadowRelevance = IsShadowCast( View );
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance( Result );
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint() const override { return sizeof( *this ) + GetAllocatedSize(); }

	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

	virtual SIZE_T GetTypeHash() const override
	{
		static SIZE_T UniquePointer;
		return reinterpret_cast<SIZE_T>(&UniquePointer);
	}
private:

	UMaterialInterface* Material;
	const FWireframeMeshVertexBuffer* VertexBuffer;
	const FWireframeMeshIndexBuffer* IndexBuffer;
	FWireframeMeshInstanceVertexBuffer InstanceVertexBuffer;
	FWireframeMeshVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
};


//////////////////////////////////////////////////////////////////////////

UWireframeMeshComponent::UWireframeMeshComponent( const FObjectInitializer& ObjectInitializer )
{
	CastShadow = false;
	bSelectable = false;
	PrimaryComponentTick.bCanEverTick = false;
	WireframeMesh = nullptr;

	SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );
}


void UWireframeMeshComponent::ShowAllEdges()
{
	HiddenEdgeIDs.Reset();
}


void UWireframeMeshComponent::SetEdgeVisibility( const FEdgeID EdgeID, const bool bEdgeVisible )
{
	if( bEdgeVisible )
	{
		HiddenEdgeIDs.Remove( EdgeID );
	}
	else
	{
		HiddenEdgeIDs.Add( EdgeID );
	}
}


FPrimitiveSceneProxy* UWireframeMeshComponent::CreateSceneProxy()
{
	if( WireframeMesh && WireframeMesh->EdgeInstances.Num() > 0 )
	{
		return new FWireframeMeshSceneProxy( this );
	}

	return nullptr;
}


int32 UWireframeMeshComponent::GetNumMaterials() const
{
	return 1;
}


FBoxSphereBounds UWireframeMeshComponent::CalcBounds( const FTransform& LocalToWorld ) const
{
	if( WireframeMesh )
	{
		return WireframeMesh->GetBounds().TransformBy( LocalToWorld );
	}

	return FBoxSphereBounds( LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f );
}
