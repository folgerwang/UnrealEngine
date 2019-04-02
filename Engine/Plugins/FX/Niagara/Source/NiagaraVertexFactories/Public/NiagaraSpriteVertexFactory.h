// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

class FMaterial;

/**
 * Uniform buffer for particle sprite vertex factories.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FNiagaraSpriteUniformParameters, NIAGARAVERTEXFACTORIES_API)
	SHADER_PARAMETER_EX( FMatrix, LocalToWorld, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX( FMatrix, LocalToWorldInverseTransposed, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX( FVector, CustomFacingVectorMask, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX( FVector4, TangentSelector, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector4, NormalsSphereCenter, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector4, NormalsCylinderUnitDirection, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector4, SubImageSize, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector, CameraFacingBlend, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, RemoveHMDRoll, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER( FVector4, MacroUVParameters )
	SHADER_PARAMETER_EX( float, RotationScale, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, RotationBias, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, NormalsType, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, DeltaSeconds, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector2D, PivotOffset, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER(int, PositionDataOffset)
	SHADER_PARAMETER(int, VelocityDataOffset)
	SHADER_PARAMETER(int, RotationDataOffset)
	SHADER_PARAMETER(int, SizeDataOffset)
	SHADER_PARAMETER(int, SubimageDataOffset)
	SHADER_PARAMETER(int, ColorDataOffset)
	SHADER_PARAMETER(int, MaterialParamDataOffset)
	SHADER_PARAMETER(int, MaterialParam1DataOffset)
	SHADER_PARAMETER(int, MaterialParam2DataOffset)
	SHADER_PARAMETER(int, MaterialParam3DataOffset)
	SHADER_PARAMETER(int, FacingDataOffset)
	SHADER_PARAMETER(int, AlignmentDataOffset)
	SHADER_PARAMETER(int, SubImageBlendMode)
	SHADER_PARAMETER(int, CameraOffsetDataOffset)
	SHADER_PARAMETER(int, UVScaleDataOffset)
	SHADER_PARAMETER(int, NormalizedAgeDataOffset)
	SHADER_PARAMETER(int, MaterialRandomDataOffset)
	SHADER_PARAMETER(FVector4, DefaultPos)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FNiagaraSpriteUniformParameters> FNiagaraSpriteUniformBufferRef;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNiagaraSpriteVFLooseParameters, NIAGARAVERTEXFACTORIES_API)
	SHADER_PARAMETER(uint32, NumCutoutVerticesPerFrame)
	SHADER_PARAMETER(uint32, NiagaraFloatDataOffset)
	SHADER_PARAMETER(uint32, NiagaraFloatDataStride)
	SHADER_PARAMETER(uint32, ParticleAlignmentMode)
	SHADER_PARAMETER(uint32, ParticleFacingMode)
	SHADER_PARAMETER(uint32, SortedIndicesOffset)
	SHADER_PARAMETER_SRV(Buffer<float2>, CutoutGeometry)
	SHADER_PARAMETER_SRV(Buffer<float>, NiagaraParticleDataFloat)
	SHADER_PARAMETER_SRV(Buffer<int>, SortedIndices)
	SHADER_PARAMETER_SRV(Buffer<uint>, IndirectArgsBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FNiagaraSpriteVFLooseParameters> FNiagaraSpriteVFLooseParametersRef;

class FNiagaraNullSubUVCutoutVertexBuffer : public FVertexBuffer
{
public:
	/**
	 * Initialize the RHI for this rendering resource
	 */
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector2D) * 4, BUF_Static | BUF_ShaderResource, CreateInfo, BufferData);
		FMemory::Memzero(BufferData, sizeof(FVector2D) * 4);
		RHIUnlockVertexBuffer(VertexBufferRHI);

		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(FVector2D), PF_G32R32F);
	}

	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}

	FShaderResourceViewRHIRef VertexBufferSRV;
};

extern NIAGARAVERTEXFACTORIES_API TGlobalResource<FNiagaraNullSubUVCutoutVertexBuffer> GFNiagaraNullSubUVCutoutVertexBuffer;

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
		LooseParameterUniformBuffer(nullptr),
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
		LooseParameterUniformBuffer(nullptr),
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
	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	
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

	FUniformBufferRHIRef LooseParameterUniformBuffer;

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
