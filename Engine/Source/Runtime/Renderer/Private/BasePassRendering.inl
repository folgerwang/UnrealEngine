// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.inl: Base pass rendering implementations.
		(Due to forward declaration issues)
=============================================================================*/

#pragma once

#include "CoreFwd.h"

class FMeshMaterialShader;
class FPrimitiveSceneProxy;
class FRHICommandList;
class FSceneView;
class FVertexFactory;
class FViewInfo;
struct FMeshBatch;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;

template<typename LightMapPolicyType>
void TBasePassVertexShaderPolicyParamType<LightMapPolicyType>::GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FMeshPassProcessorRenderState& DrawRenderState,
	const TBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
{
	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

	if (Scene)
	{
		FUniformBufferRHIParamRef ReflectionCaptureUniformBuffer = Scene->UniformBuffers.ReflectionCaptureUniformBuffer.GetReference();
		ShaderBindings.Add(ReflectionCaptureBuffer, ReflectionCaptureUniformBuffer);
	}
	else
	{
		ensure(!ReflectionCaptureBuffer.IsBound());
	}

	LightMapPolicyType::GetVertexShaderBindings(
		PrimitiveSceneProxy,
		ShaderElementData.LightMapPolicyElementData,
		this,
		ShaderBindings);
}

template<typename LightMapPolicyType>
void TBasePassVertexShaderPolicyParamType<LightMapPolicyType>::GetElementShaderBindings(
	const FScene* Scene, 
	const FSceneView* ViewIfDynamicMeshCommand, 
	const FVertexFactory* VertexFactory,
	bool bShaderRequiresPositionOnlyStream,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& MeshBatch,
	const FMeshBatchElement& BatchElement, 
	const TBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	FMeshMaterialShader::GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, bShaderRequiresPositionOnlyStream, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
}

template<typename LightMapPolicyType>
void TBasePassPixelShaderPolicyParamType<LightMapPolicyType>::GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FMeshPassProcessorRenderState& DrawRenderState,
	const TBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
{
	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

	if (Scene)
	{
		FUniformBufferRHIParamRef ReflectionCaptureUniformBuffer = Scene->UniformBuffers.ReflectionCaptureUniformBuffer.GetReference();
		ShaderBindings.Add(ReflectionCaptureBuffer, ReflectionCaptureUniformBuffer);
	}
	else
	{
		ensure(!ReflectionCaptureBuffer.IsBound());
	}

	LightMapPolicyType::GetPixelShaderBindings(
		PrimitiveSceneProxy,
		ShaderElementData.LightMapPolicyElementData,
		this,
		ShaderBindings);
}