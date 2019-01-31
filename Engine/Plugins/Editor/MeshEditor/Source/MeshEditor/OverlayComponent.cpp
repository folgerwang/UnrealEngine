// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
#include "StaticMeshResources.h"
#include "Algo/Accumulate.h"


struct FMeshBatchData
{
	FMeshBatchData()
		: MaterialProxy(nullptr)
	{}

	FMaterialRenderProxy* MaterialProxy;
	int32 StartIndex;
	int32 NumPrimitives;
	int32 MinVertexIndex;
	int32 MaxVertexIndex;
};


class FOverlaySceneProxy final : public FPrimitiveSceneProxy
{
public:

	FOverlaySceneProxy( UOverlayComponent* Component )
		: FPrimitiveSceneProxy( Component ),
		  MaterialRelevance( Component->GetMaterialRelevance( GetScene().GetFeatureLevel() ) ),
		  VertexFactory( GetScene().GetFeatureLevel(), "FOverlaySceneProxy" )
	{
		ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
		// @todo perf: Render state is marked dirty every time a new element is added, and a new scene proxy will be created.
		// Perhaps we can instead create the render buffers as BUF_Dynamic and amend them directly on the render thread.
		// However they will still require a reallocation if the number of elements grows, so this may not be a worthwhile optimization.

		const int32 NumLineVertices = Component->Lines.Num() * 4;
		const int32 NumLineIndices = Component->Lines.Num() * 6;
		const int32 NumPointVertices = Component->Points.Num() * 4;
		const int32 NumPointIndices = Component->Points.Num() * 6;
		const int32 NumTriangleVertices = Algo::Accumulate( Component->TrianglesByMaterial, 0, []( int32 Acc, const TSparseArray<FOverlayTriangle>& Tris ) { return Acc + Tris.Num() * 3; } );
		const int32 NumTriangleIndices = Algo::Accumulate( Component->TrianglesByMaterial, 0, []( int32 Acc, const TSparseArray<FOverlayTriangle>& Tris ) { return Acc + Tris.Num() * 3; } );

		const int32 TotalNumVertices = NumLineVertices + NumPointVertices + NumTriangleVertices;
		const int32 TotalNumIndices = NumLineIndices + NumPointIndices + NumTriangleIndices;
		const int32 NumTextureCoordinates = 1;

		VertexBuffers.PositionVertexBuffer.Init( TotalNumVertices );
		VertexBuffers.StaticMeshVertexBuffer.Init( TotalNumVertices, NumTextureCoordinates );
		VertexBuffers.ColorVertexBuffer.Init( TotalNumVertices );
		IndexBuffer.Indices.SetNumUninitialized( TotalNumIndices );

		int32 VertexBufferIndex = 0;
		int32 IndexBufferIndex = 0;

		// Initialize lines.
		// Lines are represented as two tris of zero thickness.
		// The vertex normals are used to hold the normalized line direction + a sign determining the direction in which the
		// material should thicken the polys.
		if( Component->Lines.Num() > 0 )
		{
			MeshBatchDatas.Emplace();
			FMeshBatchData& MeshBatchData = MeshBatchDatas.Last();
			MeshBatchData.MinVertexIndex = VertexBufferIndex;
			MeshBatchData.MaxVertexIndex = VertexBufferIndex + NumLineVertices - 1;
			MeshBatchData.StartIndex = IndexBufferIndex;
			MeshBatchData.NumPrimitives = Component->Lines.Num() * 2;
			MeshBatchData.MaterialProxy = Component->GetMaterial( 0 )->GetRenderProxy();

			for( const FOverlayLine& OverlayLine : Component->Lines )
			{
				const FVector LineDirection = ( OverlayLine.End - OverlayLine.Start ).GetSafeNormal();
				const FVector2D UV( OverlayLine.Thickness, 0.0f );

				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 0 ) = OverlayLine.Start;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 1 ) = OverlayLine.End;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 2 ) = OverlayLine.End;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 3 ) = OverlayLine.Start;

				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 0, FVector::ZeroVector, FVector::ZeroVector, -LineDirection );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 1, FVector::ZeroVector, FVector::ZeroVector, -LineDirection );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 2, FVector::ZeroVector, FVector::ZeroVector, LineDirection );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 3, FVector::ZeroVector, FVector::ZeroVector, LineDirection );

				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 0, 0, UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 1, 0, UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 2, 0, UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 3, 0, UV );

				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 0 ) = OverlayLine.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 1 ) = OverlayLine.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 2 ) = OverlayLine.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 3 ) = OverlayLine.Color;

				IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
				IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
				IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;
				IndexBuffer.Indices[ IndexBufferIndex + 3 ] = VertexBufferIndex + 2;
				IndexBuffer.Indices[ IndexBufferIndex + 4 ] = VertexBufferIndex + 3;
				IndexBuffer.Indices[ IndexBufferIndex + 5 ] = VertexBufferIndex + 0;

				VertexBufferIndex += 4;
				IndexBufferIndex += 6;
			}
		}

		// Initialize points.
		// Points are represented as two tris, all of whose vertices are coincident.
		// The material then offsets them according to the signs of the vertex normals in a camera facing orientation.
		// Size of the point is given by U0.
		if( Component->Points.Num() > 0 )
		{
			MeshBatchDatas.Emplace();
			FMeshBatchData& MeshBatchData = MeshBatchDatas.Last();
			MeshBatchData.MinVertexIndex = VertexBufferIndex;
			MeshBatchData.MaxVertexIndex = VertexBufferIndex + NumPointVertices - 1;
			MeshBatchData.StartIndex = IndexBufferIndex;
			MeshBatchData.NumPrimitives = Component->Points.Num() * 2;
			MeshBatchData.MaterialProxy = Component->GetMaterial( 1 )->GetRenderProxy();

			for( const FOverlayPoint& OverlayPoint : Component->Points )
			{
				const FVector2D UV( OverlayPoint.Size, 0.0f );

				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 0 ) = OverlayPoint.Position;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 1 ) = OverlayPoint.Position;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 2 ) = OverlayPoint.Position;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 3 ) = OverlayPoint.Position;

				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 0, FVector::ZeroVector, FVector::ZeroVector, FVector( 1.0f, -1.0f, 0.0f ) );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 1, FVector::ZeroVector, FVector::ZeroVector, FVector( 1.0f, 1.0f, 0.0f ) );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 2, FVector::ZeroVector, FVector::ZeroVector, FVector( -1.0f, 1.0f, 0.0f ) );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 3, FVector::ZeroVector, FVector::ZeroVector, FVector( -1.0f, -1.0f, 0.0f ) );

				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 0, 0, UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 1, 0, UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 2, 0, UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 3, 0, UV );

				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 0 ) = OverlayPoint.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 1 ) = OverlayPoint.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 2 ) = OverlayPoint.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 3 ) = OverlayPoint.Color;

				IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
				IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
				IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;
				IndexBuffer.Indices[ IndexBufferIndex + 3 ] = VertexBufferIndex + 2;
				IndexBuffer.Indices[ IndexBufferIndex + 4 ] = VertexBufferIndex + 3;
				IndexBuffer.Indices[ IndexBufferIndex + 5 ] = VertexBufferIndex + 0;

				VertexBufferIndex += 4;
				IndexBufferIndex += 6;
			}
		}

		// Triangles
		int32 MaterialIndex = 2;
		for( const auto& MaterialTriangles : Component->TrianglesByMaterial )
		{
			MeshBatchDatas.Emplace();
			FMeshBatchData& MeshBatchData = MeshBatchDatas.Last();
			MeshBatchData.MinVertexIndex = VertexBufferIndex;
			MeshBatchData.MaxVertexIndex = VertexBufferIndex + MaterialTriangles.Num() * 3 - 1;
			MeshBatchData.StartIndex = IndexBufferIndex;
			MeshBatchData.NumPrimitives = MaterialTriangles.Num();
			MeshBatchData.MaterialProxy = Component->GetMaterial( MaterialIndex )->GetRenderProxy();

			for( const FOverlayTriangle& Triangle : MaterialTriangles )
			{
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 0 ) = Triangle.Vertex0.Position;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 1 ) = Triangle.Vertex1.Position;
				VertexBuffers.PositionVertexBuffer.VertexPosition( VertexBufferIndex + 2 ) = Triangle.Vertex2.Position;

				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 0, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), Triangle.Vertex0.Normal );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 1, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), Triangle.Vertex1.Normal );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents( VertexBufferIndex + 2, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), Triangle.Vertex2.Normal );

				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 0, 0, Triangle.Vertex0.UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 1, 0, Triangle.Vertex1.UV );
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV( VertexBufferIndex + 2, 0, Triangle.Vertex2.UV );

				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 0 ) = Triangle.Vertex0.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 1 ) = Triangle.Vertex1.Color;
				VertexBuffers.ColorVertexBuffer.VertexColor( VertexBufferIndex + 2 ) = Triangle.Vertex2.Color;

				IndexBuffer.Indices[ IndexBufferIndex + 0 ] = VertexBufferIndex + 0;
				IndexBuffer.Indices[ IndexBufferIndex + 1 ] = VertexBufferIndex + 1;
				IndexBuffer.Indices[ IndexBufferIndex + 2 ] = VertexBufferIndex + 2;

				VertexBufferIndex += 3;
				IndexBufferIndex += 3;
			}

			MaterialIndex++;
		}

		ENQUEUE_RENDER_COMMAND( OverlayVertexBuffersInit )(
			[ this ]( FRHICommandListImmediate& RHICmdList )
			{
				VertexBuffers.PositionVertexBuffer.InitResource();
				VertexBuffers.StaticMeshVertexBuffer.InitResource();
				VertexBuffers.ColorVertexBuffer.InitResource();

				FLocalVertexFactory::FDataType Data;
				VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer( &VertexFactory, Data );
				VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer( &VertexFactory, Data );
				VertexBuffers.StaticMeshVertexBuffer.BindTexCoordVertexBuffer( &VertexFactory, Data );
				VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer( &VertexFactory, Data );
				VertexFactory.SetData( Data );

				VertexFactory.InitResource();
				IndexBuffer.InitResource();
			});
	}

	virtual ~FOverlaySceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
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
					BatchElement.IndexBuffer = &IndexBuffer;
					Mesh.bWireframe = false;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = MeshBatchData.MaterialProxy;

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, UseEditorDepthTest());
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = MeshBatchData.StartIndex;
					BatchElement.NumPrimitives = MeshBatchData.NumPrimitives;
					BatchElement.MinVertexIndex = MeshBatchData.MinVertexIndex;
					BatchElement.MaxVertexIndex = MeshBatchData.MaxVertexIndex;
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
	TArray<FMeshBatchData> MeshBatchDatas;
	FMaterialRelevance MaterialRelevance;
	FLocalVertexFactory VertexFactory;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
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
		MaterialToIndex.Remove(GetMaterial(MaterialIndex + 2));
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

