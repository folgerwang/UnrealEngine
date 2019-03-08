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

FRHIRayTracingPipelineState* FDeferredShadingSceneRenderer::BindRayTracingDeferredMaterialGatherPipeline(FRHICommandList& RHICmdList, const FViewInfo& View, FRayTracingShaderRHIParamRef RayGenShader)
{
	SCOPE_CYCLE_COUNTER(STAT_BindRayTracingPipeline);

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

	FRHIRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer);

	const FViewInfo& ReferenceView = Views[0];

	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : ReferenceView.VisibleRayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		const uint32 HitGroupIndex = 0; // Force the default CHS to be used on all geometry

		const uint32 ShaderSlot = 0; // Multiple shader slots can be used for different ray types. Slot 0 is the primary material slot.
		const uint32 MaterialIndexInUserData = MeshCommand.MaterialShaderIndex;
		RHICmdList.SetRayTracingHitGroup(
			View.RayTracingScene.RayTracingSceneRHI,
			VisibleMeshCommand.InstanceIndex, 
			MeshCommand.GeometrySegmentIndex, 
			ShaderSlot,
			PipelineState,
			HitGroupIndex,
			0, 
			nullptr,
			MaterialIndexInUserData);
	}

	return PipelineState;
}

class FMaterialSortCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMaterialSortCS);
	SHADER_USE_PARAMETER_STRUCT(FMaterialSortCS, FGlobalShader);

	class FSortSize : SHADER_PERMUTATION_INT("DIM_SORT_SIZE", 5);

	using FPermutationDomain = TShaderPermutationDomain<FSortSize>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, NumTotalEntries)
		SHADER_PARAMETER_RDG_BUFFER_UAV(StructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM5;
	}
};


IMPLEMENT_GLOBAL_SHADER(FMaterialSortCS, "/Engine/Private/RayTracing/MaterialSort.usf", "MaterialSortLocal", SF_Compute);

void SortDeferredMaterials(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	uint32 SortSize,
	uint32 NumElements,
	FRDGBufferRef MaterialBuffer)
{
	if (SortSize == 0)
	{
		return;
	}
	SortSize = FMath::Min(SortSize, 5u);

	// Setup shader and parameters
	FMaterialSortCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMaterialSortCS::FParameters>();
	PassParameters->NumTotalEntries = NumElements;
	PassParameters->MaterialBuffer = GraphBuilder.CreateUAV(MaterialBuffer);

	FMaterialSortCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FMaterialSortCS::FSortSize>(SortSize - 1);

	// Sort size represents an index into pow2 sizes, not an actual size, so convert to the actual number of elements being sorted
	const uint32 ElementBlockSize = 256 * (1 << (SortSize - 1));
	const uint32 DispatchWidth = FMath::DivideAndRoundUp(NumElements, ElementBlockSize);

	TShaderMapRef<FMaterialSortCS> SortShader(View.ShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MaterialSort SortSize=%d NumElements=%d", ElementBlockSize, NumElements),
		*SortShader,
		PassParameters,
		FIntVector(DispatchWidth, 1, 1));		
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
