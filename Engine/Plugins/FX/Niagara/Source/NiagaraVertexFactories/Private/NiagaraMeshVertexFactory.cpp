// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "NiagaraMeshVertexFactory.h"
#include "ParticleHelper.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNiagaraMeshUniformParameters,TEXT("NiagaraMeshVF"));

class FNiagaraMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		//PrevTransformBuffer.Bind(ParameterMap, TEXT("PrevTransformBuffer"));
		NiagaraParticleDataFloat.Bind(ParameterMap, TEXT("NiagaraParticleDataFloat"));
		FloatDataOffset.Bind(ParameterMap, TEXT("NiagaraFloatDataOffset"));
		FloatDataStride.Bind(ParameterMap, TEXT("NiagaraFloatDataStride"));

		// 		NiagaraParticleDataInt.Bind(ParameterMap, TEXT("NiagaraParticleDataInt"));
		// 		FloatDataOffset.Bind(ParameterMap, TEXT("NiagaraInt32DataOffset"));
		// 		FloatDataStride.Bind(ParameterMap, TEXT("NiagaraInt3DataStride"));

		MeshFacingMode.Bind(ParameterMap, TEXT("MeshFacingMode"));
		SortedIndices.Bind(ParameterMap, TEXT("SortedIndices"));
		SortedIndicesOffset.Bind(ParameterMap, TEXT("SortedIndicesOffset"));

	}

	virtual void Serialize(FArchive& Ar) override
	{
		//Ar << PrevTransformBuffer;
		Ar << NiagaraParticleDataFloat;
		Ar << FloatDataOffset;
		Ar << FloatDataStride;

		// 		Ar << NiagaraParticleDataInt;
		// 		Ar << Int32DataOffset;
		// 		Ar << Int32DataStride;

		Ar << MeshFacingMode;

		Ar << SortedIndices;
		Ar << SortedIndicesOffset;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		const bool bInstanced = GRHISupportsInstancing;
		FNiagaraMeshVertexFactory* NiagaraMeshVF = (FNiagaraMeshVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FNiagaraMeshUniformParameters>(), NiagaraMeshVF->GetUniformBuffer());

		//SetSRVParameter(RHICmdList, VertexShaderRHI, PrevTransformBuffer, NiagaraMeshVF->GetPreviousTransformBufferSRV());

		SetShaderValue(RHICmdList, VertexShaderRHI, MeshFacingMode, NiagaraMeshVF->GetMeshFacingMode());

		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataFloat, NiagaraMeshVF->GetParticleDataFloatSRV());
		SetShaderValue(RHICmdList, VertexShaderRHI, FloatDataOffset, NiagaraMeshVF->GetFloatDataOffset());
		SetShaderValue(RHICmdList, VertexShaderRHI, FloatDataStride, NiagaraMeshVF->GetFloatDataStride());

		//SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataInt, NiagaraMeshVF->GetIntDataSRV());

		SetSRVParameter(RHICmdList, VertexShaderRHI, SortedIndices, NiagaraMeshVF->GetSortedIndicesSRV() ? NiagaraMeshVF->GetSortedIndicesSRV() : GFNiagaraNullSortedIndicesVertexBuffer.VertexBufferSRV);
		SetShaderValue(RHICmdList, VertexShaderRHI, SortedIndicesOffset, NiagaraMeshVF->GetSortedIndicesOffset());
	}

private:

	//FShaderResourceParameter PrevTransformBuffer;

	FShaderResourceParameter NiagaraParticleDataFloat;
	FShaderParameter FloatDataOffset;
	FShaderParameter FloatDataStride;

	// 	FShaderResourceParameter NiagaraParticleDataInt;
	// 	FShaderParameter Int32DataOffset;
	// 	FShaderParameter Int32DataStride;

	FShaderParameter MeshFacingMode;
	FShaderResourceParameter SortedIndices;
	FShaderParameter SortedIndicesOffset;

};


void FNiagaraMeshVertexFactory::InitRHI()
{
	FVertexDeclarationElementList Elements;

	check(GRHISupportsInstancing);

	{
		if (Data.PositionComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
		}

		// only tangent,normal are used by the stream. the binormal is derived in the shader
		uint8 TangentBasisAttributes[2] = { 1, 2 };
		for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
		{
			if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
			{
				Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
			}
		}

		if (Data.ColorComponentsSRV == nullptr)
		{
			Data.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
			Data.ColorIndexMask = 0;
		}

		// Vertex color
		if (Data.ColorComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.ColorComponent, 3));
		}
		else
		{
			//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
			Elements.Add(AccessStreamComponent(NullColorComponent, 3));
		}

		if (Data.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}

			for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_TEXCOORDS; CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}
		}

		//if (Streams.Num() > 0)
		{
			InitDeclaration(Elements);
			check(IsValidRef(GetDeclaration()));
		}
	}
}

/*
uint8* FNiagaraMeshVertexFactory::LockPreviousTransformBuffer(uint32 ParticleCount)
{
	const static uint32 ElementSize = sizeof(FVector4);
	const static uint32 ParticleSize = ElementSize * 3;
	const uint32 AllocationRequest = ParticleCount * ParticleSize;

	check(!PrevTransformBuffer.MappedBuffer);

	if (AllocationRequest > PrevTransformBuffer.NumBytes)
	{
		PrevTransformBuffer.Release();
		PrevTransformBuffer.Initialize(ElementSize, ParticleCount * 3, PF_A32B32G32R32F, BUF_Dynamic);
	}

	PrevTransformBuffer.Lock();

	return PrevTransformBuffer.MappedBuffer;
}

void FNiagaraMeshVertexFactory::UnlockPreviousTransformBuffer()
{
	check(PrevTransformBuffer.MappedBuffer);

	PrevTransformBuffer.Unlock();
}

FShaderResourceViewRHIParamRef FNiagaraMeshVertexFactory::GetPreviousTransformBufferSRV() const
{
	return PrevTransformBuffer.SRV;
}
*/

bool FNiagaraMeshVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (!IsMobilePlatform(Platform) && Platform != SP_OPENGL_SM4 && (Material->IsUsedWithNiagaraMeshParticles() || Material->IsSpecialEngineMaterial()));
}

void FNiagaraMeshVertexFactory::SetData(const FStaticMeshDataType& InData)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI();
}


FVertexFactoryShaderParameters* FNiagaraMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FNiagaraMeshVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraMeshVertexFactory, "/Engine/Private/NiagaraMeshVertexFactory.ush", true, false, true, false, false);
IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraMeshVertexFactoryEmulatedInstancing, "/Engine/Private/NiagaraMeshVertexFactory.ush", true, false, true, false, false);

