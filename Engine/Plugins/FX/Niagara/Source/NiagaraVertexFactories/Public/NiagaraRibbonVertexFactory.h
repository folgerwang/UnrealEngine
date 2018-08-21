// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "NiagaraVertexFactory.h"
#include "SceneView.h"
#include "NiagaraDataSet.h"
#
class FMaterial;



//	FNiagaraRibbonVertex
struct FNiagaraRibbonVertex
{
	/** The index of the ribbon for multi-ribbon emitters */
	uint32 RibbonIndex;
};


//	FNiagaraRibbonVertexDynamicParameter
struct FNiagaraRibbonVertexDynamicParameter
{
	/** The dynamic parameter of the particle			*/
	float DynamicValue[4];
};

/**
* Uniform buffer for particle beam/trail vertex factories.
*/
BEGIN_UNIFORM_BUFFER_STRUCT(FNiagaraRibbonUniformParameters, NIAGARAVERTEXFACTORIES_API)
	UNIFORM_MEMBER(FVector4, CameraRight)
	UNIFORM_MEMBER(FVector4, CameraUp)
	UNIFORM_MEMBER(FVector4, ScreenAlignment)
	UNIFORM_MEMBER(int, PositionDataOffset)
	UNIFORM_MEMBER(int, VelocityDataOffset)
	UNIFORM_MEMBER(int, WidthDataOffset)
	UNIFORM_MEMBER(int, TwistDataOffset)
	UNIFORM_MEMBER(int, ColorDataOffset)
	UNIFORM_MEMBER(int, FacingDataOffset)
	UNIFORM_MEMBER(int, NormalizedAgeDataOffset)
	UNIFORM_MEMBER(int, MaterialRandomDataOffset)
	UNIFORM_MEMBER(int, MaterialParamDataOffset)
	UNIFORM_MEMBER(int, MaterialParam1DataOffset)
	UNIFORM_MEMBER(int, MaterialParam2DataOffset)
	UNIFORM_MEMBER(int, MaterialParam3DataOffset)
	UNIFORM_MEMBER(int, TotalNumInstances)
	UNIFORM_MEMBER(uint32, UseCustomFacing)
	UNIFORM_MEMBER(uint32, InvertDrawOrder)
	UNIFORM_MEMBER(float, UV0TilingDistance)
	UNIFORM_MEMBER(float, UV1TilingDistance)
	UNIFORM_MEMBER(FVector4, PackedVData)
	UNIFORM_MEMBER_EX(FMatrix, LocalToWorld, EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX(FMatrix, LocalToWorldInverseTransposed, EShaderPrecisionModifier::Half)
	UNIFORM_MEMBER_EX(float, DeltaSeconds, EShaderPrecisionModifier::Half)
	END_UNIFORM_BUFFER_STRUCT(FNiagaraRibbonUniformParameters)
typedef TUniformBufferRef<FNiagaraRibbonUniformParameters> FNiagaraRibbonUniformBufferRef;

/**
* Beam/Trail particle vertex factory.
*/
class NIAGARAVERTEXFACTORIES_API FNiagaraRibbonVertexFactory : public FNiagaraVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FNiagaraRibbonVertexFactory);

public:

	/** Default constructor. */
	FNiagaraRibbonVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel)
		: FNiagaraVertexFactoryBase(InType, InFeatureLevel)
		, IndexBuffer(nullptr)
		, FirstIndex(0)
		, OutTriangleCount(0)
		, DataSet(0)
	{}

	FNiagaraRibbonVertexFactory()
		: FNiagaraVertexFactoryBase(NVFT_MAX, ERHIFeatureLevel::Num)
		, IndexBuffer(nullptr)
		, FirstIndex(0)
		, OutTriangleCount(0)
		, DataSet(0)
	{}

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory?
	*/
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	// FRenderResource interface.
	virtual void InitRHI() override;

	/**
	* Set the uniform buffer for this vertex factory.
	*/
	FORCEINLINE void SetRibbonUniformBuffer(FNiagaraRibbonUniformBufferRef InSpriteUniformBuffer)
	{
		NiagaraRibbonUniformBuffer = InSpriteUniformBuffer;
	}

	/**
	* Retrieve the uniform buffer for this vertex factory.
	*/
	FORCEINLINE FNiagaraRibbonUniformBufferRef GetRibbonUniformBuffer()
	{
		return NiagaraRibbonUniformBuffer;
	}

	/**
	* Set the source vertex buffer.
	*/
	void SetVertexBuffer(const FVertexBuffer* InBuffer, uint32 StreamOffset, uint32 Stride);

	/**
	* Set the source vertex buffer that contains particle dynamic parameter data.
	*/
	void SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, int32 ParameterIndex, uint32 StreamOffset, uint32 Stride);


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

	void SetSegmentDistances(const FShaderResourceViewRHIRef& InSegmentDistancesSRV)
	{
		SegmentDistancesSRV = InSegmentDistancesSRV;
	}

	void SetPackedPerRibbonDataByIndexSRV(const FShaderResourceViewRHIRef& InPackedPerRibbonDataByIndexSRV)
	{
		PackedPerRibbonDataByIndexSRV = InPackedPerRibbonDataByIndexSRV;
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

	FORCEINLINE FShaderResourceViewRHIRef GetSegmentDistancesSRV()
	{
		return SegmentDistancesSRV;
	}

	FORCEINLINE FShaderResourceViewRHIRef GetPackedPerRibbonDataByIndexSRV()
	{
		return PackedPerRibbonDataByIndexSRV;
	}

	/**
	* Construct shader parameters for this type of vertex factory.
	*/
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FIndexBuffer*& GetIndexBuffer()
	{
		return IndexBuffer;
	}

	uint32& GetFirstIndex()
	{
		return FirstIndex;
	}

	int32& GetOutTriangleCount()
	{
		return OutTriangleCount;
	}

private:

	/** Uniform buffer with beam/trail parameters. */
	FNiagaraRibbonUniformBufferRef NiagaraRibbonUniformBuffer;

	/** Used to hold the index buffer allocation information when we call GDME more than once per frame. */
	FIndexBuffer* IndexBuffer;
	uint32 FirstIndex;
	int32 OutTriangleCount;

	const FNiagaraDataSet *DataSet;

	FShaderResourceViewRHIRef ParticleDataFloatSRV;
	uint32 FloatDataOffset;
	uint32 FloatDataStride;

	FShaderResourceViewRHIRef SortedIndicesSRV;
	FShaderResourceViewRHIRef SegmentDistancesSRV;
	FShaderResourceViewRHIRef PackedPerRibbonDataByIndexSRV;

	uint32 SortedIndicesOffset;

};