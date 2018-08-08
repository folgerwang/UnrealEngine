// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "Rendering/SkinWeightVertexBuffer.h"

class UGeometryCollectionComponent;

/** Index Buffer */
class FGeometryCollectionIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Dynamic, CreateInfo);
	}

	int32 NumIndices;
};


/** Immutable rendering data (kind of) */
struct FGeometryCollectionConstantData
{
	TArray<FVector> Vertices;
	TArray<FIntVector> Indices;
	TArray<FVector> Normals;
	TArray<FVector> TangentU;
	TArray<FVector> TangentV;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<uint16> BoneMap;
};

/** Mutable rendering data */
struct FGeometryCollectionDynamicData
{
	TArray<FMatrix> Transforms;
};

/***
*   FGeometryCollectionSceneProxy
*    
*	The FGeometryCollectionSceneProxy manages the interaction between the GeometryCollectionComponent
*   on the game thread and the vertex buffers on the render thread.
*
*   NOTE : This class is still in flux, and has a few pending todos. Your comments and 
*   thoughts are appreciated though. The remaining items to address involve:
*   - @todo double buffer - The double buffering of the FGeometryCollectionDynamicData.
*   - @todo previous state - Saving the previous FGeometryCollectionDynamicData for rendering motion blur.
*   - @todo shared memory model - The Asset(or Actor?) should hold the Vertex buffer, and pass the reference to the SceneProxy
*   - @todo GPU skin : Make the skinning use the GpuVertexShader
*/
class FGeometryCollectionSceneProxy final : public FPrimitiveSceneProxy
{
	UMaterialInterface* Material;
	FMaterialRelevance MaterialRelevance;

	int32 NumVertices;
	int32 NumIndices;

	FLocalVertexFactory VertexFactory;
	FStaticMeshVertexBuffers VertexBuffers;
	FGeometryCollectionIndexBuffer IndexBuffer;

	FGeometryCollectionDynamicData* DynamicData;
	FGeometryCollectionConstantData* ConstantData;

public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component);

	/** virtual destructor */
	virtual ~FGeometryCollectionSceneProxy();

	/** Current number of vertices to render */
	int32 GetRequiredVertexCount() const { return NumVertices;}

	/** Current number of indices to connect */
	int32 GetRequiredIndexCount() const { return NumIndices; }

	/** Called on render thread to setup static geometry for rendering */
	void SetConstantData_RenderThread(FGeometryCollectionConstantData* NewConstantData);

	/** Called on render thread to setup dynamic geometry for rendering */
	void SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData);

	/** Called on render thread to construct the vertex definitions */
	void BuildGeometry(const FGeometryCollectionConstantData* ConstantDataIn, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices);

	/** Called on render thread to setup dynamic geometry for rendering */
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	/** Manage the view assignment */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	// @todo allocated size : make this reflect internally allocated memory. 
	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	/** Size of the base class */
	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

protected:

	/** Create the rendering buffer resources */
	void InitResources();

	/** Return the rendering buffer resources */
	void ReleaseResources();

};

