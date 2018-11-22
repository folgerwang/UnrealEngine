// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphResources.h"
#include "RenderGraphBuilder.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"


/** Useful parameter struct that only have render targets.
 *
 *	FRenderTargetParameters PassParameters;
 *	PassParameters.RenderTargets.DepthStencil = ... ;
 *	PassParameters.RenderTargets[0] = ... ;
 */
BEGIN_SHADER_PARAMETER_STRUCT(FRenderTargetParameters, RENDERCORE_API)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()


/** Clears all render graph tracked resources that are not used by a shader bindings. */
extern RENDERCORE_API void ClearUnusedGraphResourcesImpl(const FShaderParameterBindings& ShaderBindings, const FShaderParametersMetadata* ParametersMetadata, void* InoutParameters);

template<typename TShaderClass>
inline void ClearUnusedGraphResources(const TShaderClass* Shader, typename TShaderClass::FParameters* InoutParameters)
{
	return ClearUnusedGraphResourcesImpl(Shader->Bindings, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), InoutParameters);
}


/** All utils for compute shaders.
 */
struct RENDERCORE_API FComputeShaderUtils
{
	/** Ideal size of group size 8x8 to occupy at least an entire wave on GCN, two warp on Nvidia. */
	static constexpr int32 kGolden2DGroupSize = 8;

	/** Compute the number of group to dispatch. */
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


	/** Dispatch a compute shader to rhi command list with its parameters. */
	template<typename TShaderClass>
	static inline void Dispatch(FRHICommandList& RHICmdList, const TShaderClass* ComputeShader, const typename TShaderClass::FParameters& Parameters, FIntVector GroupCount)
	{
		FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
		RHICmdList.SetComputeShader(ShaderRHI);
		SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, Parameters);
		RHICmdList.DispatchComputeShader(GroupCount.X, GroupCount.Y, GroupCount.Z);
		UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
	}

	/** Dispatch a compute shader to render graph builder with its parameters. */
	template<typename TShaderClass>
	static inline void AddPass(
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
};
