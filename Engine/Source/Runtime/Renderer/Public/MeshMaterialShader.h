// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
struct FMeshPassProcessorRenderState;

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

	// Clang treats FORCEINLINE as adivsory, and will not inline it on debug builds. Since Engine does not depend on the Renderer module, it fails to link against it.
#if PLATFORM_WINDOWS && defined(__clang__)
	void ValidateAfterBind();
#else
	FORCEINLINE void ValidateAfterBind()
	{
		checkfSlow(PassUniformBuffer.IsInitialized(), TEXT("FMeshMaterialShader must bind a pass uniform buffer, even if it is just FSceneTexturesUniformParameters: %s"), GetType()->GetName());
	}
#endif

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const;

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

	// FShader interface.
	virtual const FVertexFactoryParameterRef* GetVertexFactoryParameterRef() const override { return &VertexFactoryParameters; }
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;

protected:
	FShaderUniformBufferParameter PassUniformBuffer;

private:
	FVertexFactoryParameterRef VertexFactoryParameters;
};
