// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredMeshDecals.cpp: Deferred Decals implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "HitProxies.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "DepthRendering.h"
#include "DecalRenderingCommon.h"
#include "DecalRenderingShared.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "SceneRendering.h"
#include "UnrealEngine.h"
#include "DebugViewModeRendering.h"
#include "MeshPassProcessor.inl"


/**
* Policy for drawing mesh decals
*/
class FMeshDecalAccumulatePolicy
{	
public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material && Material->IsDeferredDecal() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}
};

/**
* A vertex shader for rendering mesh decals
*/
class FMeshDecalsVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FMeshDecalsVS, MeshMaterial);

protected:

	FMeshDecalsVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FMeshDecalsVS()
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FMeshDecalAccumulatePolicy::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}
};


/**
 * A hull shader for rendering mesh decals
 */
class FMeshDecalsHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FMeshDecalsHS, MeshMaterial);

protected:

	FMeshDecalsHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseHS(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FMeshDecalsHS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& FMeshDecalAccumulatePolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}
};

/**
 * A domain shader for rendering mesh decals
 */
class FMeshDecalsDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FMeshDecalsDS, MeshMaterial);

protected:

	FMeshDecalsDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseDS(Initializer)
	{}

	FMeshDecalsDS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& FMeshDecalAccumulatePolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsVS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainVS"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsHS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsDS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainDomain"),SF_Domain);


/**
* A pixel shader to render mesh decals
*/
class FMeshDecalsPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FMeshDecalsPS, MeshMaterial);

public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FMeshDecalAccumulatePolicy::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FDecalRendering::SetDecalCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FMeshDecalsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FMeshDecalsPS()
	{
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsPS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainPS"),SF_Pixel);

class FMeshDecalsEmissivePS : public FMeshDecalsPS
{
	DECLARE_SHADER_TYPE(FMeshDecalsEmissivePS, MeshMaterial);

public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FMeshDecalsPS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& Material->HasEmissiveColorConnected()
			&& IsDBufferDecalBlendMode(FDecalRenderingCommon::ComputeFinalDecalBlendMode(Platform, Material));
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshDecalsPS::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FDecalRendering::SetEmissiveDBufferDecalCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FMeshDecalsEmissivePS() {}
	FMeshDecalsEmissivePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshDecalsPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMeshDecalsEmissivePS, TEXT("/Engine/Private/MeshDecals.usf"), TEXT("MainPS"), SF_Pixel);


class FMeshDecalMeshProcessor : public FMeshPassProcessor
{
public:
	FMeshDecalMeshProcessor(const FScene* Scene, 
		const FSceneView* InViewIfDynamicMeshCommand, 
		EDecalRenderStage InPassDecalStage, 
		FDecalRenderingCommon::ERenderTargetMode InRenderTargetMode,
		FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
	const EDecalRenderStage PassDecalStage;
	const FDecalRenderingCommon::ERenderTargetMode RenderTargetMode;
};


FMeshDecalMeshProcessor::FMeshDecalMeshProcessor(const FScene* Scene, 
	const FSceneView* InViewIfDynamicMeshCommand, 
	EDecalRenderStage InPassDecalStage, 
	FDecalRenderingCommon::ERenderTargetMode InRenderTargetMode,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDecalStage(InPassDecalStage)
	, RenderTargetMode(InRenderTargetMode)
{
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.MeshDecalPassUniformBuffer);
}

void FMeshDecalMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial && MeshBatch.IsDecal(FeatureLevel))
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);

		if (Material && Material->IsDeferredDecal())
		{
			// We have no special engine material for decals since we don't want to eat the compilation & memory cost, so just skip if it failed to compile
			if (Material->GetRenderingThreadShaderMap())
			{
				const EShaderPlatform ShaderPlatform = ViewIfDynamicMeshCommand->GetShaderPlatform();
				const EDecalBlendMode FinalDecalBlendMode = FDecalRenderingCommon::ComputeFinalDecalBlendMode(ShaderPlatform, Material);
				const EDecalRenderStage LocalDecalRenderStage = FDecalRenderingCommon::ComputeRenderStage(ShaderPlatform, FinalDecalBlendMode);
				const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, *Material);
				const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, *Material);

				bool bShouldRender = FDecalRenderingCommon::IsCompatibleWithRenderStage(
					PassDecalStage,
					LocalDecalRenderStage,
					FinalDecalBlendMode,
					Material);

				if (FinalDecalBlendMode == DBM_Normal)
				{
					bShouldRender = bShouldRender && RenderTargetMode == FDecalRenderingCommon::RTM_GBufferNormal;
				}
				else
				{
					bShouldRender = bShouldRender && RenderTargetMode != FDecalRenderingCommon::RTM_GBufferNormal;
				}

				if (PassDecalStage == DRS_Emissive)
				{
					bShouldRender = bShouldRender && Material->HasEmissiveColorConnected();
				}

				if (bShouldRender)
				{
					const bool bHasNormal = Material->HasNormalConnected();

					const EDecalBlendMode DecalBlendMode = FDecalRenderingCommon::ComputeDecalBlendModeForRenderStage(
						FDecalRenderingCommon::ComputeFinalDecalBlendMode(ShaderPlatform, (EDecalBlendMode)Material->GetDecalBlendMode(), bHasNormal),
						PassDecalStage);

					if (ViewIfDynamicMeshCommand->Family->UseDebugViewPS())
					{
						// Deferred decals can only use translucent blend mode
						if (ViewIfDynamicMeshCommand->Family->EngineShowFlags.ShaderComplexity)
						{
							// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
							PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
						}
						else if (ViewIfDynamicMeshCommand->Family->GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
						{
							// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
							PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
						}
					}
					else
					{
						PassDrawRenderState.SetBlendState(GetDecalBlendState(FeatureLevel, PassDecalStage, DecalBlendMode, bHasNormal));
					}

					Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, MeshFillMode, MeshCullMode);
				}
			}
		}
	}
}

void FMeshDecalMeshProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();


	TMeshProcessorShaders<
		FMeshDecalsVS,
		FMeshDecalsHS,
		FMeshDecalsDS,
		FMeshDecalsPS> MeshDecalPassShaders;

	const EMaterialTessellationMode MaterialTessellationMode = MaterialResource.GetTessellationMode();

	const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[FeatureLevel])
		&& VertexFactoryType->SupportsTessellationShaders()
		&& MaterialTessellationMode != MTM_NoTessellation;

	if (bNeedsHSDS)
	{
		MeshDecalPassShaders.DomainShader = MaterialResource.GetShader<FMeshDecalsDS>(VertexFactoryType);
		MeshDecalPassShaders.HullShader = MaterialResource.GetShader<FMeshDecalsHS>(VertexFactoryType);
	}

	MeshDecalPassShaders.VertexShader = MaterialResource.GetShader<FMeshDecalsVS>(VertexFactoryType);

	MeshDecalPassShaders.PixelShader = PassDecalStage == DRS_Emissive
		? MaterialResource.GetShader<FMeshDecalsEmissivePS>(VertexFactoryType)
		: MaterialResource.GetShader<FMeshDecalsPS>(VertexFactoryType);


	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(MeshDecalPassShaders.VertexShader, MeshDecalPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		MeshDecalPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}


void DrawDecalMeshCommands(FRenderingCompositePassContext& Context, EDecalRenderStage CurrentDecalStage, FDecalRenderingCommon::ERenderTargetMode RenderTargetMode)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	const FViewInfo& View = Context.View;

	const bool bPerPixelDBufferMask = IsUsingPerPixelDBufferMask(View.GetShaderPlatform());

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	FDecalRenderTargetManager RenderTargetManager(Context.RHICmdList, Context.GetShaderPlatform(), CurrentDecalStage);
	RenderTargetManager.SetRenderTargetMode(RenderTargetMode, true, bPerPixelDBufferMask);
	Context.SetViewportAndCallRHI(Context.View.ViewRect);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);


	DrawDynamicMeshPass(View, RHICmdList,
		[&View, CurrentDecalStage, RenderTargetMode](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
	{
		FMeshDecalMeshProcessor PassMeshProcessor(
			View.Family->Scene->GetRenderScene(),
			&View,
			CurrentDecalStage,
			RenderTargetMode,
			DynamicMeshPassContext);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.MeshDecalBatches.Num(); ++MeshBatchIndex)
		{
			const FMeshBatch* Mesh = View.MeshDecalBatches[MeshBatchIndex].Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = View.MeshDecalBatches[MeshBatchIndex].Proxy;
			const uint64 DefaultBatchElementMask = ~0ull;

			PassMeshProcessor.AddMeshBatch(*Mesh, DefaultBatchElementMask, PrimitiveSceneProxy);
		}
	});
}

void RenderMeshDecals(FRenderingCompositePassContext& Context, EDecalRenderStage CurrentDecalStage)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Context.View;
	FScene* Scene = (FScene*)View.Family->Scene;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderMeshDecals);
	SCOPED_DRAW_EVENT(RHICmdList, MeshDecals);

	FSceneTexturesUniformParameters SceneTextureParameters;
	SetupSceneTextureUniformParameters(SceneContext, View.FeatureLevel, ESceneTextureSetupMode::All, SceneTextureParameters);
	Scene->UniformBuffers.MeshDecalPassUniformBuffer.UpdateUniformBufferImmediate(SceneTextureParameters);

	if (View.MeshDecalBatches.Num() > 0)
	{
		switch (CurrentDecalStage)
		{
		case DRS_BeforeBasePass:
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_DBuffer);
			break;

		case DRS_AfterBasePass:
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal);
			break;

		case DRS_BeforeLighting:
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_GBufferNormal);
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_SceneColorAndGBufferWithNormal);
			break;

		case DRS_Mobile:
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_SceneColor);
			break;

		case DRS_AmbientOcclusion:
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_AmbientOcclusion);
			break;

		case DRS_Emissive:
			DrawDecalMeshCommands(Context, CurrentDecalStage, FDecalRenderingCommon::RTM_SceneColor);
			break;
		}
	}
}