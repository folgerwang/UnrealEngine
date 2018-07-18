// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCacheVertexFactory.cpp: Geometry Cache vertex factory implementation
=============================================================================*/

#include "GeometryCacheVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "GPUSkinCache.h"
#include "ShaderParameterUtils.h"

/*-----------------------------------------------------------------------------
FGeometryCacheVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

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

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* GenericVertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		// Ensure the vertex factory matches this parameter object and cast relevant objects
		check(GenericVertexFactory->GetType() == &FGeometryCacheVertexVertexFactory::StaticType);
		const FGeometryCacheVertexVertexFactory* GCVertexFactory = static_cast<const FGeometryCacheVertexVertexFactory*>(GenericVertexFactory);


		FVertexShaderRHIParamRef VS = Shader->GetVertexShader();

		FGeometryCacheVertexFactoryUserData* BatchData = (FGeometryCacheVertexFactoryUserData*)BatchElement.VertexFactoryUserData;

		// Check the passed in vertex buffers make sense
		checkf(BatchData->PositionBuffer->IsInitialized(), TEXT("Batch position Vertex buffer was not initialized! Name %s"), *BatchData->PositionBuffer->GetFriendlyName());
		checkf(BatchData->MotionBlurDataBuffer->IsInitialized(), TEXT("Batch motion blur data buffer was not initialized! Name %s"), *BatchData->MotionBlurDataBuffer->GetFriendlyName());


		RHICmdList.SetStreamSource(GCVertexFactory->PositionStreamIndex, BatchData->PositionBuffer->VertexBufferRHI, 0);
		RHICmdList.SetStreamSource(GCVertexFactory->MotionBlurDataStreamIndex, BatchData->MotionBlurDataBuffer->VertexBufferRHI, 0);

		if (VS)
		{
			SetShaderValue(RHICmdList, VS, MeshOrigin, BatchData->MeshOrigin);
			SetShaderValue(RHICmdList, VS, MeshExtension, BatchData->MeshExtension);
			SetShaderValue(RHICmdList, VS, MotionBlurDataOrigin, BatchData->MotionBlurDataOrigin);
			SetShaderValue(RHICmdList, VS, MotionBlurDataExtension, BatchData->MotionBlurDataExtension);
			SetShaderValue(RHICmdList, VS, MotionBlurPositionScale, BatchData->MotionBlurPositionScale);
		}
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
void FGeometryCacheVertexVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
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

void FGeometryCacheVertexVertexFactory::InitRHI()
{
	// Position needs to be separate from the rest (we just theck tangents here)
	check(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
		// Motion Blur data also needs to be separate from the rest
		check(Data.MotionBlurDataComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
		check(Data.MotionBlurDataComponent.VertexBuffer != Data.PositionComponent.VertexBuffer)

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

FVertexFactoryShaderParameters* FGeometryCacheVertexVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return (ShaderFrequency == SF_Vertex) ? new FGeometryCacheVertexFactoryShaderParameters() : nullptr;
}

bool FGeometryCacheVertexVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	// Should this be platform or mesh type based? Returning true should work in all cases, but maybe too expensive? 
	// return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
	// TODO currently GeomCache supports only 4 UVs which could cause compilation errors when trying to compile shaders which use > 4
	return Material->IsUsedWithGeometryCache() || Material->IsSpecialEngineMaterial();
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FGeometryCacheVertexVertexFactory, "/Engine/Private/GeometryCacheVertexFactory.ush", true, false, true, false, true);
