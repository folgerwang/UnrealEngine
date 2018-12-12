// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshMaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "VertexFactory.h"
#include "MeshMaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshDrawShaderBindings.h"

class FPrimitiveSceneProxy;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;
struct FDrawingPolicyRenderState;

template<typename TBufferStruct> class TUniformBufferRef;

class FMeshMaterialShaderElementData
{
public:
	FUniformBufferRHIParamRef FadeUniformBuffer = nullptr;
	FUniformBufferRHIParamRef DitherUniformBuffer = nullptr;

	RENDERER_API void InitializeMeshMaterialData(const FSceneView* SceneView, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, const FMeshBatch& RESTRICT MeshBatch, int32 StaticMeshId, bool bAllowStencilDither);
};

/** Base class of all shaders that need material and vertex factory parameters. */
class RENDERER_API FMeshMaterialShader : public FMaterialShader
{
public:
	FMeshMaterialShader() {}

	FMeshMaterialShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		:	FMaterialShader(Initializer)
		,	VertexFactoryParameters(Initializer.VertexFactoryType, Initializer.ParameterMap, Initializer.Target.GetFrequency(), Initializer.Target.GetPlatform())
	{
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const TArray<FMaterial*>& Materials, const FVertexFactoryType* VertexFactoryType, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		return true;
	}

	FORCEINLINE void ValidateAfterBind()
	{
		checkfSlow(PassUniformBuffer.IsInitialized(), TEXT("FMeshMaterialShader must bind a pass uniform buffer, even if it is just FSceneTexturesUniformParameters: %s"), GetType()->GetName());
	}

	template< typename ShaderRHIParamRef >
	void SetPassUniformBuffer(
		FRHICommandList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI,
		FUniformBufferRHIParamRef PassUniformBufferValue)
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI, PassUniformBuffer, PassUniformBufferValue);
	}

	template< typename ShaderRHIParamRef >
	void SetParameters(
		FRHICommandList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FUniformBufferRHIParamRef PassUniformBufferValue)
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI, PassUniformBuffer, PassUniformBufferValue);

		checkfSlow(!(PassUniformBuffer.IsBound() && SceneTextureParameters.IsBound()) || SceneTextureParameters.IsSameUniformParameter(PassUniformBuffer), TEXT("If the pass uniform buffer is bound, it should contain SceneTexturesStruct: %s"), GetType()->GetName());

		SetViewParameters(RHICmdList, ShaderRHI, View, ViewUniformBuffer);
		FMaterialShader::SetParametersInner(RHICmdList, ShaderRHI, MaterialRenderProxy, Material, View);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FUniformBufferRHIParamRef PassUniformBufferValue,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const;

	template< typename ShaderRHIParamRef >
	void SetMesh(
		FRHICommandList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI,
		const FVertexFactory* VertexFactory,
		const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy,
		const FMeshBatchElement& BatchElement,
		const FDrawingPolicyRenderState& DrawRenderState,
		uint32 DataFlags = 0
	);

	void GetElementShaderBindings(
		const FScene* Scene, 
		const FSceneView* ViewIfDynamicMeshCommand, 
		const FVertexFactory* VertexFactory,
		bool bShaderRequiresPositionOnlyStream,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch,
		const FMeshBatchElement& BatchElement, 
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

	/**
	 * Retrieves the fade uniform buffer parameter from a FSceneViewState for the primitive
	 * This code was moved from SetMesh() to work around the template first-use vs first-seen differences between MSVC and others
	 */
	FUniformBufferRHIParamRef GetPrimitiveFadeUniformBufferParameter(const FSceneView& View, const FPrimitiveSceneProxy* Proxy);

	// FShader interface.
	virtual const FVertexFactoryParameterRef* GetVertexFactoryParameterRef() const override { return &VertexFactoryParameters; }
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;

protected:
	FShaderUniformBufferParameter PassUniformBuffer;

private:
	FVertexFactoryParameterRef VertexFactoryParameters;
};
