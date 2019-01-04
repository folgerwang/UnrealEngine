// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved. 

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

	if( EdgeInstanceCount > 0 )
	{
		const int32 NumVertices = EdgeInstanceCount * 4;
		const int32 NumIndices = EdgeInstanceCount * 6;
		const int32 NumTextureCoordinates = 1;

		VertexBuffers.PositionVertexBuffer.Init( NumVertices );
		VertexBuffers.StaticMeshVertexBuffer.Init( NumVertices, NumTextureCoordinates );
		VertexBuffers.ColorVertexBuffer.Init( NumVertices );
		IndexBuffer.Indices.SetNumUninitialized( NumIndices );

		// An edge instance is a concrete instance of an edge on a particular polygon. Each edge instance is expanded into two triangles which form a camera facing quad.
		// The quad has a thickness of zero: the material is responsible for giving it finite thickness, according to the edge and the camera direction.

		// The polygons for the wireframe mesh uses a specific vertex format:
		// The vertex normal is used to represent the edge direction (we use the normal rather than the tangent as it must not be modified by orthonormalization, when building the tangent basis).
		// The vertex tangent is used to represent the edge normal, which is used for optional backface culling.
		// The UV0 channel is set on a per-component basis, and contains various overrides per edge, to control the opacity and highlighting.

		int32 VertexBufferIndex = 0;
		int32 IndexBufferIndex = 0;

		for( const FWireframeEdgeInstance& EdgeInstance : EdgeInstances )
		{
			const FWireframeEdge& Edge = Edges[ EdgeInstance.EdgeID.GetValue() ];

			const FVector& PolygonNormal = Polygons[ EdgeInstance.PolygonID.GetValue() ].PolygonNormal;
			const FVector& StartVertex = Vertices[ Edge.StartVertex.GetValue() ].Position;
			const FVector& EndVertex = Vertices[ Edge.EndVertex.GetValue() ].Position;
			const FVector EdgeDirection = ( EndVertex - StartVertex ).GetSafeNormal();

			VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 0 ) = StartVertex;
			VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 1 ) = EndVertex;
			VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 2 ) = EndVertex;
			VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 3 ) = StartVertex;

			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 0, PolygonNormal, FVector::ZeroVector, -EdgeDirection );
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 1, PolygonNormal, FVector::ZeroVector, -EdgeDirection );
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 2, PolygonNormal, FVector::ZeroVector, EdgeDirection );
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 3, PolygonNormal, FVector::ZeroVector, EdgeDirection );

			VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 0 ) = Edge.Color;
			VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 1 ) = Edge.Color;
			VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 2 ) = Edge.Color;
			VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 3 ) = Edge.Color;

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
		BeginInitResource( &VertexBuffers.PositionVertexBuffer );
		BeginInitResource( &VertexBuffers.StaticMeshVertexBuffer );
		BeginInitResource( &VertexBuffers.ColorVertexBuffer );
		BeginInitResource( &IndexBuffer );
	}
}


void UWireframeMesh::ReleaseResources()
{
	if( IndexBuffer.Indices.Num() > 0 )
	{
		BeginReleaseResource( &VertexBuffers.PositionVertexBuffer );
		BeginReleaseResource( &VertexBuffers.StaticMeshVertexBuffer );
		BeginReleaseResource( &VertexBuffers.ColorVertexBuffer );
		BeginReleaseResource( &IndexBuffer );
	}
}


/** Scene proxy */
class FWireframeMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FWireframeMeshSceneProxy( UWireframeMeshComponent* Component )
		: FPrimitiveSceneProxy( Component )
		, VertexFactory( GetScene().GetFeatureLevel(), "FWireframeMeshSceneProxy" )
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		
	{
		check( Component->GetWireframeMesh() );

		const FStaticMeshVertexBuffers* VertexBuffers = &Component->GetWireframeMesh()->VertexBuffers;
		IndexBuffer = &Component->GetWireframeMesh()->IndexBuffer;

		// Init instance data
		NumVertices = Component->GetWireframeMesh()->GetNumEdgeInstances() * 4;
		const int32 NumTextureCoordinates = 1;
		InstanceVertexBuffer.Init( NumVertices, NumTextureCoordinates );

		for( int32 Index = 0; Index < NumVertices; ++Index )
		{
			InstanceVertexBuffer.SetVertexUV( Index, 0, FVector2D::ZeroVector );
		}

		for( const FEdgeID HiddenEdgeID : Component->HiddenEdgeIDs )
		{
			const TArray<int32>& EdgeInstanceIndices = Component->GetWireframeMesh()->GetEdgeInstanceIDs( HiddenEdgeID );
			for( const int32 EdgeInstanceIndex : EdgeInstanceIndices )
			{
				// Hide an edge by setting U of all of its instance vertices to 1.0
				InstanceVertexBuffer.SetVertexUV( EdgeInstanceIndex * 4 + 0, 0, FVector2D( 1.0f, 0.0f ) );
				InstanceVertexBuffer.SetVertexUV( EdgeInstanceIndex * 4 + 1, 0, FVector2D( 1.0f, 0.0f ) );
				InstanceVertexBuffer.SetVertexUV( EdgeInstanceIndex * 4 + 2, 0, FVector2D( 1.0f, 0.0f ) );
				InstanceVertexBuffer.SetVertexUV( EdgeInstanceIndex * 4 + 3, 0, FVector2D( 1.0f, 0.0f ) );
			}
		}

		ENQUEUE_RENDER_COMMAND( WireframeMeshVertexFactoryInit )(
			[ this, VertexBuffers ]( FRHICommandListImmediate& RHICmdList )
			{
				InstanceVertexBuffer.InitResource();

				FLocalVertexFactory::FDataType Data;
				VertexBuffers->PositionVertexBuffer.BindPositionVertexBuffer( &VertexFactory, Data );
				VertexBuffers->StaticMeshVertexBuffer.BindTangentVertexBuffer( &VertexFactory, Data );
				VertexBuffers->ColorVertexBuffer.BindColorVertexBuffer( &VertexFactory, Data );
				InstanceVertexBuffer.BindTexCoordVertexBuffer( &VertexFactory, Data );
				VertexFactory.SetData( Data );

				VertexFactory.InitResource();
			});

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

		FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();

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

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, UseEditorDepthTest());
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = IndexBuffer->Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = NumVertices - 1;
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
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance( Result );
		Result.bVelocityRelevance = IsMovable() && Result.bOpaqueRelevance && Result.bRenderInMainPass;
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
	const FDynamicMeshIndexBuffer32* IndexBuffer;
	FStaticMeshVertexBuffer InstanceVertexBuffer;
	FLocalVertexFactory VertexFactory;
	int32 NumVertices;

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
