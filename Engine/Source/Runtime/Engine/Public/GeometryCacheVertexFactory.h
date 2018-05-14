// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUSkinVertexFactory.h: GPU skinning vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"

/**
 * The mesh batch element user data should point to an instance of this struct
 */
struct FGeometryCacheVertexFactoryUserData
{
	const FVertexBuffer* PositionBuffer;
	const FVertexBuffer* MotionBlurDataBuffer;

	// Gpu vertex decompression parameters
	FVector MeshOrigin;
	FVector MeshExtension;

	// Motion blur parameters
	FVector MotionBlurDataOrigin;
	FVector MotionBlurDataExtension;
	float MotionBlurPositionScale;
};

class FGeometryCacheVertexFactoryShaderParameters;

/** 
 * Vertex factory for geometry caches. Allows specifying explicit motion blur data as
 * previous frames or motion vectors.
 */
class ENGINE_API FGeometryCacheVertexVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGeometryCacheVertexVertexFactory);

	typedef FVertexFactory Super;

public:
	FGeometryCacheVertexVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel), PositionStreamIndex(-1), MotionBlurDataStreamIndex(-1)
	{}

	struct FDataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent, TFixedAllocator<MAX_STATIC_TEXCOORDS / 2> > TextureCoordinates;

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;

		/** The stream to read the motion blur data from. */
		FVertexStreamComponent MotionBlurDataComponent;
	};


	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	*/
	void SetData(const FDataType& InData);

	virtual void InitRHI() override;

	// FRenderResource interface.
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	friend FGeometryCacheVertexFactoryShaderParameters;
	
protected:
	// Vertex buffer required for creating the Vertex Declaration
	FVertexBuffer VBAlias;

	int32 PositionStreamIndex;
	int32 MotionBlurDataStreamIndex;

	FDataType Data;
};

