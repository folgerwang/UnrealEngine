// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved. 

#include "OverlayComponent.h"
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


struct FOverlayVertex
{
	FOverlayVertex() {}

	FOverlayVertex( const FVector& InPosition, const FVector2D& InUV, const FVector& InTangentX, const FVector& InTangentZ, const FColor& InColor )
		: Position( InPosition ),
		  UV( InUV ),
		  TangentX( InTangentX ),
		  TangentZ( InTangentZ ),
		  Color( InColor )
	{
		TangentZ.Vector.W = 255;
	}

	FVector Position;
	FVector2D UV;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;
};


class FOverlayVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FOverlayVertex> Vertices;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* VertexBufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer( Vertices.Num() * sizeof( FOverlayVertex ), BUF_Static, CreateInfo, VertexBufferData );

		// Copy the vertex data into the vertex buffer.		
		FMemory::Memcpy( VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof( FOverlayVertex ) );
		RHIUnlockVertexBuffer( VertexBufferRHI );
	}

};


class FOverlayIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer( sizeof( int32 ), Indices.Num() * sizeof( int32 ), BUF_Static, CreateInfo, Buffer );

		// Write the indices to the index buffer.		
		FMemory::Memcpy( Buffer, Indices.GetData(), Indices.Num() * sizeof( int32 ) );
		RHIUnlockIndexBuffer( IndexBufferRHI );
	}
};


class FOverlayVertexFactory : public FLocalVertexFactory
{
public:
	FOverlayVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FOverlayVertexFactory")
	{}

	/** Init function that should only be called on render thread. */
	void Init_RenderThread( const FOverlayVertexBuffer* VertexBuffer )
	{
		check( IsInRenderingThread() );

		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FOverlayVertex, Position, VET_Float3 );
		NewData.TextureCoordinates.Add(
			FVertexStreamComponent( VertexBuffer, STRUCT_OFFSET( FOverlayVertex, UV ), sizeof( FOverlayVertex ), VET_Float2 )
			);
		NewData.TangentBasisComponents[ 0 ] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FOverlayVertex, TangentX, VET_PackedNormal );
		NewData.TangentBasisComponents[ 1 ] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FOverlayVertex, TangentZ, VET_PackedNormal );
		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( VertexBuffer, FOverlayVertex, Color, VET_Color );

		SetData( NewData );
	}

	/** Initialization */
	void Init( const FOverlayVertexBuffer* VertexBuffer )
	{
		if ( IsInRenderingThread() )
		{
			Init_RenderThread( VertexBuffer );
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitOverlayVertexFactory,
				FOverlayVertexFactory*, VertexFactory, this,
				const FOverlayVertexBuffer*, VertexBuffer, VertexBuffer,
				{
					VertexFactory->Init_RenderThread( VertexBuffer );
				}
			);
		}	
	}
};


struct FMeshBatchData
{
	FMeshBatchData(ERHIFeatureLevel::Type FeatureLevel)
		: MaterialProxy(nullptr)
		, VertexBuffer()
		, IndexBuffer()
		, VertexFactory(FeatureLevel)
	{}

	FMaterialRenderProxy* MaterialProxy;
	FOverlayVertexBuffer VertexBuffer;
	FOverlayIndexBuffer IndexBuffer;
	FOverlayVertexFactory VertexFactory;
};


class FOverlaySceneProxy : public FPrimitiveSceneProxy
{
public:

	FOverlaySceneProxy( UOverlayComponent* Component )
		: FPrimitiveSceneProxy( Component ),
		  MaterialRelevance( Component->GetMaterialRelevance( GetScene().GetFeatureLevel() ) )
	{
		ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
		// @todo perf: Render state is marked dirty every time a new element is added, and a new scene proxy will be created.
		// Perhaps we can instead create the render buffers as BUF_Dynamic and amend them directly on the render thread.
		// However they will still require a reallocation if the number of elements grows, so this may not be a worthwhile optimization.

		// Initialize lines.
		// Lines are represented as two tris of zero thickness.
		// The vertex normals are used to hold the normalized line direction + a sign determining the direction in which the
		// material should thicken the polys.
		if( Component->Lines.Num() > 0 )
		{
			MeshBatchDatas.Emplace(FMeshBatchData(FeatureLevel));
			FMeshBatchData& MeshBatchData = MeshBatchDatas.Last();

			MeshBatchData.MaterialProxy = Component->GetMaterial( 0 )->GetRenderProxy( IsSelected() );
			MeshBatchData.VertexFactory.Init( &MeshBatchData.VertexBuffer );
			BeginInitResource( &MeshBatchData.VertexFactory );

			int32 VertexBufferIndex = MeshBatchData.VertexBuffer.Vertices.AddUninitialized( Component->Lines.Num() * 4 );
			int32 IndexBufferIndex = MeshBatchData.IndexBuffer.Indices.AddUninitialized( Component->Lines.Num() * 6 );

			for( const FOverlayLine& OverlayLine : Component->Lines )
			{
				const FVector LineDirection = ( OverlayLine.End - OverlayLine.Start ).GetSafeNormal();
				const FVector2D UV( OverlayLine.Thickness, 0.0f );

				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 0 ] = FOverlayVertex( OverlayLine.Start, UV, FVector::ZeroVector, -LineDirection, OverlayLine.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 1 ] = FOverlayVertex( OverlayLine.End, UV, FVector::ZeroVector, -LineDirection, OverlayLine.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 2 ] = FOverlayVertex( OverlayLine.End, UV, FVector::ZeroVector, LineDirection, OverlayLine.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 3 ] = FOverlayVertex( OverlayLine.Start, UV, FVector::ZeroVector, LineDirection, OverlayLine.Color );

				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 3 ] = VertexBufferIndex + 2;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 4 ] = VertexBufferIndex + 3;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 5 ] = VertexBufferIndex + 0;

				VertexBufferIndex += 4;
				IndexBufferIndex += 6;
			}

			BeginInitResource( &MeshBatchData.VertexBuffer );
			BeginInitResource( &MeshBatchData.IndexBuffer );
		}

		// Initialize points.
		// Points are represented as two tris, all of whose vertices are coincident.
		// The material then offsets them according to the signs of the vertex normals in a camera facing orientation.
		// Size of the point is given by U0.
		if( Component->Points.Num() > 0 )
		{
			MeshBatchDatas.Emplace(FMeshBatchData(FeatureLevel));
			FMeshBatchData& MeshBatchData = MeshBatchDatas.Last();

			MeshBatchData.MaterialProxy = Component->GetMaterial( 1 )->GetRenderProxy( IsSelected() );
			MeshBatchData.VertexFactory.Init( &MeshBatchData.VertexBuffer );
			BeginInitResource( &MeshBatchData.VertexFactory );

			int32 VertexBufferIndex = MeshBatchData.VertexBuffer.Vertices.AddUninitialized( Component->Points.Num() * 4 );
			int32 IndexBufferIndex = MeshBatchData.IndexBuffer.Indices.AddUninitialized( Component->Points.Num() * 6 );

			for( const FOverlayPoint& OverlayPoint : Component->Points )
			{
				const FVector2D UV( OverlayPoint.Size, 0.0f );

				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 0 ] = FOverlayVertex( OverlayPoint.Position, UV, FVector::ZeroVector, FVector( 1.0f, -1.0f, 0.0f ), OverlayPoint.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 1 ] = FOverlayVertex( OverlayPoint.Position, UV, FVector::ZeroVector, FVector( 1.0f, 1.0f, 0.0f ), OverlayPoint.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 2 ] = FOverlayVertex( OverlayPoint.Position, UV, FVector::ZeroVector, FVector( -1.0f, 1.0f, 0.0f ), OverlayPoint.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 3 ] = FOverlayVertex( OverlayPoint.Position, UV, FVector::ZeroVector, FVector( -1.0f, -1.0f, 0.0f ), OverlayPoint.Color );

				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 3 ] = VertexBufferIndex + 2;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 4 ] = VertexBufferIndex + 3;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 5 ] = VertexBufferIndex + 0;

				VertexBufferIndex += 4;
				IndexBufferIndex += 6;
			}

			BeginInitResource( &MeshBatchData.VertexBuffer );
			BeginInitResource( &MeshBatchData.IndexBuffer );
		}

		// Triangles
		int32 MaterialIndex = 2;
		for( const auto& MaterialTriangles : Component->TrianglesByMaterial )
		{
			MeshBatchDatas.Emplace(FMeshBatchData(FeatureLevel));
			FMeshBatchData& MeshBatchData = MeshBatchDatas.Last();

			MeshBatchData.MaterialProxy = Component->GetMaterial( MaterialIndex )->GetRenderProxy( IsSelected() );
			MeshBatchData.VertexFactory.Init( &MeshBatchData.VertexBuffer );
			BeginInitResource( &MeshBatchData.VertexFactory );

			int32 VertexBufferIndex = MeshBatchData.VertexBuffer.Vertices.AddUninitialized( MaterialTriangles.Num() * 3 );
			int32 IndexBufferIndex = MeshBatchData.IndexBuffer.Indices.AddUninitialized( MaterialTriangles.Num() * 3 );

			for( const FOverlayTriangle& Triangle : MaterialTriangles )
			{
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 0 ] = FOverlayVertex( Triangle.Vertex0.Position, Triangle.Vertex0.UV, FVector( 1, 0, 0 ), Triangle.Vertex0.Normal, Triangle.Vertex0.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 1 ] = FOverlayVertex( Triangle.Vertex1.Position, Triangle.Vertex1.UV, FVector( 1, 0, 0 ), Triangle.Vertex1.Normal, Triangle.Vertex1.Color );
				MeshBatchData.VertexBuffer.Vertices[ VertexBufferIndex + 2 ] = FOverlayVertex( Triangle.Vertex2.Position, Triangle.Vertex2.UV, FVector( 1, 0, 0 ), Triangle.Vertex2.Normal, Triangle.Vertex2.Color );

				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
				MeshBatchData.IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;

				VertexBufferIndex += 3;
				IndexBufferIndex += 3;
			}

			BeginInitResource( &MeshBatchData.VertexBuffer );
			BeginInitResource( &MeshBatchData.IndexBuffer );
		}
	}

	virtual ~FOverlaySceneProxy()
	{
		for( FMeshBatchData& MeshBatchData : MeshBatchDatas )
		{
			MeshBatchData.VertexBuffer.ReleaseResource();
			MeshBatchData.IndexBuffer.ReleaseResource();
			MeshBatchData.VertexFactory.ReleaseResource();
		}
	}

	virtual void GetDynamicMeshElements( const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector ) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_OverlaySceneProxy_GetDynamicMeshElements );

		for( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
		{
			if( VisibilityMap & ( 1 << ViewIndex ) )
			{
				for( const FMeshBatchData& MeshBatchData : MeshBatchDatas )
				{
					const FSceneView* View = Views[ ViewIndex ];
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[ 0 ];
					BatchElement.IndexBuffer = &MeshBatchData.IndexBuffer;
					Mesh.bWireframe = false;
					Mesh.VertexFactory = &MeshBatchData.VertexFactory;
					Mesh.MaterialRenderProxy = MeshBatchData.MaterialProxy;
					BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate( GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest() );
					BatchElement.FirstIndex = 0;
					BatchElement.NumPrimitives = MeshBatchData.IndexBuffer.Indices.Num() / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = MeshBatchData.VertexBuffer.Vertices.Num() - 1;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh( ViewIndex, Mesh );
				}
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

	TArray<FMeshBatchData> MeshBatchDatas;
	FMaterialRelevance MaterialRelevance;
};



UOverlayComponent::UOverlayComponent( const FObjectInitializer& ObjectInitializer )
{
	CastShadow = false;
	bSelectable = false;
	PrimaryComponentTick.bCanEverTick = false;
	bBoundsDirty = true;

	SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );
}


void UOverlayComponent::SetLineMaterial( UMaterialInterface* InLineMaterial )
{
	LineMaterial = InLineMaterial;
	SetMaterial( 0, InLineMaterial );
}


void UOverlayComponent::SetPointMaterial( UMaterialInterface* InPointMaterial )
{
	PointMaterial = InPointMaterial;
	SetMaterial( 1, InPointMaterial );
}


void UOverlayComponent::Clear()
{
	Lines.Reset();
	Points.Reset();
	Triangles.Reset();
	for( int32 Index = 0; Index < TrianglesByMaterial.Num(); Index++ )
	{
		SetMaterial( Index + 2, nullptr );
	}
	TrianglesByMaterial.Reset();
	MaterialToIndex.Reset();
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


FOverlayLineID UOverlayComponent::AddLine( const FOverlayLine& OverlayLine )
{
	const FOverlayLineID ID( Lines.Add( OverlayLine ) );
	MarkRenderStateDirty();
	bBoundsDirty = true;
	return ID;
}


void UOverlayComponent::InsertLine( const FOverlayLineID ID, const FOverlayLine& OverlayLine )
{
	Lines.Insert( ID.GetValue(), OverlayLine ); 
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


void UOverlayComponent::SetLineColor( const FOverlayLineID ID, const FColor& NewColor )
{
	FOverlayLine& OverlayLine = Lines[ ID.GetValue() ];
	OverlayLine.Color = NewColor;
	MarkRenderStateDirty();
}


void UOverlayComponent::SetLineThickness( const FOverlayLineID ID, const float NewThickness )
{
	FOverlayLine& OverlayLine = Lines[ ID.GetValue() ];
	OverlayLine.Thickness = NewThickness;
	MarkRenderStateDirty();
}


void UOverlayComponent::RemoveLine( const FOverlayLineID ID )
{
	Lines.RemoveAt( ID.GetValue() );
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


bool UOverlayComponent::IsLineValid( const FOverlayLineID ID ) const
{
	return Lines.IsAllocated( ID.GetValue() );
}


FOverlayPointID UOverlayComponent::AddPoint( const FOverlayPoint& OverlayPoint )
{
	const FOverlayPointID ID( Points.Add( OverlayPoint ) );
	MarkRenderStateDirty();
	bBoundsDirty = true;
	return ID;
}


void UOverlayComponent::InsertPoint( const FOverlayPointID ID, const FOverlayPoint& OverlayPoint )
{
	Points.Insert( ID.GetValue(), OverlayPoint );
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


void UOverlayComponent::SetPointColor( const FOverlayPointID ID, const FColor& NewColor )
{
	FOverlayPoint& OverlayPoint = Points[ ID.GetValue() ];
	OverlayPoint.Color = NewColor;
	MarkRenderStateDirty();
}


void UOverlayComponent::SetPointSize( const FOverlayPointID ID, const float NewSize )
{
	FOverlayPoint& OverlayPoint = Points[ ID.GetValue() ];
	OverlayPoint.Size = NewSize;
	MarkRenderStateDirty();
}


void UOverlayComponent::RemovePoint( const FOverlayPointID ID )
{
	Points.RemoveAt( ID.GetValue() );
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


bool UOverlayComponent::IsPointValid( const FOverlayPointID ID ) const
{
	return Points.IsAllocated( ID.GetValue() );
}


int32 UOverlayComponent::FindOrAddMaterialIndex( UMaterialInterface* Material )
{
	const int32* MaterialIndexPtr = MaterialToIndex.Find( Material );
	if( MaterialIndexPtr == nullptr )
	{
		const int32 MaterialIndex = TrianglesByMaterial.Add( TSparseArray<FOverlayTriangle>() );
		MaterialToIndex.Add( Material, MaterialIndex );
		SetMaterial( MaterialIndex + 2, Material );
		return MaterialIndex;
	}

	return *MaterialIndexPtr;
}


FOverlayTriangleID UOverlayComponent::AddTriangle( const FOverlayTriangle& OverlayTriangle )
{
	const int32 MaterialIndex = FindOrAddMaterialIndex( OverlayTriangle.Material );
	const int32 IndexByMaterial = TrianglesByMaterial[ MaterialIndex ].Add( OverlayTriangle );
	const FOverlayTriangleID ID( Triangles.Add( MakeTuple( MaterialIndex, IndexByMaterial ) ) );
	MarkRenderStateDirty();
	bBoundsDirty = true;
	return ID;
}


void UOverlayComponent::InsertTriangle( const FOverlayTriangleID ID, const FOverlayTriangle& OverlayTriangle )
{
	const int32 MaterialIndex = FindOrAddMaterialIndex( OverlayTriangle.Material );
	const int32 IndexByMaterial = TrianglesByMaterial[ MaterialIndex ].Add( OverlayTriangle );
	Triangles.Insert( ID.GetValue(), MakeTuple( MaterialIndex, IndexByMaterial ) );
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


void UOverlayComponent::RemoveTriangle( const FOverlayTriangleID ID )
{
	const TTuple<int32, int32> MaterialAndTriangleIndex = Triangles[ ID.GetValue() ];
	const int32 MaterialIndex = MaterialAndTriangleIndex.Get<0>();
	const int32 IndexByMaterial = MaterialAndTriangleIndex.Get<1>();
	TSparseArray<FOverlayTriangle>& Container = TrianglesByMaterial[ MaterialIndex ];
	Container.RemoveAt( IndexByMaterial );
	if( Container.Num() == 0 )
	{
		TrianglesByMaterial.RemoveAt( MaterialIndex );
		SetMaterial( MaterialIndex + 2, nullptr );
	}
	Triangles.RemoveAt( ID.GetValue() );
	MarkRenderStateDirty();
	bBoundsDirty = true;
}


bool UOverlayComponent::IsTriangleValid( const FOverlayTriangleID ID ) const
{
	return Triangles.IsAllocated( ID.GetValue() );
}


FPrimitiveSceneProxy* UOverlayComponent::CreateSceneProxy()
{
	if( Lines.Num() > 0 || Points.Num() > 0 || Triangles.Num() > 0 )
	{
		return new FOverlaySceneProxy( this );
	}

	return nullptr;
}


int32 UOverlayComponent::GetNumMaterials() const
{
	return TrianglesByMaterial.GetMaxIndex() + 2;
}


FBoxSphereBounds UOverlayComponent::CalcBounds( const FTransform& LocalToWorld ) const
{
	if( bBoundsDirty )
	{
		FBox Box( ForceInit );

		for( const FOverlayLine& Line : Lines )
		{
			Box += Line.Start;
			Box += Line.End;
		}

		for( const FOverlayPoint& Point : Points )
		{
			Box += Point.Position;
		}

		for( const TSparseArray<FOverlayTriangle>& TriangleArray : TrianglesByMaterial )
		{
			for( const FOverlayTriangle& Triangle : TriangleArray )
			{
				Box += Triangle.Vertex0.Position;
				Box += Triangle.Vertex1.Position;
				Box += Triangle.Vertex2.Position;
			}
		}

		Bounds = FBoxSphereBounds( Box );
		bBoundsDirty = false;
	}

	return Bounds.TransformBy( LocalToWorld );
}

