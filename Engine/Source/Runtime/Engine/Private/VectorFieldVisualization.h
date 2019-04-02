// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	VectorFieldVisualization.h: Visualization of vector fields.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"

class FMaterial;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class FSceneView;
class FVectorFieldInstance;

/*------------------------------------------------------------------------------
	Forward declarations.
------------------------------------------------------------------------------*/

/** An instance of a vector field. */
class FVectorFieldInstance;
/** Vertex factory shader parameters for visualizing vector fields. */
class FVectorFieldVisualizationVertexFactoryShaderParameters;
/** Interface with which primitives can be drawn. */
class FPrimitiveDrawInterface;
/** View from which a scene is rendered. */
class FSceneView;

/*------------------------------------------------------------------------------
	Vertex factory for visualizing vector fields.
------------------------------------------------------------------------------*/

/**
 * Uniform buffer to hold parameters for vector field visualization.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FVectorFieldVisualizationParameters, )
	SHADER_PARAMETER( FMatrix, VolumeToWorld )
	SHADER_PARAMETER( FMatrix, VolumeToWorldNoScale )
	SHADER_PARAMETER( FVector, VoxelSize )
	SHADER_PARAMETER( float, Scale )
END_GLOBAL_SHADER_PARAMETER_STRUCT()
typedef TUniformBufferRef<FVectorFieldVisualizationParameters> FVectorFieldVisualizationBufferRef;

/**
 * Vertex factory for visualizing vector field volumes.
 */
class FVectorFieldVisualizationVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FVectorFieldVisualizationVertexFactory);

public:
	FVectorFieldVisualizationVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel)
	{
	}

	/**
	 * Constructs render resources for this vertex factory.
	 */
	virtual void InitRHI() override;

	/**
	 * Release render resources for this vertex factory.
	 */
	virtual void ReleaseRHI() override;

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Construct shader parameters for this type of vertex factory.
	 */
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	/**
	 * Set parameters for this vertex factory instance.
	 */
	void SetParameters(
		const FVectorFieldVisualizationParameters& InUniformParameters,
		FTexture3DRHIParamRef InVectorFieldTextureRHI );

private:

	/** Allow the shader parameters class access to private members. */
	friend class FVectorFieldVisualizationVertexFactoryShaderParameters;

	/** Uniform buffer. */
	FUniformBufferRHIRef UniformBuffer;
	/** Texture containing the vector field. */
	FTexture3DRHIParamRef VectorFieldTextureRHI;
};

/*------------------------------------------------------------------------------
	Drawing interface.
------------------------------------------------------------------------------*/

/**
 * Draw the bounds for a vector field instance.
 * @param PDI - The primitive drawing interface with which to draw.
 * @param View - The view in which to draw.
 * @param VectorFieldInstance - The vector field instance to draw.
 */
void DrawVectorFieldBounds(
	FPrimitiveDrawInterface* PDI,
	const FSceneView* View,
	FVectorFieldInstance* VectorFieldInstance );

void GetVectorFieldMesh(
	FVectorFieldVisualizationVertexFactory* VertexFactory,
	FVectorFieldInstance* VectorFieldInstance,
	int32 ViewIndex,
	FMeshElementCollector& Collector);
