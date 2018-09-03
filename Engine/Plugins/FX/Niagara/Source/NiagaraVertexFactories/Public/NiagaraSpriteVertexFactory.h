// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "NiagaraVertexFactory.h"
#include "NiagaraDataSet.h"
#include "SceneView.h"
#include "NiagaraGlobalReadBuffer.h"

class FMaterial;

/**
 * Uniform buffer for particle sprite vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FNiagaraSpriteUniformParameters, NIAGARAVERTEXFACTORIES_API)
	UNIFORM_MEMBER_EX( FMatrix, LocalToWorld, EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX( FMatrix, LocalToWorldInverseTransposed, EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX( FVector, CustomFacingVectorMask, EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX( FVector4, TangentSelector, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( FVector4, NormalsSphereCenter, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( FVector4, NormalsCylinderUnitDirection, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( FVector4, SubImageSize, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( FVector, CameraFacingBlend, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( float, RemoveHMDRoll, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER( FVector4, MacroUVParameters )
	UNIFORM_MEMBER_EX( float, RotationScale, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( float, RotationBias, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( float, NormalsType, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( float, DeltaSeconds, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER_EX( FVector2D, PivotOffset, EShaderPrecisionModifier::Half )
	UNIFORM_MEMBER(int, PositionDataOffset)
	UNIFORM_MEMBER(int, VelocityDataOffset)
	UNIFORM_MEMBER(int, RotationDataOffset)
	UNIFORM_MEMBER(int, SizeDataOffset)
	UNIFORM_MEMBER(int, SubimageDataOffset)
	UNIFORM_MEMBER(int, ColorDataOffset)
	UNIFORM_MEMBER(int, MaterialParamDataOffset)
	UNIFORM_MEMBER(int, MaterialParam1DataOffset)
	UNIFORM_MEMBER(int, MaterialParam2DataOffset)
	UNIFORM_MEMBER(int, MaterialParam3DataOffset)
	UNIFORM_MEMBER(int, FacingDataOffset)
	UNIFORM_MEMBER(int, AlignmentDataOffset)
	UNIFORM_MEMBER(int, SubImageBlendMode)
	UNIFORM_MEMBER(int, CameraOffsetDataOffset)
	UNIFORM_MEMBER(int, UVScaleDataOffset)
	UNIFORM_MEMBER(int, NormalizedAgeDataOffset)
	UNIFORM_MEMBER(int, MaterialRandomDataOffset)
	UNIFORM_MEMBER(FVector4, DefaultPos)
	END_UNIFORM_BUFFER_STRUCT(FNiagaraSpriteUniformParameters)

typedef TUniformBufferRef<FNiagaraSpriteUniformParameters> FNiagaraSpriteUniformBufferRef;

/**
 * Vertex factory for rendering particle sprites.
 */
class NIAGARAVERTEXFACTORIES_API FNiagaraSpriteVertexFactory : public FNiagaraVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FNiagaraSpriteVertexFactory);

public:

	/** Default constructor. */
	FNiagaraSpriteVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel )
		: FNiagaraVertexFactoryBase(InType, InFeatureLevel),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		AlignmentMode(0),
		FacingMode(0),
		FloatDataOffset(0),
		FloatDataStride(0),
		SortedIndicesOffset(0)
	{}

	FNiagaraSpriteVertexFactory()
		: FNiagaraVertexFactoryBase(NVFT_MAX, ERHIFeatureLevel::Num),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		AlignmentMode(0),
		FacingMode(0),
		FloatDataOffset(0),
		FloatDataStride(0),
		SortedIndicesOffset(0)
	{}

	// FRenderResource interface.
	virtual void InitRHI() override;

	virtual bool RendersPrimitivesAsCameraFacingSprites() const override { return true; }

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	
	void SetTexCoordBuffer(const FVertexBuffer* InTexCoordBuffer);

	inline void SetNumVertsInInstanceBuffer(int32 InNumVertsInInstanceBuffer)
	{
		NumVertsInInstanceBuffer = InNumVertsInInstanceBuffer;
	}
	
	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetSpriteUniformBuffer( const FNiagaraSpriteUniformBufferRef& InSpriteUniformBuffer )
	{
		SpriteUniformBuffer = InSpriteUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FUniformBufferRHIParamRef GetSpriteUniformBuffer()
	{
		return SpriteUniformBuffer;
	}

	void SetCutoutParameters(int32 InNumCutoutVerticesPerFrame, FShaderResourceViewRHIParamRef InCutoutGeometrySRV)
	{
		NumCutoutVerticesPerFrame = InNumCutoutVerticesPerFrame;
		CutoutGeometrySRV = InCutoutGeometrySRV;
	}

	inline int32 GetNumCutoutVerticesPerFrame() const { return NumCutoutVerticesPerFrame; }
	inline FShaderResourceViewRHIParamRef GetCutoutGeometrySRV() const { return CutoutGeometrySRV; }

	void SetParticleData(const FShaderResourceViewRHIRef& InParticleDataFloatSRV, uint32 InFloatDataOffset, uint32 InFloatDataStride)
	{
		ParticleDataFloatSRV = InParticleDataFloatSRV;
		FloatDataOffset = InFloatDataOffset;
		FloatDataStride = InFloatDataStride;
	}

	void SetSortedIndices(const FShaderResourceViewRHIRef& InSortedIndicesSRV, uint32 InSortedIndicesOffset)
	{
		SortedIndicesSRV = InSortedIndicesSRV;
		SortedIndicesOffset = InSortedIndicesOffset;
	}

	FORCEINLINE FShaderResourceViewRHIRef GetParticleDataFloatSRV()
	{
		return ParticleDataFloatSRV;
	}

	FORCEINLINE int32 GetFloatDataOffset()
	{
		return FloatDataOffset;
	}

	FORCEINLINE int32 GetFloatDataStride()
	{
		return FloatDataStride;
	}

	FORCEINLINE FShaderResourceViewRHIRef GetSortedIndicesSRV()
	{
		return SortedIndicesSRV;
	}

	FORCEINLINE int32 GetSortedIndicesOffset()
	{
		return SortedIndicesOffset;
	}

	void SetFacingMode(uint32 InMode)
	{
		FacingMode = InMode;
	}

	uint32 GetFacingMode()
	{
		return FacingMode;
	}

	void SetAlignmentMode(uint32 InMode)
	{
		AlignmentMode = InMode;
	}

	uint32 GetAlignmentMode()
	{
		return AlignmentMode;
	}

	/**
	 * Construct shader parameters for this type of vertex factory.
	 */
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/** Initialize streams for this vertex factory. */
	void InitStreams();

private:

	int32 NumVertsInInstanceBuffer;

	/** Uniform buffer with sprite parameters. */
	FUniformBufferRHIParamRef SpriteUniformBuffer;

	int32 NumCutoutVerticesPerFrame;
	FShaderResourceViewRHIParamRef CutoutGeometrySRV;
	uint32 AlignmentMode;
	uint32 FacingMode;
	
	
	FShaderResourceViewRHIRef ParticleDataFloatSRV;
	uint32 FloatDataOffset;
	uint32 FloatDataStride;

	FShaderResourceViewRHIRef SortedIndicesSRV;
	uint32 SortedIndicesOffset;
};
