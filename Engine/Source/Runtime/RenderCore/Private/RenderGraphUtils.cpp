// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RenderGraphUtils.h"


void ClearUnusedGraphResourcesImpl(const FShaderParameterBindings& ShaderBindings, const FShaderParametersMetadata* ParametersMetadata, void* InoutParameters)
{
	int32 GraphTextureId = 0;
	int32 GraphSRVId = 0;
	int32 GraphUAVId = 0;

	uint8* Base = reinterpret_cast<uint8*>(InoutParameters);

	for (int32 ResourceIndex = 0, Num = ParametersMetadata->GetLayout().Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = EUniformBufferBaseType(ParametersMetadata->GetLayout().Resources[ResourceIndex]);
		uint16 ByteOffset = ParametersMetadata->GetLayout().ResourceOffsets[ResourceIndex];

		if (Type == UBMT_GRAPH_TRACKED_TEXTURE)
		{
			if (GraphTextureId < ShaderBindings.GraphTextures.Num() && ByteOffset == ShaderBindings.GraphTextures[GraphTextureId].ByteOffset)
			{
				GraphTextureId++;
				continue;
			}
		}
		else if (Type == UBMT_GRAPH_TRACKED_SRV || Type == UBMT_GRAPH_TRACKED_BUFFER_SRV)
		{
			if (GraphSRVId < ShaderBindings.GraphSRVs.Num() && ByteOffset == ShaderBindings.GraphSRVs[GraphSRVId].ByteOffset)
			{
				GraphSRVId++;
				continue;
			}
		}
		else if (Type == UBMT_GRAPH_TRACKED_UAV || Type == UBMT_GRAPH_TRACKED_BUFFER_UAV)
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

		void** ResourcePointerAddress = reinterpret_cast<void**>(Base + ByteOffset);
		*ResourcePointerAddress = nullptr;
	}
}
