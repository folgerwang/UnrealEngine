// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DummyRenderResources.cpp: Implementations of frequently used render resources.
=============================================================================*/

#include "CommonRenderResources.h"
#include "Containers/DynamicRHIResourceArray.h"


TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
TGlobalResource<FEmptyVertexDeclaration> GEmptyVertexDeclaration;

TGlobalResource<FScreenRectangleVertexBuffer> GScreenRectangleVertexBuffer;
TGlobalResource<FScreenRectangleIndexBuffer> GScreenRectangleIndexBuffer;

IMPLEMENT_GLOBAL_SHADER(FVisualizeTextureVS, "/Engine/Private/Tools/FullscreenVertexShader.usf", "MainVS", SF_Vertex);


void FScreenRectangleVertexBuffer::InitRHI()
{
	TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
	Vertices.SetNumUninitialized(6);

	Vertices[0].Position = FVector4(1, 1, 0, 1);
	Vertices[0].UV = FVector2D(1, 1);

	Vertices[1].Position = FVector4(0, 1, 0, 1);
	Vertices[1].UV = FVector2D(0, 1);

	Vertices[2].Position = FVector4(1, 0, 0, 1);
	Vertices[2].UV = FVector2D(1, 0);

	Vertices[3].Position = FVector4(0, 0, 0, 1);
	Vertices[3].UV = FVector2D(0, 0);

	//The final two vertices are used for the triangle optimization (a single triangle spans the entire viewport )
	Vertices[4].Position = FVector4(-1, 1, 0, 1);
	Vertices[4].UV = FVector2D(-1, 1);

	Vertices[5].Position = FVector4(1, -1, 0, 1);
	Vertices[5].UV = FVector2D(1, -1);

	// Create vertex buffer. Fill buffer with initial data upon creation
	FRHIResourceCreateInfo CreateInfo(&Vertices);
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
}

void FScreenRectangleIndexBuffer::InitRHI()
{
	// Indices 0 - 5 are used for rendering a quad. Indices 6 - 8 are used for triangle optimization.
	const uint16 Indices[] = { 0, 1, 2, 2, 1, 3, 0, 4, 5 };

	TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
	uint32 NumIndices = ARRAY_COUNT(Indices);
	IndexBuffer.AddUninitialized(NumIndices);
	FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

	// Create index buffer. Fill buffer with initial data upon creation
	FRHIResourceCreateInfo CreateInfo(&IndexBuffer);
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
}
