// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "NiagaraSpriteVertexFactory.h"
#include "ParticleHelper.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNiagaraSpriteUniformParameters,TEXT("NiagaraSpriteVF"));

TGlobalResource<FNullDynamicParameterVertexBuffer> GNullNiagaraDynamicParameterVertexBuffer;

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
TGlobalResource<FNiagaraNullSubUVCutoutVertexBuffer> GFNiagaraNullSubUVCutoutVertexBuffer;

/**
 * Shader parameters for the particle vertex factory.
 */
class FNiagaraSpriteVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}
};

class FNiagaraSpriteVertexFactoryShaderParametersVS : public FNiagaraSpriteVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		NumCutoutVerticesPerFrame.Bind(ParameterMap, TEXT("NumCutoutVerticesPerFrame"));
		CutoutGeometry.Bind(ParameterMap, TEXT("CutoutGeometry"));

		NiagaraParticleDataFloat.Bind(ParameterMap, TEXT("NiagaraParticleDataFloat"));
		FloatDataOffset.Bind(ParameterMap, TEXT("NiagaraFloatDataOffset"));
		FloatDataStride.Bind(ParameterMap, TEXT("NiagaraFloatDataStride"));

//  		NiagaraParticleDataInt.Bind(ParameterMap, TEXT("NiagaraParticleDataInt"));
//  		Int32DataOffset.Bind(ParameterMap, TEXT("NiagaraInt32DataOffset"));
//  		Int32DataStride.Bind(ParameterMap, TEXT("NiagaraInt3DataStride"));

		ParticleAlignmentMode.Bind(ParameterMap, TEXT("ParticleAlignmentMode"));
		ParticleFacingMode.Bind(ParameterMap, TEXT("ParticleFacingMode"));

		SortedIndices.Bind(ParameterMap, TEXT("SortedIndices"));
		SortedIndicesOffset.Bind(ParameterMap, TEXT("SortedIndicesOffset"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << NumCutoutVerticesPerFrame;
		Ar << CutoutGeometry;
		Ar << ParticleFacingMode;
		Ar << ParticleAlignmentMode;

		Ar << NiagaraParticleDataFloat;
		Ar << FloatDataOffset;
		Ar << FloatDataStride;

//  		Ar << NiagaraParticleDataInt;
//  		Ar << Int32DataOffset;
//  		Ar << Int32DataStride;

		Ar << SortedIndices;
		Ar << SortedIndicesOffset;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FNiagaraSpriteVertexFactory* SpriteVF = (FNiagaraSpriteVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FNiagaraSpriteUniformParameters>(), SpriteVF->GetSpriteUniformBuffer() );
		
		SetShaderValue(RHICmdList, VertexShaderRHI, NumCutoutVerticesPerFrame, SpriteVF->GetNumCutoutVerticesPerFrame());
		FShaderResourceViewRHIParamRef NullSRV = GFNiagaraNullSubUVCutoutVertexBuffer.VertexBufferSRV;
		SetSRVParameter(RHICmdList, VertexShaderRHI, CutoutGeometry, SpriteVF->GetCutoutGeometrySRV() ? SpriteVF->GetCutoutGeometrySRV() : NullSRV);

		SetShaderValue(RHICmdList, VertexShaderRHI, ParticleAlignmentMode, SpriteVF->GetAlignmentMode());
		SetShaderValue(RHICmdList, VertexShaderRHI, ParticleFacingMode, SpriteVF->GetFacingMode());
		
		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataFloat, SpriteVF->GetParticleDataFloatSRV());
		SetShaderValue(RHICmdList, VertexShaderRHI, FloatDataOffset, SpriteVF->GetFloatDataOffset());
		SetShaderValue(RHICmdList, VertexShaderRHI, FloatDataStride, SpriteVF->GetFloatDataStride());

		SetSRVParameter(RHICmdList, VertexShaderRHI, SortedIndices, SpriteVF->GetSortedIndicesSRV() ? SpriteVF->GetSortedIndicesSRV() : GFNiagaraNullSortedIndicesVertexBuffer.VertexBufferSRV);
		SetShaderValue(RHICmdList, VertexShaderRHI, SortedIndicesOffset, SpriteVF->GetSortedIndicesOffset());
	}

private:
	FShaderParameter NumCutoutVerticesPerFrame;

	FShaderParameter ParticleAlignmentMode;
	FShaderParameter ParticleFacingMode;

	FShaderResourceParameter CutoutGeometry;

	FShaderResourceParameter NiagaraParticleDataFloat;
	FShaderParameter FloatDataOffset;
	FShaderParameter FloatDataStride;

//  	FShaderResourceParameter NiagaraParticleDataInt;
//  	FShaderParameter Int32DataOffset;
//  	FShaderParameter Int32DataStride;
	
	FShaderResourceParameter SortedIndices;
	FShaderParameter SortedIndicesOffset;
};

class FNiagaraSpriteVertexFactoryShaderParametersPS : public FNiagaraSpriteVertexFactoryShaderParameters
{
public:

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FNiagaraSpriteVertexFactory* SpriteVF = (FNiagaraSpriteVertexFactory*)VertexFactory;
		FPixelShaderRHIParamRef PixelShaderRHI = Shader->GetPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, Shader->GetUniformBufferParameter<FNiagaraSpriteUniformParameters>(), SpriteVF->GetSpriteUniformBuffer() );
	}
};

/**
 * The particle system vertex declaration resource type.
 */
class FNiagaraSpriteVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Constructor.
	FNiagaraSpriteVertexDeclaration(bool bInInstanced, int32 InNumVertsInInstanceBuffer) :
		bInstanced(bInInstanced),
		NumVertsInInstanceBuffer(InNumVertsInInstanceBuffer)
	{

	}

	// Destructor.
	virtual ~FNiagaraSpriteVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, int32& Offset)
	{
		uint32 InitialStride = sizeof(float) * 2;
		/** The stream to read the texture coordinates from. */
		check( Offset == 0 );
		Elements.Add(FVertexElement(0, Offset, VET_Float2, 0, InitialStride, false));
	}

	virtual void InitDynamicRHI()
	{
		FVertexDeclarationElementList Elements;
		int32	Offset = 0;

		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		// This is done in InitDynamicRHI instead of InitRHI to allow FParticleSpriteVertexFactory::InitRHI
		// to rely on it being initialized, since InitDynamicRHI is called before InitRHI.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseDynamicRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}

private:

	bool bInstanced;
	int32 NumVertsInInstanceBuffer;
};

/** The simple element vertex declaration. */
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteVertexDeclarationInstanced(true, 4);
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteEightVertexDeclarationInstanced(true, 8);
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteVertexDeclarationNonInstanced(false, 4);
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteEightVertexDeclarationNonInstanced(false, 8);

inline TGlobalResource<FNiagaraSpriteVertexDeclaration>& GetNiagaraSpriteVertexDeclaration(bool SupportsInstancing, int32 NumVertsInInstanceBuffer)
{
	check(NumVertsInInstanceBuffer == 4 || NumVertsInInstanceBuffer == 8);
	if (SupportsInstancing)
	{
		return NumVertsInInstanceBuffer == 4 ? GParticleSpriteVertexDeclarationInstanced : GParticleSpriteEightVertexDeclarationInstanced;
	}
	else
	{
		return NumVertsInInstanceBuffer == 4 ? GParticleSpriteVertexDeclarationNonInstanced : GParticleSpriteEightVertexDeclarationNonInstanced;
	}
}

bool FNiagaraSpriteVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Platform != SP_OPENGL_SM4 && (Material->IsUsedWithNiagaraSprites() || Material->IsSpecialEngineMaterial()));
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FNiagaraSpriteVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FNiagaraVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

	// Set a define so we can tell in MaterialTemplate.usf when we are compiling a sprite vertex factory
	OutEnvironment.SetDefine(TEXT("PARTICLE_SPRITE_FACTORY"),TEXT("1"));
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FNiagaraSpriteVertexFactory::InitRHI()
{
	InitStreams();
	SetDeclaration(GetNiagaraSpriteVertexDeclaration(GRHISupportsInstancing, NumVertsInInstanceBuffer).VertexDeclarationRHI);
}

void FNiagaraSpriteVertexFactory::InitStreams()
{
	const bool bInstanced = GRHISupportsInstancing;

	check(Streams.Num() == 0);
	if(bInstanced) 
	{
		FVertexStream* TexCoordStream = new(Streams) FVertexStream;
		TexCoordStream->VertexBuffer = &GParticleTexCoordVertexBuffer;
		TexCoordStream->Stride = sizeof(FVector2D);
		TexCoordStream->Offset = 0;
	}
}

void FNiagaraSpriteVertexFactory::SetTexCoordBuffer(const FVertexBuffer* InTexCoordBuffer)
{
	FVertexStream& TexCoordStream = Streams[0];
	TexCoordStream.VertexBuffer = InTexCoordBuffer;
}

FVertexFactoryShaderParameters* FNiagaraSpriteVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FNiagaraSpriteVertexFactoryShaderParametersVS();
	}
	else if (ShaderFrequency == SF_Pixel)
	{
		return new FNiagaraSpriteVertexFactoryShaderParametersPS();
	}
	return NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraSpriteVertexFactory,"/Engine/Private/NiagaraSpriteVertexFactory.ush",true,false,true,false,false);
