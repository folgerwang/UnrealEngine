// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphResources.h"
#include "RenderGraphBuilder.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"


/** An empty shader parameter structure ready to be used anywhere. */
BEGIN_SHADER_PARAMETER_STRUCT(FEmptyShaderParameters, RENDERCORE_API)
END_SHADER_PARAMETER_STRUCT()


/** Useful parameter struct that only have render targets.
 *
 *	FRenderTargetParameters PassParameters;
 *	PassParameters.RenderTargets.DepthStencil = ... ;
 *	PassParameters.RenderTargets[0] = ... ;
 */
BEGIN_SHADER_PARAMETER_STRUCT(FRenderTargetParameters, RENDERCORE_API)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()


/**
 * Clears all render graph tracked resources that are not bound by a shader.
 * Excludes any resources on the ExcludeList from being cleared regardless of whether the 
 * shader binds them or not. This is needed for resources that are used outside of shader
 * bindings such as indirect arguments buffers.
 */
extern RENDERCORE_API void ClearUnusedGraphResourcesImpl(
	const FShaderParameterBindings& ShaderBindings,
	const FShaderParametersMetadata* ParametersMetadata,
	void* InoutParameters,
	std::initializer_list< FRDGResourceRef > ExcludeList);

template<typename TShaderClass>
FORCEINLINE_DEBUGGABLE void ClearUnusedGraphResources(
	const TShaderClass* Shader,
	typename TShaderClass::FParameters* InoutParameters,
	std::initializer_list< FRDGResourceRef > ExcludeList = {})
{
	const FShaderParametersMetadata* ParametersMetadata = TShaderClass::FParameters::FTypeInfo::GetStructMetadata();

	// Verify the shader have all the parameters it needs. This is done before the
	// ClearUnusedGraphResourcesImpl() to not misslead user on why some resource are misseing
	// when debugging a validation failure.
	ValidateShaderParameters(Shader, ParametersMetadata, InoutParameters);

	// Clear the resources the shader won't need.
	return ClearUnusedGraphResourcesImpl(Shader->Bindings, ParametersMetadata, InoutParameters, ExcludeList);
}


/** Register external texture with fallback if the resource is invalid. */
RENDERCORE_API FRDGTextureRef RegisterExternalTextureWithFallback(
	FRDGBuilder& GraphBuilder,
	const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
	const TRefCountPtr<IPooledRenderTarget>& FallbackPooledTexture,
	const TCHAR* ExternalPooledTextureName = TEXT("External"));


/** All utils for compute shaders.
 */
struct RENDERCORE_API FComputeShaderUtils
{
	/** Ideal size of group size 8x8 to occupy at least an entire wave on GCN, two warp on Nvidia. */
	static constexpr int32 kGolden2DGroupSize = 8;

	/** Compute the number of group to dispatch. */
	static FIntVector GetGroupCount(const int32 ThreadCount, const int32 GroupSize)
	{
		return FIntVector(
			FMath::DivideAndRoundUp(ThreadCount, GroupSize),
			1,
			1);
	}
	static FIntVector GetGroupCount(const FIntPoint& ThreadCount, const FIntPoint& GroupSize)
	{
		return FIntVector(
			FMath::DivideAndRoundUp(ThreadCount.X, GroupSize.X),
			FMath::DivideAndRoundUp(ThreadCount.Y, GroupSize.Y),
			1);
	}
	static FIntVector GetGroupCount(const FIntPoint& ThreadCount, const int32 GroupSize)
	{
		return FIntVector(
			FMath::DivideAndRoundUp(ThreadCount.X, GroupSize),
			FMath::DivideAndRoundUp(ThreadCount.Y, GroupSize),
			1);
	}
	static FIntVector GetGroupCount(const FIntVector& ThreadCount, const FIntVector& GroupSize)
	{
		return FIntVector(
			FMath::DivideAndRoundUp(ThreadCount.X, GroupSize.X),
			FMath::DivideAndRoundUp(ThreadCount.Y, GroupSize.Y),
			FMath::DivideAndRoundUp(ThreadCount.Z, GroupSize.Z));
	}


	/** Dispatch a compute shader to rhi command list with its parameters. */
	template<typename TShaderClass>
	static FORCEINLINE_DEBUGGABLE void Dispatch(FRHICommandList& RHICmdList, const TShaderClass* ComputeShader, const typename TShaderClass::FParameters& Parameters, FIntVector GroupCount)
	{
		FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
		RHICmdList.SetComputeShader(ShaderRHI);
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);
		RHICmdList.DispatchComputeShader(GroupCount.X, GroupCount.Y, GroupCount.Z);
		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	/** Dispatch a compute shader to render graph builder with its parameters. */
	template<typename TShaderClass>
	static FORCEINLINE_DEBUGGABLE void AddPass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		const TShaderClass* ComputeShader,
		typename TShaderClass::FParameters* Parameters,
		FIntVector GroupCount)
	{
		ClearUnusedGraphResources(ComputeShader, Parameters);

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName),
			Parameters,
			ERenderGraphPassFlags::Compute,
			[Parameters, ComputeShader, GroupCount](FRHICommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, GroupCount);
		});
	}

	/** Dispatch a compute shader to render graph builder with its parameters. */
	template<typename TShaderClass>
	static FORCEINLINE_DEBUGGABLE void AddPass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		const TShaderClass* ComputeShader,
		typename TShaderClass::FParameters* Parameters,
		FRDGBufferRef IndirectArgsBuffer,
		uint32 IndirectArgOffset)
	{
		ClearUnusedGraphResources(ComputeShader, Parameters, { IndirectArgsBuffer });

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName),
			Parameters,
			ERenderGraphPassFlags::Compute,
			[Parameters, ComputeShader, IndirectArgsBuffer, IndirectArgOffset](FRHICommandList& RHICmdList)
		{
			FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
			RHICmdList.SetComputeShader(ShaderRHI);
			SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, *Parameters);
			RHICmdList.DispatchIndirectComputeShader( IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgOffset );
			UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
		});
	}
};