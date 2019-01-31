// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RenderGraphUtils.h"
#include <initializer_list>

void ClearUnusedGraphResourcesImpl(
	const FShaderParameterBindings& ShaderBindings,
	const FShaderParametersMetadata* ParametersMetadata,
	void* InoutParameters,
	std::initializer_list< FRDGResourceRef > ExcludeList)
{
	int32 GraphTextureId = 0;
	int32 GraphSRVId = 0;
	int32 GraphUAVId = 0;

	uint8* Base = reinterpret_cast<uint8*>(InoutParameters);

	for (int32 ResourceIndex = 0, Num = ParametersMetadata->GetLayout().Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParametersMetadata->GetLayout().Resources[ResourceIndex].MemberType;
		uint16 ByteOffset = ParametersMetadata->GetLayout().Resources[ResourceIndex].MemberOffset;

		if (Type == UBMT_RDG_TEXTURE)
		{
			if (GraphTextureId < ShaderBindings.GraphTextures.Num() && ByteOffset == ShaderBindings.GraphTextures[GraphTextureId].ByteOffset)
			{
				GraphTextureId++;
				continue;
			}
		}
		else if (Type == UBMT_RDG_TEXTURE_SRV || Type == UBMT_RDG_BUFFER_SRV)
		{
			if (GraphSRVId < ShaderBindings.GraphSRVs.Num() && ByteOffset == ShaderBindings.GraphSRVs[GraphSRVId].ByteOffset)
			{
				GraphSRVId++;
				continue;
			}
		}
		else if (Type == UBMT_RDG_TEXTURE_UAV || Type == UBMT_RDG_BUFFER_UAV)
		{
			if (GraphUAVId < ShaderBindings.GraphUAVs.Num() && ByteOffset == ShaderBindings.GraphUAVs[GraphUAVId].ByteOffset)
			{
				GraphUAVId++;
				continue;
			}
		}
		else
		{
			continue;
		}

		for( FRDGResourceRef ExcludeResource : ExcludeList )
		{
			auto Resource = *reinterpret_cast<const FRDGResource* const*>(Base + ByteOffset);
			if( Resource == ExcludeResource )
			{
				continue;
			}
		}

		void** ResourcePointerAddress = reinterpret_cast<void**>(Base + ByteOffset);
		*ResourcePointerAddress = nullptr;
	}
}


FRDGTextureRef RegisterExternalTextureWithFallback(
	FRDGBuilder& GraphBuilder,
	const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
	const TRefCountPtr<IPooledRenderTarget>& FallbackPooledTexture,
	const TCHAR* ExternalPooledTextureName)
{
	ensureMsgf(FallbackPooledTexture.IsValid(), TEXT("RegisterExternalTextureWithDummyFallback() requires a valid fallback pooled texture."));
	if (ExternalPooledTexture.IsValid())
	{
		return GraphBuilder.RegisterExternalTexture(ExternalPooledTexture, ExternalPooledTextureName);
	}
	else
	{
		return GraphBuilder.RegisterExternalTexture(FallbackPooledTexture);
	}
}

