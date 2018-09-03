// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "NiagaraRibbonVertexFactory.h"
#include "ParticleHelper.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNiagaraRibbonUniformParameters,TEXT("NiagaraRibbonVF"));


class FNiagaraRibbonVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}
};

/**
* Shader parameters for the beam/trail vertex factory.
*/
class FNiagaraRibbonVertexFactoryShaderParametersVS : public FNiagaraRibbonVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		NiagaraParticleDataFloat.Bind(ParameterMap, TEXT("NiagaraParticleDataFloat"));
		FloatDataOffset.Bind(ParameterMap, TEXT("NiagaraFloatDataOffset"));
		FloatDataStride.Bind(ParameterMap, TEXT("NiagaraFloatDataStride"));
		SortedIndices.Bind(ParameterMap, TEXT("SortedIndices"));
		SortedIndicesOffset.Bind(ParameterMap, TEXT("SortedIndicesOffset"));
		SegmentDistances.Bind(ParameterMap, TEXT("SegmentDistances"));
		PackedPerRibbonDataByIndex.Bind(ParameterMap, TEXT("PackedPerRibbonDataByIndex"));

		ensure(NiagaraParticleDataFloat.IsBound());
		ensure(FloatDataOffset.IsBound());
		ensure(FloatDataStride.IsBound());
		ensure(SortedIndices.IsBound());
		ensure(SortedIndicesOffset.IsBound());
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << NiagaraParticleDataFloat;
		Ar << FloatDataOffset;
		Ar << FloatDataStride;
		Ar << SortedIndices;
		Ar << SortedIndicesOffset;
		Ar << SegmentDistances;
		Ar << PackedPerRibbonDataByIndex;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		FNiagaraRibbonVertexFactory* RibbonVF = (FNiagaraRibbonVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, Shader->GetVertexShader(), Shader->GetUniformBufferParameter<FNiagaraRibbonUniformParameters>(), RibbonVF->GetRibbonUniformBuffer());

		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataFloat, RibbonVF->GetParticleDataFloatSRV());
		SetShaderValue(RHICmdList, VertexShaderRHI, FloatDataOffset, RibbonVF->GetFloatDataOffset());
		SetShaderValue(RHICmdList, VertexShaderRHI, FloatDataStride, RibbonVF->GetFloatDataStride());

		SetSRVParameter(RHICmdList, VertexShaderRHI, SortedIndices, RibbonVF->GetSortedIndicesSRV());
		SetSRVParameter(RHICmdList, VertexShaderRHI, SegmentDistances, RibbonVF->GetSegmentDistancesSRV());
		SetSRVParameter(RHICmdList, VertexShaderRHI, PackedPerRibbonDataByIndex, RibbonVF->GetPackedPerRibbonDataByIndexSRV());
		SetShaderValue(RHICmdList, VertexShaderRHI, SortedIndicesOffset, RibbonVF->GetSortedIndicesOffset());
	}

private:
	FShaderResourceParameter NiagaraParticleDataFloat;
	FShaderParameter FloatDataOffset;
	FShaderParameter FloatDataStride;

	FShaderResourceParameter SortedIndices;
	FShaderResourceParameter SegmentDistances;
	FShaderResourceParameter PackedPerRibbonDataByIndex;
	FShaderParameter SortedIndicesOffset;
};



/**
* Shader parameters for the beam/trail vertex factory.
*/
class FNiagaraRibbonVertexFactoryShaderParametersPS : public FNiagaraRibbonVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		FNiagaraRibbonVertexFactory* RibbonVF = (FNiagaraRibbonVertexFactory*)VertexFactory;
		SetUniformBufferParameter(RHICmdList, Shader->GetPixelShader(), Shader->GetUniformBufferParameter<FNiagaraRibbonUniformParameters>(), RibbonVF->GetRibbonUniformBuffer());
	}
};


///////////////////////////////////////////////////////////////////////////////
/**
* The Niagara ribbon vertex declaration resource type.
*/
class FNiagaraRibbonVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FNiagaraRibbonVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, int32& Offset)
	{
		uint32 Stride = sizeof(FNiagaraRibbonVertex);
		/** The stream to read the ribbon index from. */
		Elements.Add(FVertexElement(0, Offset, VET_UByte4, 0, Stride));
		Offset += sizeof(int32);
	}

	virtual void InitDynamicRHI()
	{
		FVertexDeclarationElementList Elements;
		int32	Offset = 0;
		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		// This is done in InitDynamicRHI instead of InitRHI to allow FNiagaraRibbonVertexFactory::InitRHI
		// to rely on it being initialized, since InitDynamicRHI is called before InitRHI.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseDynamicRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FNiagaraRibbonVertexDeclaration> GNiagaraRibbonVertexDeclaration;

///////////////////////////////////////////////////////////////////////////////

bool FNiagaraRibbonVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (!IsMobilePlatform(Platform) && Platform != SP_OPENGL_SM4 && (Material->IsUsedWithNiagaraRibbons() || Material->IsSpecialEngineMaterial()));
}

/**
* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
*/
void FNiagaraRibbonVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FNiagaraVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("NIAGARA_RIBBON_FACTORY"), TEXT("1"));
}

/**
*	Initialize the Render Hardware Interface for this vertex factory
*/
void FNiagaraRibbonVertexFactory::InitRHI()
{
	SetDeclaration(GNiagaraRibbonVertexDeclaration.VertexDeclarationRHI);

	FVertexStream* VertexStream = new(Streams) FVertexStream;
	FVertexStream* DynamicParameterStream = new(Streams) FVertexStream;
	FVertexStream* DynamicParameter1Stream = new(Streams) FVertexStream;
	FVertexStream* DynamicParameter2Stream = new(Streams) FVertexStream;
	FVertexStream* DynamicParameter3Stream = new(Streams) FVertexStream;
}

FVertexFactoryShaderParameters* FNiagaraRibbonVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FNiagaraRibbonVertexFactoryShaderParametersVS();
	}
	else if (ShaderFrequency == SF_Pixel)
	{
		return new FNiagaraRibbonVertexFactoryShaderParametersPS();
	}
	return NULL;
}

void FNiagaraRibbonVertexFactory::SetVertexBuffer(const FVertexBuffer* InBuffer, uint32 StreamOffset, uint32 Stride)
{
	check(Streams.Num() == 5);
	FVertexStream& VertexStream = Streams[0];
	VertexStream.VertexBuffer = InBuffer;
	VertexStream.Stride = Stride;
	VertexStream.Offset = StreamOffset;
}

void FNiagaraRibbonVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, int32 ParameterIndex, uint32 StreamOffset, uint32 Stride)
{
	check(Streams.Num() == 5);
	FVertexStream& DynamicParameterStream = Streams[1 + ParameterIndex];
	if (InDynamicParameterBuffer)
	{
		DynamicParameterStream.VertexBuffer = InDynamicParameterBuffer;
		DynamicParameterStream.Stride = Stride;
		DynamicParameterStream.Offset = StreamOffset;
	}
	else
	{
		DynamicParameterStream.VertexBuffer = &GNullDynamicParameterVertexBuffer;
		DynamicParameterStream.Stride = 0;
		DynamicParameterStream.Offset = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraRibbonVertexFactory, "/Engine/Private/NiagaraRibbonVertexFactory.ush", true, false, true, false, false);
