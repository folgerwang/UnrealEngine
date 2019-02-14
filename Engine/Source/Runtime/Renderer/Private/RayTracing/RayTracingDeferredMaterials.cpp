// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RayTracingDeferredMaterials.h"
#include "RHIDefinitions.h"
#include "RenderCore.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"

#if RHI_RAYTRACING

class FRayTracingDeferredMaterialCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDeferredMaterialCHS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingDeferredMaterialCHS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

class FRayTracingDeferredMaterialMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDeferredMaterialMS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingDeferredMaterialMS, FGlobalShader)

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingDeferredMaterialCHS, "/Engine/Private/RayTracing/RayTracingDeferredMaterials.usf", "DeferredMaterialCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FRayTracingDeferredMaterialMS,  "/Engine/Private/RayTracing/RayTracingDeferredMaterials.usf", "DeferredMaterialMS",  SF_RayMiss);

FRHIRayTracingPipelineState* FDeferredShadingSceneRenderer::BindRayTracingPipelineForDeferredMaterialGather(FRHICommandList& RHICmdList, const FViewInfo& View, FRayTracingShaderRHIParamRef RayGenShader)
{
	SCOPE_CYCLE_COUNTER(STAT_BindRayTracingPipeline);

	FRHIRayTracingPipelineState* PipelineState = nullptr;

	FRayTracingPipelineStateInitializer Initializer;

	FRayTracingShaderRHIParamRef RayGenShaderTable[] = { RayGenShader };
	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	auto MissShader = View.ShaderMap->GetShader<FRayTracingDeferredMaterialMS>();
	FRayTracingShaderRHIParamRef MissShaderTable[] = { MissShader->GetRayTracingShader() };
	Initializer.SetMissShaderTable(MissShaderTable);

	Initializer.MaxPayloadSizeInBytes = 12; // sizeof FDeferredMaterialPayload

	// Get the ray tracing materials
	auto ClosestHitShader = View.ShaderMap->GetShader<FRayTracingDeferredMaterialCHS>();
	FRayTracingShaderRHIParamRef HitShaderTable[] = { ClosestHitShader->GetRayTracingShader() };
	Initializer.SetHitGroupTable(HitShaderTable);
	Initializer.HitGroupStride = 1;

	PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer);

	for (const FVisibleMeshDrawCommand& VisibleMeshDrawCommand : View.RaytraycingVisibleMeshDrawCommands)
	{
		const FMeshDrawCommand& MeshDrawCommand = *VisibleMeshDrawCommand.MeshDrawCommand;

		const uint32 HitGroupIndex = 0; // Force the default CHS to be used on all geometry

		const uint32 ShaderSlot = 0; // Multiple shader slots can be used for different ray types. Slot 0 is the primary material slot.
		const uint32 MaterialIndexInUserData = MeshDrawCommand.RayTracingMaterialLibraryIndex;
		RHICmdList.SetRayTracingHitGroup(View.PerViewRayTracingScene.RayTracingSceneRHI,
			VisibleMeshDrawCommand.RayTracedInstanceIndex, MeshDrawCommand.RayTracedSegmentIndex, ShaderSlot,
			PipelineState, HitGroupIndex, 0, nullptr,
			MaterialIndexInUserData);
	}

	return PipelineState;
}

template<uint32 SortSize>
class TMaterialSortCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(TMaterialSortCS);
	SHADER_USE_PARAMETER_STRUCT(TMaterialSortCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, NumTotalEntries)
		SHADER_PARAMETER_RDG_BUFFER_UAV(StructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Compile time template-based conditional
		OutEnvironment.SetDefine(TEXT("NUM_ELEMENTS"), SortSize);
	}

};

IMPLEMENT_SHADER_TYPE(template<>, TMaterialSortCS<256>, TEXT("/Engine/Private/RayTracing/MaterialSort.usf"), TEXT("MaterialSortLocal"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TMaterialSortCS<512>, TEXT("/Engine/Private/RayTracing/MaterialSort.usf"), TEXT("MaterialSortLocal"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TMaterialSortCS<1024>, TEXT("/Engine/Private/RayTracing/MaterialSort.usf"), TEXT("MaterialSortLocal"), SF_Compute);

template<uint32 SortSize>
static void TemplateSortMaterials(FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	uint32 NumElements,
	FRDGBufferRef MaterialBuffer)
{
	// Setup shader and parameters
	TMaterialSortCS<SortSize>::FParameters* PassParameters = GraphBuilder.AllocParameters<TMaterialSortCS<SortSize>::FParameters>();
	PassParameters->NumTotalEntries = NumElements;
	PassParameters->MaterialBuffer = GraphBuilder.CreateUAV(MaterialBuffer);

	// Get the CS
	auto SortShader = View.ShaderMap->GetShader<TMaterialSortCS<SortSize> >();
	ClearUnusedGraphResources(SortShader, PassParameters);

	// Add the pass to the graph
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Material Sort<%d>", SortSize),
		PassParameters,
		ERenderGraphPassFlags::Compute,
		[PassParameters, &View, SortShader, NumElements](FRHICommandList& RHICmdList)

	{
		// Note that we are presently rounding down and leaving the last N elements unsorted
		const uint32 DispatchWidth = (NumElements + SortSize - 1) / SortSize;

		FRHIComputeShader* ShaderRHI = SortShader->GetComputeShader();

		RHICmdList.SetComputeShader(ShaderRHI);
		SetShaderParameters(RHICmdList, SortShader, ShaderRHI, *PassParameters);
		RHICmdList.DispatchComputeShader(DispatchWidth, 1, 1);
		UnsetShaderUAVs(RHICmdList, SortShader, ShaderRHI);
	});
}

void SortDeferredMaterials(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	uint32 SortSize,
	uint32 NumElements,
	FRDGBufferRef MaterialBuffer)
{
	SortSize = FMath::Min(SortSize, 3u);

	switch (SortSize)
	{
	case 1: TemplateSortMaterials<256>(GraphBuilder, View, NumElements, MaterialBuffer); break;
	case 2: TemplateSortMaterials<512>(GraphBuilder, View, NumElements, MaterialBuffer); break;
	case 3: TemplateSortMaterials<1024>(GraphBuilder, View, NumElements, MaterialBuffer); break;
	};
}

#else // RHI_RAYTRACING

void SortDeferredMaterials(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	uint32 SortSize,
	uint32 NumElements,
	FRDGBufferRef MaterialBuffer)
{
	checkNoEntry();
}

#endif // RHI_RAYTRACING
