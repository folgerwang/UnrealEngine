// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "NiagaraVertexFactory.h"
#include "../../Niagara/Classes/NiagaraDataSet.h"
#include "SceneView.h"
#include "Components.h"
#include "SceneManagement.h"
#include "VertexFactory.h"



class FMaterial;
class FVertexBuffer;
struct FDynamicReadBuffer;
struct FShaderCompilerEnvironment;


/**
* Uniform buffer for mesh particle vertex factories.
*/
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNiagaraMeshUniformParameters, NIAGARAVERTEXFACTORIES_API)
	SHADER_PARAMETER_EX( FMatrix, LocalToWorld, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX( FMatrix, LocalToWorldInverseTransposed, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER(FVector4, SubImageSize)
	SHADER_PARAMETER(uint32, TexCoordWeightA)
	SHADER_PARAMETER(uint32, TexCoordWeightB)
	SHADER_PARAMETER(uint32, PrevTransformAvailable)
	SHADER_PARAMETER(float, DeltaSeconds)
	SHADER_PARAMETER(int, PositionDataOffset)
	SHADER_PARAMETER(int, VelocityDataOffset)
	SHADER_PARAMETER(int, ColorDataOffset)
	SHADER_PARAMETER(int, TransformDataOffset)
	SHADER_PARAMETER(int, ScaleDataOffset)
	SHADER_PARAMETER(int, SizeDataOffset)
	SHADER_PARAMETER(int, MaterialParamDataOffset)
	SHADER_PARAMETER(int, MaterialParam1DataOffset)
	SHADER_PARAMETER(int, MaterialParam2DataOffset)
	SHADER_PARAMETER(int, MaterialParam3DataOffset)
	SHADER_PARAMETER(int, NormalizedAgeDataOffset)
	SHADER_PARAMETER(int, MaterialRandomDataOffset)
	SHADER_PARAMETER(FVector4, DefaultPos)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FNiagaraMeshUniformParameters> FNiagaraMeshUniformBufferRef;

class FNiagaraMeshInstanceVertices;


/**
* Vertex factory for rendering instanced mesh particles with out dynamic parameter support.
*/
class NIAGARAVERTEXFACTORIES_API FNiagaraMeshVertexFactory : public FNiagaraVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FNiagaraMeshVertexFactory);
public:
	
	/** Default constructor. */
	FNiagaraMeshVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel)
		: FNiagaraVertexFactoryBase(InType, InFeatureLevel)
		, MeshFacingMode(0)
		, InstanceVerticesCPU(nullptr)
		, FloatDataOffset(0)
		, FloatDataStride(0)
		, SortedIndicesOffset(0)
	{}

	FNiagaraMeshVertexFactory()
		: FNiagaraVertexFactoryBase(NVFT_MAX, ERHIFeatureLevel::Num)
		, MeshFacingMode(0)
		, InstanceVerticesCPU(nullptr)
		, FloatDataOffset(0)
		, FloatDataStride(0)
		, SortedIndicesOffset(0)
	{}

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory?
	*/
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);


	/**
	* Modify compile environment to enable instancing
	* @param OutEnvironment - shader compile environment to modify
	*/
	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNiagaraVertexFactoryBase::ModifyCompilationEnvironment(Type, Platform, Material, OutEnvironment);

		// Set a define so we can tell in MaterialTemplate.usf when we are compiling a mesh particle vertex factory
		OutEnvironment.SetDefine(TEXT("NIAGARA_MESH_FACTORY"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("NIAGARA_MESH_INSTANCED"), TEXT("1"));
	}

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

	/**
	* An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	*/
	void SetData(const FStaticMeshDataType& InData);

	/**
	* Set the uniform buffer for this vertex factory.
	*/
	FORCEINLINE void SetUniformBuffer(const FNiagaraMeshUniformBufferRef& InMeshParticleUniformBuffer)
	{
		MeshParticleUniformBuffer = InMeshParticleUniformBuffer;
	}

	/**
	* Retrieve the uniform buffer for this vertex factory.
	*/
	FORCEINLINE FUniformBufferRHIParamRef GetUniformBuffer()
	{
		return MeshParticleUniformBuffer;
	}
	
	//uint8* LockPreviousTransformBuffer(uint32 ParticleCount);
	//void UnlockPreviousTransformBuffer();
	//FShaderResourceViewRHIParamRef GetPreviousTransformBufferSRV() const;

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FNiagaraMeshVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
	
	uint32 GetMeshFacingMode() const
	{
		return MeshFacingMode;
	}

	void SetMeshFacingMode(uint32 InMode)
	{
		MeshFacingMode = InMode;
	}

protected:
	FStaticMeshDataType Data;
	uint32 MeshFacingMode;

	/** Uniform buffer with mesh particle parameters. */
	FUniformBufferRHIParamRef MeshParticleUniformBuffer;
	
	/** Used to remember this in the case that we reuse the same vertex factory for multiple renders . */
	FNiagaraMeshInstanceVertices* InstanceVerticesCPU;

	FShaderResourceViewRHIRef ParticleDataFloatSRV;
	uint32 FloatDataOffset;
	uint32 FloatDataStride;

	FShaderResourceViewRHIRef SortedIndicesSRV;
	uint32 SortedIndicesOffset;
};


class NIAGARAVERTEXFACTORIES_API FNiagaraMeshVertexFactoryEmulatedInstancing : public FNiagaraMeshVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactoryEmulatedInstancing);

public:
	FNiagaraMeshVertexFactoryEmulatedInstancing(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel)
		: FNiagaraMeshVertexFactory(InType, InFeatureLevel)
	{}

	FNiagaraMeshVertexFactoryEmulatedInstancing()
		: FNiagaraMeshVertexFactory()
	{}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Platform == SP_OPENGL_ES2_ANDROID || Platform == SP_OPENGL_ES2_WEBGL) // Those are only platforms that might not support hardware instancing
			&& FNiagaraMeshVertexFactory::ShouldCompilePermutation(Platform, Material, ShaderType);
	}

	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNiagaraMeshVertexFactory::ModifyCompilationEnvironment(Type, Platform, Material, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_INSTANCED"), TEXT("0"));
	}
};

inline FNiagaraMeshVertexFactory* ConstructNiagaraMeshVertexFactory()
{
	if (GRHISupportsInstancing)
	{
		return new FNiagaraMeshVertexFactory();
	}
	else
	{
		return new FNiagaraMeshVertexFactoryEmulatedInstancing();
	}
}

inline FNiagaraMeshVertexFactory* ConstructNiagaraMeshVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel)
{
	if (GRHISupportsInstancing)
	{
		return new FNiagaraMeshVertexFactory(InType, InFeatureLevel);
	}
	else
	{
		return new FNiagaraMeshVertexFactoryEmulatedInstancing(InType, InFeatureLevel);
	}
}
