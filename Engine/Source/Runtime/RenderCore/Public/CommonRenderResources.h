// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DummyRenderResource.h: Frequently used rendering resources
=============================================================================*/

#pragma once

#include "RenderResource.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "PipelineStateCache.h"


/** The vertex data used to filter a texture. */
struct FFilterVertex
{
	FVector4 Position;
	FVector2D UV;
};

/** The filter vertex declaration resource type. */
class FFilterVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FFilterVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FFilterVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FFilterVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FFilterVertex, UV), VET_Float2, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern RENDERCORE_API TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/** The empty vertex declaration resource type. */
class FEmptyVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FEmptyVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern RENDERCORE_API TGlobalResource<FEmptyVertexDeclaration> GEmptyVertexDeclaration;

/**
 * Static vertex and index buffer used for 2D screen rectangles.
 */
class FScreenRectangleVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override;
};

extern RENDERCORE_API TGlobalResource<FScreenRectangleVertexBuffer> GScreenRectangleVertexBuffer;


class FScreenRectangleIndexBuffer : public FIndexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override;
};

extern RENDERCORE_API TGlobalResource<FScreenRectangleIndexBuffer> GScreenRectangleIndexBuffer;


/** Vertex shader to draw a full screen quad that works on all platforms. */
class RENDERCORE_API FVisualizeTextureVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeTextureVS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeTextureVS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()
};
