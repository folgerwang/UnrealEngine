// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCacheVertexFactoryUniformBufferParameters, ENGINE_API)
	SHADER_PARAMETER(FVector, MeshOrigin)
	SHADER_PARAMETER(FVector, MeshExtension)
	SHADER_PARAMETER(FVector, MotionBlurDataOrigin)
	SHADER_PARAMETER(FVector, MotionBlurDataExtension)
	SHADER_PARAMETER(float, MotionBlurPositionScale)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FGeometryCacheVertexFactoryUniformBufferParameters> FGeometryCacheVertexFactoryUniformBufferParametersRef;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCacheManualVertexFetchUniformBufferParameters, ENGINE_API)
	SHADER_PARAMETER_SRV(Buffer<float>, Position)
	SHADER_PARAMETER_SRV(Buffer<float>, MotionBlurData)
	SHADER_PARAMETER_SRV(Buffer<half4>, TangentX)
	SHADER_PARAMETER_SRV(Buffer<half4>, TangentZ)
	SHADER_PARAMETER_SRV(Buffer<float4>, Color)
	SHADER_PARAMETER_SRV(Buffer<float>, TexCoords)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FGeometryCacheManualVertexFetchUniformBufferParameters> FGeometryCacheManualVertexFetchUniformBufferParametersRef;

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

	FGeometryCacheVertexFactoryUniformBufferParametersRef UniformBuffer;

	FShaderResourceViewRHIRef PositionSRV;
	FShaderResourceViewRHIRef TangentXSRV;
	FShaderResourceViewRHIRef TangentZSRV;
	FShaderResourceViewRHIRef ColorSRV;
	FShaderResourceViewRHIRef MotionBlurDataSRV;
	FShaderResourceViewRHIRef TexCoordsSRV;

	FUniformBufferRHIRef ManualVertexFetchUniformBuffer;
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


	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	*/
	void SetData(const FDataType& InData);

	void CreateManualVertexFetchUniformBuffer(
		const FVertexBuffer* PoistionBuffer,
		const FVertexBuffer* MotionBlurBuffer,
		FGeometryCacheVertexFactoryUserData& OutUserData) const;

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

