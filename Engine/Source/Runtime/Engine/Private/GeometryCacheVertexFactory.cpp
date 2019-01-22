// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCacheVertexFactory.cpp: Geometry Cache vertex factory implementation
=============================================================================*/

#include "GeometryCacheVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "GPUSkinCache.h"
#include "ShaderParameterUtils.h"
#include "MeshMaterialShader.h"

/*-----------------------------------------------------------------------------
FGeometryCacheVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCacheVertexFactoryUniformBufferParameters, "GeomCache");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCacheManualVertexFetchUniformBufferParameters, "GeomCacheMVF");

/** Shader parameters for use with TGPUSkinVertexFactory */
class FGeometryCacheVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		MeshOrigin.Bind(ParameterMap, TEXT("MeshOrigin"));
		MeshExtension.Bind(ParameterMap, TEXT("MeshExtension"));
		MotionBlurDataOrigin.Bind(ParameterMap, TEXT("MotionBlurDataOrigin"));
		MotionBlurDataExtension.Bind(ParameterMap, TEXT("MotionBlurDataExtension"));
		MotionBlurPositionScale.Bind(ParameterMap, TEXT("MotionBlurPositionScale"));
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{
		Ar << MeshOrigin;
		Ar << MeshExtension;
		Ar << MotionBlurDataOrigin;
		Ar << MotionBlurDataExtension;
		Ar << MotionBlurPositionScale;
	}

	virtual void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		bool bShaderRequiresPositionOnlyStream,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* GenericVertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const override
	{
		// Ensure the vertex factory matches this parameter object and cast relevant objects
		check(GenericVertexFactory->GetType() == &FGeometryCacheVertexVertexFactory::StaticType);
		const FGeometryCacheVertexVertexFactory* GCVertexFactory = static_cast<const FGeometryCacheVertexVertexFactory*>(GenericVertexFactory);

		FGeometryCacheVertexFactoryUserData* BatchData = (FGeometryCacheVertexFactoryUserData*)BatchElement.VertexFactoryUserData;

		// Check the passed in vertex buffers make sense
		checkf(BatchData->PositionBuffer->IsInitialized(), TEXT("Batch position Vertex buffer was not initialized! Name %s"), *BatchData->PositionBuffer->GetFriendlyName());
		checkf(BatchData->MotionBlurDataBuffer->IsInitialized(), TEXT("Batch motion blur data buffer was not initialized! Name %s"), *BatchData->MotionBlurDataBuffer->GetFriendlyName());

		VertexStreams.Add(FVertexInputStream(GCVertexFactory->PositionStreamIndex, 0, BatchData->PositionBuffer->VertexBufferRHI));
		VertexStreams.Add(FVertexInputStream(GCVertexFactory->MotionBlurDataStreamIndex, 0, BatchData->MotionBlurDataBuffer->VertexBufferRHI));

		ShaderBindings.Add(MeshOrigin, BatchData->MeshOrigin);
		ShaderBindings.Add(MeshExtension, BatchData->MeshExtension);
		ShaderBindings.Add(MotionBlurDataOrigin, BatchData->MotionBlurDataOrigin);
		ShaderBindings.Add(MotionBlurDataExtension, BatchData->MotionBlurDataExtension);
		ShaderBindings.Add(MotionBlurPositionScale, BatchData->MotionBlurPositionScale);

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGeometryCacheVertexFactoryUniformBufferParameters>(), BatchData->UniformBuffer);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGeometryCacheManualVertexFetchUniformBufferParameters>(), BatchData->ManualVertexFetchUniformBuffer);
	}

	virtual uint32 GetSize() const override { return sizeof(*this); }

private:
	FShaderParameter MeshOrigin;
	FShaderParameter MeshExtension;
	FShaderParameter MotionBlurDataOrigin;
	FShaderParameter MotionBlurDataExtension;
	FShaderParameter MotionBlurPositionScale;
};

/*-----------------------------------------------------------------------------
FGPUSkinPassthroughVertexFactory
-----------------------------------------------------------------------------*/
void FGeometryCacheVertexVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	Super::ModifyCompilationEnvironment(Type, Platform, Material, OutEnvironment);
}

bool FGeometryCacheVertexVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType)
{
	//FIXME: This can probably be beter like this?!?
	//return Material->IsUsedWithGeometryCache() || Material->IsSpecialEngineMaterial();

	return true;
}

void FGeometryCacheVertexVertexFactory::SetData(const FDataType& InData)
{
	check(IsInRenderingThread());

	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES2 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	Data = InData;
	// This will call InitRHI below where the real action happens
	UpdateRHI();
}

class FDefaultGeometryCacheVertexBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector4) * 2, BUF_Static | BUF_ShaderResource, CreateInfo, BufferData);
		FVector4* DummyContents = (FVector4*)BufferData;
		DummyContents[0] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		RHIUnlockVertexBuffer(VertexBufferRHI);

		SRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
	}

	virtual void ReleaseRHI() override
	{
		SRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};
TGlobalResource<FDefaultGeometryCacheVertexBuffer> GDefaultGeometryCacheVertexBuffer;

class FDummyTangentBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector4) * 2, BUF_Static | BUF_ShaderResource, CreateInfo, BufferData);
		FVector4* DummyContents = (FVector4*)BufferData;
		DummyContents[0] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		RHIUnlockVertexBuffer(VertexBufferRHI);

		SRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
	}

	virtual void ReleaseRHI() override
	{
		SRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};
TGlobalResource<FDummyTangentBuffer> GDummyTangentBuffer;

void FGeometryCacheVertexVertexFactory::InitRHI()
{
	// Position needs to be separate from the rest (we just theck tangents here)
	check(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer);
	// Motion Blur data also needs to be separate from the rest
	check(Data.MotionBlurDataComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer);
	check(Data.MotionBlurDataComponent.VertexBuffer != Data.PositionComponent.VertexBuffer);

	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if (Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		FVertexDeclarationElementList PositionOnlyStreamElements;
		PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.PositionComponent, 0));
		InitPositionDeclaration(PositionOnlyStreamElements);
	}

	FVertexDeclarationElementList Elements;
	if (Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
		PositionStreamIndex = Elements.Last().StreamIndex;
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

	if (Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent, 3));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.Add(AccessStreamComponent(NullColorComponent, 3));
	}

	if (Data.MotionBlurDataComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.MotionBlurDataComponent, 4));
	}
	else if (Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 4));
	}
	MotionBlurDataStreamIndex = Elements.Last().StreamIndex;

	if (Data.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 5;
		for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}

		for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}
	}

	check(Streams.Num() > 0);
	check(PositionStreamIndex >= 0);
	check(MotionBlurDataStreamIndex >= 0);
	check(MotionBlurDataStreamIndex != PositionStreamIndex);

	InitDeclaration(Elements);

	check(IsValidRef(GetDeclaration()));
}

void FGeometryCacheVertexVertexFactory::CreateManualVertexFetchUniformBuffer(
	const FVertexBuffer* PoistionBuffer, 
	const FVertexBuffer* MotionBlurBuffer,
	FGeometryCacheVertexFactoryUserData& OutUserData) const
{
	FGeometryCacheManualVertexFetchUniformBufferParameters ManualVertexFetchParameters;

	if (PoistionBuffer != NULL)
	{
		OutUserData.PositionSRV = RHICreateShaderResourceView(PoistionBuffer->VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		// Position will need per-component fetch since we don't have R32G32B32 pixel format
		ManualVertexFetchParameters.Position = OutUserData.PositionSRV;
	}
	else
	{
		ManualVertexFetchParameters.Position = GDefaultGeometryCacheVertexBuffer.SRV;
	}

	if (Data.TangentBasisComponents[0].VertexBuffer != NULL)
	{
		OutUserData.TangentXSRV = RHICreateShaderResourceView(Data.TangentBasisComponents[0].VertexBuffer->VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
		ManualVertexFetchParameters.TangentX = OutUserData.TangentXSRV;
	}
	else
	{
		ManualVertexFetchParameters.TangentX = GDummyTangentBuffer.SRV;
	}

	if (Data.TangentBasisComponents[1].VertexBuffer != NULL)
	{
		OutUserData.TangentZSRV = RHICreateShaderResourceView(Data.TangentBasisComponents[1].VertexBuffer->VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
		ManualVertexFetchParameters.TangentZ = OutUserData.TangentZSRV;
	}
	else
	{
		ManualVertexFetchParameters.TangentZ = GDummyTangentBuffer.SRV;
	}

	if (Data.ColorComponent.VertexBuffer)
	{
		OutUserData.ColorSRV = RHICreateShaderResourceView(Data.ColorComponent.VertexBuffer->VertexBufferRHI, sizeof(FColor), PF_B8G8R8A8);
		ManualVertexFetchParameters.Color = OutUserData.ColorSRV;
	}
	else
	{
		OutUserData.ColorSRV = GNullColorVertexBuffer.VertexBufferSRV;
		ManualVertexFetchParameters.Color = OutUserData.ColorSRV;
	}

	if (MotionBlurBuffer)
	{
		OutUserData.MotionBlurDataSRV = RHICreateShaderResourceView(MotionBlurBuffer->VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		ManualVertexFetchParameters.MotionBlurData = OutUserData.MotionBlurDataSRV;
	}
	else if (PoistionBuffer != NULL)
	{
		ManualVertexFetchParameters.MotionBlurData = OutUserData.PositionSRV;
	}
	else
	{
		ManualVertexFetchParameters.MotionBlurData = GDefaultGeometryCacheVertexBuffer.SRV;
	}

	if (Data.TextureCoordinates.Num())
	{
		checkf(Data.TextureCoordinates.Num() <= 1, TEXT("We're assuming FGeometryCacheSceneProxy uses only one TextureCoordinates vertex buffer"));
		OutUserData.TexCoordsSRV = RHICreateShaderResourceView(Data.TextureCoordinates[0].VertexBuffer->VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		// TexCoords will need per-component fetch since we don't have R32G32 pixel format
		ManualVertexFetchParameters.TexCoords = OutUserData.TexCoordsSRV;
	}
	else
	{
		ManualVertexFetchParameters.TexCoords = GDefaultGeometryCacheVertexBuffer.SRV;
	}

	OutUserData.ManualVertexFetchUniformBuffer = FGeometryCacheManualVertexFetchUniformBufferParametersRef::CreateUniformBufferImmediate(ManualVertexFetchParameters, UniformBuffer_SingleFrame);
}

FVertexFactoryShaderParameters* FGeometryCacheVertexVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	switch (ShaderFrequency)
	{
		case SF_Vertex:
#if RHI_RAYTRACING
		case SF_RayHitGroup:
#endif
			return new FGeometryCacheVertexFactoryShaderParameters();
		default:
			return nullptr;
	}
}

bool FGeometryCacheVertexVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	// Should this be platform or mesh type based? Returning true should work in all cases, but maybe too expensive? 
	// return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
	// TODO currently GeomCache supports only 4 UVs which could cause compilation errors when trying to compile shaders which use > 4
	return Material->IsUsedWithGeometryCache() || Material->IsSpecialEngineMaterial();
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FGeometryCacheVertexVertexFactory, "/Engine/Private/GeometryCacheVertexFactory.ush", true, false, true, false, true);
