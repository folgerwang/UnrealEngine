// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFogVoxelization.cpp
=============================================================================*/

#include "VolumetricFog.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "VolumetricFogShared.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"
#include "SpriteIndexBuffer.h"
#include "StaticMeshResources.h"
#include "MeshPassProcessor.inl"

int32 GVolumetricFogVoxelizationSlicesPerGSPass = 8;
FAutoConsoleVariableRef CVarVolumetricFogVoxelizationSlicesPerPass(
	TEXT("r.VolumetricFog.VoxelizationSlicesPerGSPass"),
	GVolumetricFogVoxelizationSlicesPerGSPass,
	TEXT("How many depth slices to render in a single voxelization pass (max geometry shader expansion).  Must recompile voxelization shaders to propagate changes."),
	ECVF_ReadOnly
	);

int32 GVolumetricFogVoxelizationShowOnlyPassIndex = -1;
FAutoConsoleVariableRef CVarVolumetricFogVoxelizationShowOnlyPassIndex(
	TEXT("r.VolumetricFog.VoxelizationShowOnlyPassIndex"),
	GVolumetricFogVoxelizationShowOnlyPassIndex,
	TEXT("When >= 0, indicates a single voxelization pass to render for debugging."),
	ECVF_RenderThreadSafe
	);

static FORCEINLINE int32 GetVoxelizationSlicesPerPass(EShaderPlatform Platform)
{
	return RHISupportsGeometryShaders(Platform) ? GVolumetricFogVoxelizationSlicesPerGSPass : 1;
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FVoxelizeVolumePassUniformParameters, "VoxelizeVolumePass");

void SetupVoxelizeVolumePassUniformBuffer(FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View, 
	const FVolumetricFogIntegrationParameterData& IntegrationData, 
	FVector2D Jitter, 
	FVoxelizeVolumePassUniformParameters& Parameters)
{
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
	SetupSceneTextureUniformParameters(SceneRenderTargets, View.FeatureLevel, ESceneTextureSetupMode::None, Parameters.SceneTextures);

	Parameters.ViewToVolumeClip = View.ViewMatrices.ComputeProjectionNoAAMatrix();
	Parameters.ViewToVolumeClip.M[2][0] += Jitter.X;
	Parameters.ViewToVolumeClip.M[2][1] += Jitter.Y;

	Parameters.FrameJitterOffset0 = IntegrationData.FrameJitterOffsetValues[0];

	SetupVolumetricFogGlobalData(View, Parameters.VolumetricFog);
}

class FQuadMeshVertexBuffer : public FRenderResource
{
public:
	FStaticMeshVertexBuffers Buffers;

	FQuadMeshVertexBuffer()
	{
		TArray<FDynamicMeshVertex> Vertices;

		// Vertex position constructed in the shader
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));
		Vertices.Add(FDynamicMeshVertex(FVector(0.0f, 0.0f, 0.0f)));

		Buffers.PositionVertexBuffer.Init(Vertices.Num());
		Buffers.StaticMeshVertexBuffer.Init(Vertices.Num(), 1);

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			Buffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			Buffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			Buffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
		}
	}

	virtual void InitRHI() override
	{
		Buffers.PositionVertexBuffer.InitResource();
		Buffers.StaticMeshVertexBuffer.InitResource();
	}

	virtual void ReleaseRHI() override
	{
		Buffers.PositionVertexBuffer.ReleaseResource();
		Buffers.StaticMeshVertexBuffer.ReleaseResource();
	}
};

TGlobalResource<FQuadMeshVertexBuffer> GQuadMeshVertexBuffer;

TGlobalResource<FSpriteIndexBuffer<1>> GQuadMeshIndexBuffer;

class FQuadMeshVertexFactory : public FLocalVertexFactory
{
public:
	FQuadMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FQuadMeshVertexFactory")
	{}

	~FQuadMeshVertexFactory()
	{
		ReleaseResource();
	}

	virtual void InitRHI() override
	{
		FQuadMeshVertexBuffer* VertexBuffer = &GQuadMeshVertexBuffer;
		FLocalVertexFactory::FDataType NewData;
		VertexBuffer->Buffers.PositionVertexBuffer.BindPositionVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(this, NewData, 0);
		FColorVertexBuffer::BindDefaultColorVertexBuffer(this, NewData, FColorVertexBuffer::NullBindStride::ZeroForDefaultBufferBind);
		SetData(NewData);
		FLocalVertexFactory::InitRHI();
	}

	bool HasIncompatibleFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel)
	{
		return InFeatureLevel != GetFeatureLevel();
	}
};

FQuadMeshVertexFactory* GQuadMeshVertexFactory = NULL;

class FVoxelizeVolumeShaderElementData : public FMeshMaterialShaderElementData
{
public:

	FVoxelizeVolumeShaderElementData(int32 InVoxelizationPassIndex)
		: VoxelizationPassIndex(InVoxelizationPassIndex)
	{
	}

	int32 VoxelizationPassIndex;
};

class FVoxelizeVolumeVS : public FMeshMaterialShader
{
    protected:

	FVoxelizeVolumeVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
		VoxelizationPassIndex.Bind(Initializer.ParameterMap, TEXT("VoxelizationPassIndex"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FVoxelizeVolumePassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FVoxelizeVolumeVS()
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) 
			&& DoesPlatformSupportVolumetricFogVoxelization(Platform)
			&& Material->GetMaterialDomain() == MD_Volume;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		if (RHISupportsGeometryShaders(Platform))
		{
			OutEnvironment.CompilerFlags.Add( CFLAG_VertexToGeometryShader );
		}
	}

public:

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << VoxelizationPassIndex;
		return bShaderHasOutdatedParameters;
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FVoxelizeVolumeShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		if (!RHISupportsGeometryShaders(Scene->GetShaderPlatform()))
		{
			ShaderBindings.Add(VoxelizationPassIndex, ShaderElementData.VoxelizationPassIndex);
		}
	}

protected:

	FShaderParameter VoxelizationPassIndex;
};

enum EVoxelizeShapeMode
{
	VMode_Primitive_Sphere,
	VMode_Object_Box
};

template<EVoxelizeShapeMode Mode>
class TVoxelizeVolumeVS : public FVoxelizeVolumeVS
{
	DECLARE_SHADER_TYPE(TVoxelizeVolumeVS,MeshMaterial);
	typedef FVoxelizeVolumeVS Super;

protected:

	TVoxelizeVolumeVS() {}
	TVoxelizeVolumeVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{
	}

public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		
		if (Mode == VMode_Primitive_Sphere)
		{
			OutEnvironment.SetDefine(TEXT("VOXELIZE_SHAPE_MODE"), TEXT("PRIMITIVE_SPHERE_MODE"));
		}
		else if (Mode == VMode_Object_Box)
		{
			OutEnvironment.SetDefine(TEXT("VOXELIZE_SHAPE_MODE"), TEXT("OBJECT_BOX_MODE"));
		}
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVoxelizeVolumeVS<VMode_Primitive_Sphere>,TEXT("/Engine/Private/VolumetricFogVoxelization.usf"),TEXT("VoxelizeVS"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVoxelizeVolumeVS<VMode_Object_Box>,TEXT("/Engine/Private/VolumetricFogVoxelization.usf"),TEXT("VoxelizeVS"),SF_Vertex); 

class FVoxelizeVolumeGS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVoxelizeVolumeGS,MeshMaterial);

protected:

	FVoxelizeVolumeGS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
		VoxelizationPassIndex.Bind(Initializer.ParameterMap, TEXT("VoxelizationPassIndex"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FVoxelizeVolumePassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FVoxelizeVolumeGS()
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) 
			&& RHISupportsGeometryShaders(Platform)
			&& DoesPlatformSupportVolumetricFogVoxelization(Platform)
			&& Material->GetMaterialDomain() == MD_Volume;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_SLICES_PER_VOXELIZATION_PASS"), GetVoxelizationSlicesPerPass(Platform));
	}

public:
	
	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << VoxelizationPassIndex;
		return bShaderHasOutdatedParameters;
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FVoxelizeVolumeShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(VoxelizationPassIndex, ShaderElementData.VoxelizationPassIndex);
	}
	
protected:

	FShaderParameter VoxelizationPassIndex;
};

template<EVoxelizeShapeMode Mode>
class TVoxelizeVolumeGS : public FVoxelizeVolumeGS
{
	DECLARE_SHADER_TYPE(TVoxelizeVolumeGS,MeshMaterial);
	typedef FVoxelizeVolumeGS Super;

protected:

	TVoxelizeVolumeGS() {}
	TVoxelizeVolumeGS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{
	}

public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		
		if (Mode == VMode_Primitive_Sphere)
		{
			OutEnvironment.SetDefine(TEXT("VOXELIZE_SHAPE_MODE"), TEXT("PRIMITIVE_SPHERE_MODE"));
		}
		else if (Mode == VMode_Object_Box)
		{
			OutEnvironment.SetDefine(TEXT("VOXELIZE_SHAPE_MODE"), TEXT("OBJECT_BOX_MODE"));
		}
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVoxelizeVolumeGS<VMode_Primitive_Sphere>,TEXT("/Engine/Private/VolumetricFogVoxelization.usf"),TEXT("VoxelizeGS"),SF_Geometry); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVoxelizeVolumeGS<VMode_Object_Box>,TEXT("/Engine/Private/VolumetricFogVoxelization.usf"),TEXT("VoxelizeGS"),SF_Geometry); 

class FVoxelizeVolumePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVoxelizeVolumePS,MeshMaterial);

protected:

	FVoxelizeVolumePS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FVoxelizeVolumePassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FVoxelizeVolumePS()
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) 
			&& DoesPlatformSupportVolumetricFogVoxelization(Platform)
			&& Material->GetMaterialDomain() == MD_Volume;
	}
};

template<EVoxelizeShapeMode Mode>
class TVoxelizeVolumePS : public FVoxelizeVolumePS
{
	DECLARE_SHADER_TYPE(TVoxelizeVolumePS,MeshMaterial);
	typedef FVoxelizeVolumePS Super;

protected:

	TVoxelizeVolumePS() {}
	TVoxelizeVolumePS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{
	}

public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		
		if (Mode == VMode_Primitive_Sphere)
		{
			OutEnvironment.SetDefine(TEXT("VOXELIZE_SHAPE_MODE"), TEXT("PRIMITIVE_SPHERE_MODE"));
		}
		else if (Mode == VMode_Object_Box)
		{
			OutEnvironment.SetDefine(TEXT("VOXELIZE_SHAPE_MODE"), TEXT("OBJECT_BOX_MODE"));
		}
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVoxelizeVolumePS<VMode_Primitive_Sphere>,TEXT("/Engine/Private/VolumetricFogVoxelization.usf"),TEXT("VoxelizePS"),SF_Pixel); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVoxelizeVolumePS<VMode_Object_Box>,TEXT("/Engine/Private/VolumetricFogVoxelization.usf"),TEXT("VoxelizePS"),SF_Pixel);

class FVoxelizeVolumeMeshProcessor : public FMeshPassProcessor
{
public:
	FVoxelizeVolumeMeshProcessor(const FScene* Scene, const FViewInfo* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

	void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, int32 NumVoxelizationPasses, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		checkf(false, TEXT("Default AddMeshBatch can't be used as rendering requires extra parameters per pass."));
	}

private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 NumVoxelizationPasses,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
};

FVoxelizeVolumeMeshProcessor::FVoxelizeVolumeMeshProcessor(const FScene* Scene, const FViewInfo* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState.SetBlendState(TStaticBlendState<
		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.VoxelizeVolumeViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.VoxelizeVolumePassUniformBuffer);
}

void FVoxelizeVolumeMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, int32 NumVoxelizationPasses, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy)
{
	// Determine the mesh's material and blend mode.
	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
	const ERasterizerCullMode MeshCullMode = CM_None;

	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

	Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, Material, NumVoxelizationPasses, MeshFillMode, MeshCullMode);
}

void FVoxelizeVolumeMeshProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	int32 NumVoxelizationPasses,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FVoxelizeVolumeVS,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FVoxelizeVolumePS,
		FVoxelizeVolumeGS> PassShaders;

	const bool bUsePrimitiveSphere = VertexFactory != GQuadMeshVertexFactory;

	if (bUsePrimitiveSphere)
	{
		PassShaders.VertexShader = MaterialResource.GetShader<TVoxelizeVolumeVS<VMode_Primitive_Sphere>>(VertexFactory->GetType());
		if (RHISupportsGeometryShaders(GShaderPlatformForFeatureLevel[FeatureLevel]))
		{
			PassShaders.GeometryShader = MaterialResource.GetShader<TVoxelizeVolumeGS<VMode_Primitive_Sphere>>(VertexFactory->GetType());
		}
		PassShaders.PixelShader = MaterialResource.GetShader<TVoxelizeVolumePS<VMode_Primitive_Sphere>>(VertexFactory->GetType());
	}
	else
	{
		PassShaders.VertexShader = MaterialResource.GetShader<TVoxelizeVolumeVS<VMode_Object_Box>>(VertexFactory->GetType());
		if (RHISupportsGeometryShaders(GShaderPlatformForFeatureLevel[FeatureLevel]))
		{
			PassShaders.GeometryShader = MaterialResource.GetShader<TVoxelizeVolumeGS<VMode_Object_Box>>(VertexFactory->GetType());
		}
		PassShaders.PixelShader = MaterialResource.GetShader<TVoxelizeVolumePS<VMode_Object_Box>>(VertexFactory->GetType());
	};

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	for (int32 VoxelizationPassIndex = 0; VoxelizationPassIndex < NumVoxelizationPasses; VoxelizationPassIndex++)
	{
		if (GVolumetricFogVoxelizationShowOnlyPassIndex < 0 || GVolumetricFogVoxelizationShowOnlyPassIndex == VoxelizationPassIndex)
		{
			FVoxelizeVolumeShaderElementData ShaderElementData(VoxelizationPassIndex);
			ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, -1, true);

			BuildMeshDrawCommands(
				MeshBatch,
				BatchElementMask,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				MaterialResource,
				PassDrawRenderState,
				PassShaders,
				MeshFillMode,
				MeshCullMode,
				SortKey,
				EMeshPassFeatures::Default,
				ShaderElementData);
		}
	}
}

void VoxelizeVolumePrimitive(FVoxelizeVolumeMeshProcessor& PassMeshProcessor,
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	FIntVector VolumetricFogGridSize,
	FVector GridZParams,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& OriginalMesh)
{
	const FMaterial& Material = *OriginalMesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());

	if (Material.GetMaterialDomain() == MD_Volume)
	{
		FMeshBatch LocalQuadMesh;

		// The voxelization shaders require camera facing quads as input
		// Vertex factories like particle sprites can work as-is, everything else needs to override with a camera facing quad
		const bool bOverrideWithQuadMesh = !OriginalMesh.VertexFactory->RendersPrimitivesAsCameraFacingSprites();

		if (bOverrideWithQuadMesh)
		{
			if (!GQuadMeshVertexFactory || GQuadMeshVertexFactory->HasIncompatibleFeatureLevel(View.GetFeatureLevel()))
			{
				if (GQuadMeshVertexFactory)
				{
					GQuadMeshVertexFactory->ReleaseResource();
					delete GQuadMeshVertexFactory;
				}
				GQuadMeshVertexFactory = new FQuadMeshVertexFactory(View.GetFeatureLevel());
				GQuadMeshVertexBuffer.UpdateRHI();
				GQuadMeshVertexFactory->InitResource();
			}
			LocalQuadMesh.VertexFactory = GQuadMeshVertexFactory;
			LocalQuadMesh.MaterialRenderProxy = OriginalMesh.MaterialRenderProxy;
			LocalQuadMesh.Elements[0].IndexBuffer = &GQuadMeshIndexBuffer;
			LocalQuadMesh.Elements[0].PrimitiveUniformBuffer = OriginalMesh.Elements[0].PrimitiveUniformBuffer;
			LocalQuadMesh.Elements[0].FirstIndex = 0;
			LocalQuadMesh.Elements[0].NumPrimitives = 2;
			LocalQuadMesh.Elements[0].MinVertexIndex = 0;
			LocalQuadMesh.Elements[0].MaxVertexIndex = 3;
		}

		const FMeshBatch& Mesh = bOverrideWithQuadMesh ? LocalQuadMesh : OriginalMesh;

		FBoxSphereBounds Bounds = PrimitiveSceneProxy->GetBounds();
		//@todo - compute NumSlices based on the largest particle size.  Bounds is overly conservative in most cases.
		float BoundsCenterDepth = View.ViewMatrices.GetViewMatrix().TransformPosition(Bounds.Origin).Z;
		int32 NearSlice = ComputeZSliceFromDepth(BoundsCenterDepth - Bounds.SphereRadius, GridZParams);
		int32 FarSlice = ComputeZSliceFromDepth(BoundsCenterDepth + Bounds.SphereRadius, GridZParams);

		NearSlice = FMath::Clamp(NearSlice, 0, VolumetricFogGridSize.Z - 1);
		FarSlice = FMath::Clamp(FarSlice, 0, VolumetricFogGridSize.Z - 1);

		const int32 NumSlices = FarSlice - NearSlice + 1;
		const int32 NumVoxelizationPasses = FMath::DivideAndRoundUp(NumSlices, GetVoxelizationSlicesPerPass(View.GetShaderPlatform()));

		const uint64 DefaultBatchElementMask = ~0ull;
		PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, NumVoxelizationPasses, PrimitiveSceneProxy);
	}
}

void FDeferredShadingSceneRenderer::VoxelizeFogVolumePrimitives(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FVolumetricFogIntegrationParameterData& IntegrationData,
	FIntVector VolumetricFogGridSize,
	FVector GridZParams,
	float VolumetricFogDistance)
{
	if (View.VolumetricMeshBatches.Num() > 0 && DoesPlatformSupportVolumetricFogVoxelization(View.GetShaderPlatform()))
	{
		FRenderTargetParameters* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(IntegrationData.VBufferA, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::EStore);
		PassParameters->RenderTargets[1] = FRenderTargetBinding(IntegrationData.VBufferB, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::EStore);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("VoxelizeVolumePrimitives"),
			PassParameters,
			ERenderGraphPassFlags::None,
			[PassParameters, Scene = Scene, &View, VolumetricFogGridSize, IntegrationData, VolumetricFogDistance, GridZParams](FRHICommandListImmediate& RHICmdList)
		{
			FViewUniformShaderParameters ViewVoxelizeParameters = *View.CachedViewUniformShaderParameters;

			// Update the parts of VoxelizeParameters which are dependent on the buffer size and view rect
			View.SetupViewRectUniformBufferParameters(
				ViewVoxelizeParameters,
				FIntPoint(VolumetricFogGridSize.X, VolumetricFogGridSize.Y),
				FIntRect(0, 0, VolumetricFogGridSize.X, VolumetricFogGridSize.Y),
				View.ViewMatrices,
				View.PrevViewInfo.ViewMatrices
			);

			FVector2D Jitter(IntegrationData.FrameJitterOffsetValues[0].X / VolumetricFogGridSize.X, IntegrationData.FrameJitterOffsetValues[0].Y / VolumetricFogGridSize.Y);

			FVoxelizeVolumePassUniformParameters VoxelizeVolumePassParameters;
			SetupVoxelizeVolumePassUniformBuffer(RHICmdList, View, IntegrationData, Jitter, VoxelizeVolumePassParameters);
			Scene->UniformBuffers.VoxelizeVolumeViewUniformBuffer.UpdateUniformBufferImmediate(ViewVoxelizeParameters);
			Scene->UniformBuffers.VoxelizeVolumePassUniformBuffer.UpdateUniformBufferImmediate(VoxelizeVolumePassParameters);

			FMeshPassProcessorRenderState DrawRenderState(View, Scene->UniformBuffers.VoxelizeVolumePassUniformBuffer);
			DrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.VoxelizeVolumeViewUniformBuffer);

			DrawDynamicMeshPass(View, RHICmdList,
				[&View, VolumetricFogDistance, &RHICmdList, &VolumetricFogGridSize, &GridZParams](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FVoxelizeVolumeMeshProcessor PassMeshProcessor(
					View.Family->Scene->GetRenderScene(),
					&View,
					DynamicMeshPassContext);

				for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.VolumetricMeshBatches.Num(); ++MeshBatchIndex)
				{
					const FMeshBatch* Mesh = View.VolumetricMeshBatches[MeshBatchIndex].Mesh;
					const FPrimitiveSceneProxy* PrimitiveSceneProxy = View.VolumetricMeshBatches[MeshBatchIndex].Proxy;
					const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
					const FBoxSphereBounds Bounds = PrimitiveSceneProxy->GetBounds();

					if ((View.ViewMatrices.GetViewOrigin() - Bounds.Origin).SizeSquared() < (VolumetricFogDistance + Bounds.SphereRadius) * (VolumetricFogDistance + Bounds.SphereRadius))
					{
						VoxelizeVolumePrimitive(PassMeshProcessor, RHICmdList, View, VolumetricFogGridSize, GridZParams, PrimitiveSceneProxy, *Mesh);
					}
				}
			});
		});
	}
}