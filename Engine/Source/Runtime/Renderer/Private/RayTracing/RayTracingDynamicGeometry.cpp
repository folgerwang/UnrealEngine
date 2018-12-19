// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshMaterialShader.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"
#include "RayTracing/RayTracingDynamicGeometryCollection.h"

#if RHI_RAYTRACING

static bool IsSupportedDynamicVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
{
	return VertexFactoryType == FindVertexFactoryType(FName(TEXT("FNiagaraSpriteVertexFactory"), FNAME_Find));
}

class FRayTracingDynamicGeometryConverterCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRayTracingDynamicGeometryConverterCS, MeshMaterial);
public:
	FRayTracingDynamicGeometryConverterCS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());

		RWVertexPositions.Bind(Initializer.ParameterMap, TEXT("VertexPositions"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
	}

	FRayTracingDynamicGeometryConverterCS() = default;

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// #dxr_todo: this should also check if ray tracing is enabled for the target platform & project
		return IsSupportedDynamicVertexFactoryType(VertexFactoryType) && IsRayTracingSupportedForThisProject();
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << RWVertexPositions;
		Ar << NumVertices;
		return bShaderHasOutdatedParameters;
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
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ViewUniformBuffer, PassUniformBufferValue, ShaderElementData, ShaderBindings);
	}

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
		FVertexInputStreamArray& VertexStreams) const
	{
		FMeshMaterialShader::GetElementShaderBindings(Scene, ViewIfDynamicMeshCommand, VertexFactory, bShaderRequiresPositionOnlyStream, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
	}

	FRWShaderParameter RWVertexPositions;
	FShaderParameter NumVertices;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRayTracingDynamicGeometryConverterCS, TEXT("/Engine/Private/RayTracing/RayTracingDynamicMesh.usf"), TEXT("RayTracingDynamicGeometryConverterCS"), SF_Compute);

void FRayTracingDynamicGeometryCollection::AddDynamicMeshBatchForGeometryUpdate(
	const FScene* Scene, 
	const FSceneView* View, 
	const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
	const FMeshBatch& MeshBatch, 
	FRayTracingGeometry& Geometry,
	FRWBuffer& Buffer)
{
	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), FallbackMaterialRenderProxyPtr);

	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

	TMeshProcessorShaders<
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FRayTracingDynamicGeometryConverterCS> Shaders;

	FMeshComputeDispatchCommand DispatchCmd;

	FRayTracingDynamicGeometryConverterCS* Shader = Material.GetShader<FRayTracingDynamicGeometryConverterCS>(MeshBatch.VertexFactory->GetType());
	DispatchCmd.MaterialShader = Shader;
	FMeshDrawShaderBindings& ShaderBindings = DispatchCmd.ShaderBindings;

	Shaders.ComputeShader = Shader;
	ShaderBindings.Initialize(Shaders.GetUntypedShaders());

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(View, PrimitiveSceneProxy, MeshBatch, -1, false);

	FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute);
	Shader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), PrimitiveSceneProxy, MaterialRenderProxy, Material, Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.OpaqueBasePassUniformBuffer, ShaderElementData, SingleShaderBindings);

	FVertexInputStreamArray DummyArray;
	Shader->GetElementShaderBindings(Scene, View, MeshBatch.VertexFactory, false, Scene->GetFeatureLevel(), PrimitiveSceneProxy, MeshBatch, MeshBatch.Elements[0], ShaderElementData, SingleShaderBindings, DummyArray);

	DispatchCmd.NumVertices = MeshBatch.Elements[0].NumPrimitives * 3 * MeshBatch.Elements[0].NumInstances;
	DispatchCmd.TargetBuffer = &Buffer;
	DispatchCmd.TargetGeometry = &Geometry;

	uint32 DesiredVertexBufferSize = FMath::DivideAndRoundUp((uint32)(DispatchCmd.NumVertices * sizeof(FVector)), 4096u) * 4096u;
	if (Buffer.NumBytes != DesiredVertexBufferSize)
	{
		int32 OriginalSize = Buffer.NumBytes;
		Buffer.Initialize(4, DesiredVertexBufferSize / 4, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource, TEXT("RayTracingDynamicVertexBuffer"));
	}

	check(DispatchCmd.TargetBuffer->NumBytes >= DispatchCmd.NumVertices * sizeof(FVector));

	FMeshDrawCommandDebugData DebugData;
	DebugData.Shaders = Shaders.GetUntypedShaders();

	ShaderBindings.Finalize(DebugData);

	DispatchCommands.Add(DispatchCmd);

	check(Geometry.IsInitialized());
	Geometry.Initializer.PositionVertexBuffer = Buffer.Buffer;
	Geometry.Initializer.TotalPrimitiveCount = DispatchCmd.NumVertices / 3;
	Geometry.RayTracingGeometryRHI = RHICreateRayTracingGeometry(Geometry.Initializer);
}

void FRayTracingDynamicGeometryCollection::DispatchUpdates(FRHICommandListImmediate& RHICmdList)
{
	if (DispatchCommands.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, RayTracingDynamicGeometryUpdate);

		for (auto& Cmd : DispatchCommands)
		{
			FRayTracingDynamicGeometryConverterCS* Shader = Cmd.MaterialShader;

			RHICmdList.SetComputeShader(Shader->GetComputeShader());

			Cmd.ShaderBindings.SetOnCommandListForCompute(RHICmdList, Shader->GetComputeShader());
			Shader->RWVertexPositions.SetBuffer(RHICmdList, Shader->GetComputeShader(), *Cmd.TargetBuffer);
			SetShaderValue(RHICmdList, Shader->GetComputeShader(), Shader->NumVertices, Cmd.NumVertices);

			RHICmdList.DispatchComputeShader(FMath::DivideAndRoundUp<uint32>(Cmd.NumVertices, 256), 1, 1);
			Shader->RWVertexPositions.UnsetUAV(RHICmdList, Shader->GetComputeShader());
		}

		TArray<FAccelerationStructureUpdateParams> BuildParams;

		for (auto& Cmd : DispatchCommands)
		{
			BuildParams.Add(FAccelerationStructureUpdateParams { Cmd.TargetGeometry->RayTracingGeometryRHI, Cmd.TargetBuffer->Buffer });
		}

		RHICmdList.BuildAccelerationStructures(BuildParams);

		Clear();
	}
}

void FRayTracingDynamicGeometryCollection::Clear()
{
	DispatchCommands.Empty();
}

#endif // RHI_RAYTRACING
