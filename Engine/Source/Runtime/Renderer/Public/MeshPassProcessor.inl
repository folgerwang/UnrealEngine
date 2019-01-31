// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshPassProcessor.inl:
=============================================================================*/

#pragma once

template<typename PassShadersType, typename ShaderElementDataType>
void FMeshPassProcessor::BuildMeshDrawCommands(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
	PassShadersType PassShaders,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	int32 InstanceFactor,
	FMeshDrawCommandSortKey SortKey,
	EMeshPassFeatures MeshPassFeatures,
	const ShaderElementDataType& ShaderElementData)
{
	const FVertexFactory* RESTRICT VertexFactory = MeshBatch.VertexFactory;
	const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = PrimitiveSceneProxy ? PrimitiveSceneProxy->GetPrimitiveSceneInfo() : nullptr;

	checkf(MaterialRenderProxy.ImmutableSamplerState.ImmutableSamplers[0] == nullptr, TEXT("Immutable samplers not yet supported in Mesh Draw Command pipeline"));

	FMeshDrawCommand SharedMeshDrawCommand;

	SharedMeshDrawCommand.SetStencilRef(DrawRenderState.GetStencilRef());
	SharedMeshDrawCommand.PipelineState.PrimitiveType = (EPrimitiveType)MeshBatch.Type;

	const bool bPositionOnly = (MeshPassFeatures & EMeshPassFeatures::PositionOnly) != EMeshPassFeatures::Default;

	check(VertexFactory && VertexFactory->IsInitialized());
	FVertexDeclarationRHIParamRef VertexDeclaration = bPositionOnly ? VertexFactory->GetPositionDeclaration() : VertexFactory->GetDeclaration();
	check(!VertexFactory->NeedsDeclaration() || VertexDeclaration);

	SharedMeshDrawCommand.SetShaders(VertexDeclaration, PassShaders.GetUntypedShaders());

	SharedMeshDrawCommand.PipelineState.RasterizerState = GetStaticRasterizerState<true>(MeshFillMode, MeshCullMode);

	check(DrawRenderState.GetDepthStencilState());
	check(DrawRenderState.GetBlendState());

	SharedMeshDrawCommand.PipelineState.BlendState = DrawRenderState.GetBlendState();
	SharedMeshDrawCommand.PipelineState.DepthStencilState = DrawRenderState.GetDepthStencilState();

	check(VertexFactory && VertexFactory->IsInitialized());

	if (bPositionOnly)
	{
		VertexFactory->GetPositionOnlyStream(SharedMeshDrawCommand.VertexStreams);
	}
	else
	{
		VertexFactory->GetStreams(FeatureLevel, SharedMeshDrawCommand.VertexStreams);
	}

	SharedMeshDrawCommand.PrimitiveIdStreamIndex = VertexFactory->GetPrimitiveIdStreamIndex(bPositionOnly);

	if (PassShaders.VertexShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Vertex);
		PassShaders.VertexShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	if (PassShaders.HullShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Hull);
		PassShaders.HullShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	if (PassShaders.DomainShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Domain);
		PassShaders.DomainShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	if (PassShaders.PixelShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Pixel);
		PassShaders.PixelShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	if (PassShaders.GeometryShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Geometry);
		PassShaders.GeometryShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	SharedMeshDrawCommand.SetDebugData(PrimitiveSceneProxy, &MaterialResource, &MaterialRenderProxy, PassShaders.GetUntypedShaders());

	const int32 NumElements = MeshBatch.Elements.Num();

	for (int32 BatchElementIndex = 0; BatchElementIndex < NumElements; BatchElementIndex++)
	{
		if ((1ull << BatchElementIndex) & BatchElementMask)
		{
			const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];
			FMeshDrawCommand& MeshDrawCommand = DrawListContext->AddCommand(SharedMeshDrawCommand);

			if (PassShaders.VertexShader)
			{
				FMeshDrawSingleShaderBindings VertexShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Vertex);
				PassShaders.VertexShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, bPositionOnly, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, VertexShaderBindings, MeshDrawCommand.VertexStreams);
			}

			if (PassShaders.HullShader && PassShaders.DomainShader)
			{
				FMeshDrawSingleShaderBindings HullShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Hull);
				FMeshDrawSingleShaderBindings DomainShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Domain);
				PassShaders.HullShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, false, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, HullShaderBindings, MeshDrawCommand.VertexStreams);
				PassShaders.DomainShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, false, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, DomainShaderBindings, MeshDrawCommand.VertexStreams);
			}

			if (PassShaders.PixelShader)
			{
				FMeshDrawSingleShaderBindings PixelShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Pixel);
				PassShaders.PixelShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, false, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, PixelShaderBindings, MeshDrawCommand.VertexStreams);
			}


			if (PassShaders.GeometryShader)
			{
				FMeshDrawSingleShaderBindings GeometryShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Geometry);
				PassShaders.GeometryShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, false, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, GeometryShaderBindings, MeshDrawCommand.VertexStreams);
			}

			const int32 DrawPrimitiveId = GetDrawCommandPrimitiveId(PrimitiveSceneInfo, BatchElement);

			DrawListContext->FinalizeCommand(MeshBatch, BatchElementIndex, DrawPrimitiveId, MeshFillMode, MeshCullMode, InstanceFactor, SortKey, MeshDrawCommand, true);
		}
	}
}

/**
* Provides a callback to build FMeshDrawCommands and then submits them immediately.  Useful for legacy / editor code paths.
* Does many dynamic allocations - do not use for game rendering.
*/
template<typename LambdaType>
void DrawDynamicMeshPass(const FSceneView& View, FRHICommandList& RHICmdList, const LambdaType& BuildPassProcessorLambda)
{
	FDynamicMeshDrawCommandStorage DynamicMeshDrawCommandStorage;
	FMeshCommandOneFrameArray VisibleMeshDrawCommands;

	FDynamicPassMeshDrawListContext DynamicMeshPassContext(DynamicMeshDrawCommandStorage, VisibleMeshDrawCommands);

	BuildPassProcessorLambda(&DynamicMeshPassContext);

	DrawDynamicMeshPassPrivate(View, RHICmdList, VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage);
}

template<typename PassShadersType, typename ShaderElementDataType>
void FMeshPassProcessor::BuildRayTracingDrawCommands(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
	PassShadersType PassShaders,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	int32 InstanceFactor,
	FMeshDrawCommandSortKey SortKey,
	EMeshPassFeatures MeshPassFeatures,
	const ShaderElementDataType& ShaderElementData)
{
	const FVertexFactory* RESTRICT VertexFactory = MeshBatch.VertexFactory;

	checkf(MaterialRenderProxy.ImmutableSamplerState.ImmutableSamplers[0] == nullptr, TEXT("Immutable samplers not yet supported in Mesh Draw Command pipeline"));

	FMeshDrawCommand SharedMeshDrawCommand;

	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->GetStreams(FeatureLevel, SharedMeshDrawCommand.VertexStreams);

#if RHI_RAYTRACING
	SharedMeshDrawCommand.SetRayTracingShaders(PassShaders.GetUntypedShaders());
#endif

	SharedMeshDrawCommand.SetDebugData(PrimitiveSceneProxy, &MaterialResource, &MaterialRenderProxy, PassShaders.GetUntypedShaders());

#if RHI_RAYTRACING
	if (PassShaders.RayHitGroupShader)
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_RayHitGroup);
		PassShaders.RayHitGroupShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}
#endif

	const int32 NumElements = MeshBatch.Elements.Num();

	for (int32 BatchElementIndex = 0; BatchElementIndex < NumElements; BatchElementIndex++)
	{
		if ((1ull << BatchElementIndex) & BatchElementMask)
		{
			const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];
			FMeshDrawCommand& MeshDrawCommand = DrawListContext->AddCommand(SharedMeshDrawCommand);

#if RHI_RAYTRACING
			if (PassShaders.RayHitGroupShader)
			{
				FMeshDrawSingleShaderBindings RayHitGroupShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_RayHitGroup);
				PassShaders.RayHitGroupShader->GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, false, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, RayHitGroupShaderBindings, MeshDrawCommand.VertexStreams);
			}
#endif

			const int32 DrawPrimitiveId = 0;
			const int32 ScenePrimitiveId = 0;
			DrawListContext->FinalizeCommand(MeshBatch, BatchElementIndex, DrawPrimitiveId, MeshFillMode, MeshCullMode, InstanceFactor, SortKey, MeshDrawCommand, false);
		}
	}
}