// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RayTracingTestbed.h"
#include "RHI.h"

#if RHI_RAYTRACING

#include "Containers/DynamicRHIResourceArray.h"
#include "GlobalShader.h"
#include "RayTracingDefinitions.h"

void TestBasicRayTracing(bool bValidateResults)
{
	if (!GRHISupportsRayTracing)
	{
		return;
	}

	FVertexBufferRHIRef VertexBuffer;

	{
		TResourceArray<FVector> PositionData;
		PositionData.SetNumUninitialized(3);
		PositionData[0] = FVector( 1, -1, 0);
		PositionData[1] = FVector( 1,  1, 0);
		PositionData[2] = FVector(-1, -1, 0);

		FRHIResourceCreateInfo CreateInfo;
		CreateInfo.ResourceArray = &PositionData;

		VertexBuffer = RHICreateVertexBuffer(PositionData.GetResourceDataSize(), BUF_Static, CreateInfo);
	}

	FIndexBufferRHIRef IndexBuffer;

	{
		TResourceArray<uint16> IndexData;
		IndexData.SetNumUninitialized(3);
		IndexData[0] = 0;
		IndexData[1] = 1;
		IndexData[2] = 2;

		FRHIResourceCreateInfo CreateInfo;
		CreateInfo.ResourceArray = &IndexData;

		IndexBuffer = RHICreateIndexBuffer(2, IndexData.GetResourceDataSize(), BUF_Static, CreateInfo);
	}

	static constexpr uint32 NumRays = 4;

	FStructuredBufferRHIRef RayBuffer;
	FShaderResourceViewRHIRef RayBufferView;

	{
		TResourceArray<FBasicRayData> RayData;
		RayData.SetNumUninitialized(NumRays);
		RayData[0] = FBasicRayData{ { 0.75f, 0.0f, -1.0f}, 0xFFFFFFFF, {0.0f, 0.0f,  1.0f}, 100000.0f }; // expected to hit
		RayData[1] = FBasicRayData{ { 0.75f, 0.0f, -1.0f}, 0xFFFFFFFF, {0.0f, 0.0f,  1.0f},      0.5f }; // expected to miss (short ray)
		RayData[2] = FBasicRayData{ { 0.75f, 0.0f,  1.0f}, 0xFFFFFFFF, {0.0f, 0.0f, -1.0f}, 100000.0f }; // expected to miss (back face culled)
		RayData[3] = FBasicRayData{ {-0.75f, 0.0f, -1.0f}, 0xFFFFFFFF, {0.0f, 0.0f,  1.0f}, 100000.0f }; // expected to miss (doesn't intersect)

		FRHIResourceCreateInfo CreateInfo;
		CreateInfo.ResourceArray = &RayData;

		RayBuffer = RHICreateStructuredBuffer(sizeof(FBasicRayData), RayData.GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		RayBufferView = RHICreateShaderResourceView(RayBuffer);
	}

	FVertexBufferRHIRef OcclusionResultBuffer;
	FUnorderedAccessViewRHIRef OcclusionResultBufferView;

	{
		FRHIResourceCreateInfo CreateInfo;
		OcclusionResultBuffer = RHICreateVertexBuffer(sizeof(uint32)*NumRays, BUF_Static | BUF_UnorderedAccess, CreateInfo);
		OcclusionResultBufferView = RHICreateUnorderedAccessView(OcclusionResultBuffer, PF_R32_UINT);
	}

	FVertexBufferRHIRef IntersectionResultBuffer;
	FUnorderedAccessViewRHIRef IntersectionResultBufferView;

	{
		FRHIResourceCreateInfo CreateInfo;
		IntersectionResultBuffer = RHICreateVertexBuffer(sizeof(FBasicRayIntersectionData)*NumRays, BUF_Static | BUF_UnorderedAccess, CreateInfo);
		IntersectionResultBufferView = RHICreateUnorderedAccessView(IntersectionResultBuffer, PF_R32_UINT);
	}

	FRayTracingGeometryInitializer GeometryInitializer;
	GeometryInitializer.IndexBuffer = IndexBuffer;
	GeometryInitializer.PositionVertexBuffer = VertexBuffer;
	GeometryInitializer.VertexBufferByteOffset = 0;
	GeometryInitializer.VertexBufferStride = sizeof(FVector);
	GeometryInitializer.VertexBufferElementType = VET_Float3;
	GeometryInitializer.BaseVertexIndex = 0;
	GeometryInitializer.PrimitiveType = PT_TriangleList;
	GeometryInitializer.TotalPrimitiveCount = 1;
	GeometryInitializer.bFastBuild = false;
	FRayTracingGeometryRHIRef Geometry = RHICreateRayTracingGeometry(GeometryInitializer);

	FRayTracingGeometryInstance Instances[] = {
		FRayTracingGeometryInstance { Geometry, FMatrix::Identity }
	};

	FRayTracingSceneInitializer Initializer;
	Initializer.Instances = Instances;
	Initializer.bIsDynamic = false;
	Initializer.ShaderSlotsPerGeometrySegment = RAY_TRACING_NUM_SHADER_SLOTS;
	FRayTracingSceneRHIParamRef Scene = RHICreateRayTracingScene(Initializer);

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	RHICmdList.BuildAccelerationStructure(Geometry);
	RHICmdList.BuildAccelerationStructure(Scene);

	RHICmdList.RayTraceOcclusion(Scene, RayBufferView, OcclusionResultBufferView, NumRays);
	RHICmdList.RayTraceIntersection(Scene, RayBufferView, IntersectionResultBufferView, NumRays);

	if (bValidateResults)
	{
		GDynamicRHI->RHISubmitCommandsAndFlushGPU();
		GDynamicRHI->RHIBlockUntilGPUIdle();

		// Read back and validate occlusion trace results

		{
			auto MappedResults = (const uint32*)RHILockVertexBuffer(OcclusionResultBuffer, 0, sizeof(uint32)*NumRays, RLM_ReadOnly);

			check(MappedResults);

			check(MappedResults[0] != 0); // expect hit
			check(MappedResults[1] == 0); // expect miss
			check(MappedResults[2] == 0); // expect miss
			check(MappedResults[3] == 0); // expect miss

			RHIUnlockVertexBuffer(OcclusionResultBuffer);
		}

		// Read back and validate intersection trace results

		{
			auto MappedResults = (const FBasicRayIntersectionData*)RHILockVertexBuffer(IntersectionResultBuffer, 0, sizeof(FBasicRayIntersectionData)*NumRays, RLM_ReadOnly);

			check(MappedResults);

			// expect hit primitive 0, instance 0, barycentrics {0.5, 0.125}
			check(MappedResults[0].PrimitiveIndex == 0);
			check(MappedResults[0].InstanceIndex == 0);
			check(FMath::IsNearlyEqual(MappedResults[0].Barycentrics[0], 0.5f));
			check(FMath::IsNearlyEqual(MappedResults[0].Barycentrics[1], 0.125f));

			check(MappedResults[1].PrimitiveIndex == ~0u); // expect miss
			check(MappedResults[2].PrimitiveIndex == ~0u); // expect miss
			check(MappedResults[3].PrimitiveIndex == ~0u); // expect miss

			RHIUnlockVertexBuffer(IntersectionResultBuffer);
		}
	}
}

// Dummy shader to test shader compilation and reflection.
class FTestRaygenShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTestRaygenShader, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FTestRaygenShader() {}
	virtual ~FTestRaygenShader() {}

	/** Initialization constructor. */
	FTestRaygenShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TLAS.Bind(Initializer.ParameterMap, TEXT("TLAS"));
		Rays.Bind(Initializer.ParameterMap, TEXT("Rays"));
		Output.Bind(Initializer.ParameterMap, TEXT("Output"));
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TLAS;
		Ar << Rays;
		Ar << Output;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter	TLAS;   // SRV RaytracingAccelerationStructure
	FShaderResourceParameter	Rays;   // SRV StructuredBuffer<FBasicRayData>
	FShaderResourceParameter	Output; // UAV RWStructuredBuffer<uint>
};

IMPLEMENT_SHADER_TYPE(, FTestRaygenShader, TEXT("/Engine/Private/RayTracing/RayTracingTest.usf"), TEXT("TestMainRGS"), SF_RayGen);

#else // RHI_RAYTRACING

void TestBasicRayTracing(bool bValidateResults)
{
	// Nothing to do when ray tracing is disabled
}

#endif // RHI_RAYTRACING
