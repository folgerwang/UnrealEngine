// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VelocityRendering.cpp: Velocity rendering implementation.
=============================================================================*/

#include "VelocityRendering.h"
#include "SceneUtils.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "PostProcess/ScreenSpaceReflections.h"
#include "UnrealEngine.h"
#if WITH_EDITOR
#include "Misc/CoreMisc.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif
#include "VisualizeTexture.h"
#include "MeshPassProcessor.inl"

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarBasePassOutputsVelocity(
	TEXT("r.BasePassOutputsVelocity"),
	0,
	TEXT("Enables rendering WPO velocities on the base pass.\n") \
	TEXT(" 0: Renders in a separate pass/rendertarget, all movable static meshes + dynamic.\n") \
	TEXT(" 1: Renders during the regular base pass adding an extra GBuffer, but allowing motion blur on materials with Time-based WPO."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarParallelVelocity(
	TEXT("r.ParallelVelocity"),
	1,  
	TEXT("Toggles parallel velocity rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarRHICmdVelocityPassDeferredContexts(
	TEXT("r.RHICmdVelocityPassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize velocity pass command list execution."));

RENDERER_API TAutoConsoleVariable<int32> CVarAllowMotionBlurInVR(
	TEXT("vr.AllowMotionBlurInVR"),
	0,
	TEXT("For projects with motion blur enabled, this allows motion blur to be enabled even while in VR."));

DECLARE_GPU_STAT_NAMED(RenderVelocities, TEXT("Render Velocities"));

bool IsParallelVelocity()
{
	return GRHICommandList.UseParallelAlgorithms() && CVarParallelVelocity.GetValueOnRenderThread();
}

//=============================================================================
/** Encapsulates the Velocity vertex shader. */
class FVelocityVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVelocityVS,MeshMaterial);

public:

	bool SupportsVelocity() const
	{
		return GPUSkinCachePreviousPositionBuffer.IsBound() ||
			PrevTransformBuffer.IsBound() || 
			(PrevTransform0.IsBound() && PrevTransform1.IsBound() && PrevTransform2.IsBound()) ||
			//@todo MeshCommandPipeline - now that PreviousLocalToWorld is in the primitive uniform buffer, we can't look at whether the shader bound it to cull what gets rendered in velocity pass
			true;
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return ((Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialMayModifyMeshPosition()))
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) 
			&& !FVelocityRendering::VertexFactoryOnlyOutputsVelocityInBasePass(Platform, VertexFactoryType->SupportsStaticLighting());
	}

protected:
	FVelocityVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		GPUSkinCachePreviousPositionBuffer.Bind(Initializer.ParameterMap, TEXT("GPUSkinCachePreviousPositionBuffer"));
		PrevTransform0.Bind(Initializer.ParameterMap, TEXT("PrevTransform0"));
		PrevTransform1.Bind(Initializer.ParameterMap, TEXT("PrevTransform1"));
		PrevTransform2.Bind(Initializer.ParameterMap, TEXT("PrevTransform2"));
		PrevTransformBuffer.Bind(Initializer.ParameterMap, TEXT("PrevTransformBuffer"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FVelocityVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);

		Ar << GPUSkinCachePreviousPositionBuffer;
		Ar << PrevTransform0;
		Ar << PrevTransform1;
		Ar << PrevTransform2;
		Ar << PrevTransformBuffer;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter GPUSkinCachePreviousPositionBuffer;
	FShaderParameter PrevTransform0;
	FShaderParameter PrevTransform1;
	FShaderParameter PrevTransform2;
	FShaderResourceParameter PrevTransformBuffer;
};


//=============================================================================
/** Encapsulates the Velocity hull shader. */
class FVelocityHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FVelocityHS,MeshMaterial);

protected:
	FVelocityHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	FVelocityHS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCompilePermutation(Platform, Material, VertexFactoryType) &&
				FVelocityVS::ShouldCompilePermutation(Platform, Material, VertexFactoryType); // same rules as VS
	}
};

//=============================================================================
/** Encapsulates the Velocity domain shader. */
class FVelocityDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FVelocityDS,MeshMaterial);

protected:
	FVelocityDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{}

	FVelocityDS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCompilePermutation(Platform, Material, VertexFactoryType) &&
				FVelocityVS::ShouldCompilePermutation(Platform, Material, VertexFactoryType); // same rules as VS
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityVS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainVertexShader"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityHS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityDS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainDomain"),SF_Domain);

//=============================================================================
/** Encapsulates the Velocity pixel shader. */
class FVelocityPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVelocityPS,MeshMaterial);
public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return ((Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialMayModifyMeshPosition()))
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && 
			!FVelocityRendering::VertexFactoryOnlyOutputsVelocityInBasePass(Platform, VertexFactoryType->SupportsStaticLighting());
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_G16R16);
	}

	FVelocityPS() {}

	FVelocityPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityPS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainPixelShader"),SF_Pixel);

IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(VelocityPipeline, FVelocityVS, FVelocityPS, true);


int32 GetMotionBlurQualityFromCVar()
{
	int32 MotionBlurQuality;

	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurQuality"));
	MotionBlurQuality = FMath::Clamp(ICVar->GetValueOnRenderThread(), 0, 4);

	return MotionBlurQuality;
}

bool IsMotionBlurEnabled(const FViewInfo& View)
{
	if (View.GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		return false; 
	}

	int32 MotionBlurQuality = GetMotionBlurQualityFromCVar();

	return View.Family->EngineShowFlags.PostProcessing
		&& View.Family->EngineShowFlags.MotionBlur
		&& View.FinalPostProcessSettings.MotionBlurAmount > 0.001f
		&& View.FinalPostProcessSettings.MotionBlurMax > 0.001f
		&& View.Family->bRealtimeUpdate
		&& MotionBlurQuality > 0
		&& !IsSimpleForwardShadingEnabled(GShaderPlatformForFeatureLevel[View.GetFeatureLevel()])
		&& (CVarAllowMotionBlurInVR->GetInt() != 0 || !(View.Family->Views.Num() > 1));
}

static void BeginVelocityRendering(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT, bool bPerformClear)
{
	check(RHICmdList.IsOutsideRenderPass());

	FTextureRHIRef VelocityTexture = VelocityRT->GetRenderTargetItem().TargetableTexture;
	FTexture2DRHIRef DepthTexture = FSceneRenderTargets::Get(RHICmdList).GetSceneDepthTexture();	

	FRHIRenderPassInfo RPInfo(VelocityTexture, ERenderTargetActions::Load_Store);
	RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_Store, ERenderTargetActions::Load_Store);
	RPInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthTexture;
	RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilWrite;

	if (bPerformClear)
	{
		RPInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
	}

	RHICmdList.BeginRenderPass(RPInfo, TEXT("VelocityRendering"));

	if (!bPerformClear)
	{
		// some platforms need the clear color when rendertargets transition to SRVs.  We propagate here to allow parallel rendering to always
		// have the proper mapping when the RT is transitioned.
		RHICmdList.BindClearMRTValues(true, false, false);
	}
}

static void SetVelocitiesState(FRHICommandList& RHICmdList, const FViewInfo& View, const FSceneRenderer* SceneRender, FMeshPassProcessorRenderState& DrawRenderState, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	const FIntPoint BufferSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
	const FIntPoint VelocityBufferSize = BufferSize;		// full resolution so we can reuse the existing full res z buffer

	if (!View.IsInstancedStereoPass())
	{
		const uint32 MinX = View.ViewRect.Min.X * VelocityBufferSize.X / BufferSize.X;
		const uint32 MinY = View.ViewRect.Min.Y * VelocityBufferSize.Y / BufferSize.Y;
		const uint32 MaxX = View.ViewRect.Max.X * VelocityBufferSize.X / BufferSize.X;
		const uint32 MaxY = View.ViewRect.Max.Y * VelocityBufferSize.Y / BufferSize.Y;
		RHICmdList.SetViewport(MinX, MinY, 0.0f, MaxX, MaxY, 1.0f);
	}
	else
	{
		if (View.bIsMultiViewEnabled)
		{
			const uint32 LeftMinX = SceneRender->Views[0].ViewRect.Min.X;
			const uint32 LeftMaxX = SceneRender->Views[0].ViewRect.Max.X;
			const uint32 RightMinX = SceneRender->Views[1].ViewRect.Min.X;
			const uint32 RightMaxX = SceneRender->Views[1].ViewRect.Max.X;
			
			const uint32 LeftMaxY = SceneRender->Views[0].ViewRect.Max.Y;
			const uint32 RightMaxY = SceneRender->Views[1].ViewRect.Max.Y;
			
			RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0, 0.0f, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, 1.0f);
		}
		else
		{
			const uint32 MaxX = SceneRender->InstancedStereoWidth * VelocityBufferSize.X / BufferSize.X;
			const uint32 MaxY = View.ViewRect.Max.Y * VelocityBufferSize.Y / BufferSize.Y;
			RHICmdList.SetViewport(0, 0, 0.0f, MaxX, MaxY, 1.0f);
		}
	}

	DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	//TODO Where does this state go?
	//RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(FM_Solid, CM_CW));
}

DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLP_Velocity, STATGROUP_ParallelCommandListMarkers);


class FVelocityPassParallelCommandListSet : public FParallelCommandListSet
{
	TRefCountPtr<IPooledRenderTarget>& VelocityRT;

public:
	FVelocityPassParallelCommandListSet(
		const FViewInfo& InView,
		const FSceneRenderer* InSceneRenderer,
		FRHICommandListImmediate& InParentCmdList,
		bool bInParallelExecute,
		bool bInCreateSceneContext,
		const FMeshPassProcessorRenderState& InDrawRenderState,
		TRefCountPtr<IPooledRenderTarget>& InVelocityRT)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Velocity), InView, InSceneRenderer, InParentCmdList, bInParallelExecute, bInCreateSceneContext, InDrawRenderState)
		, VelocityRT(InVelocityRT)
	{
	}

	virtual ~FVelocityPassParallelCommandListSet()
	{
		Dispatch();
	}	

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		BeginVelocityRendering(CmdList, VelocityRT, false);
		SetVelocitiesState(CmdList, View, SceneRenderer, DrawRenderState, VelocityRT);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksVelocityPass(
	TEXT("r.RHICmdFlushRenderThreadTasksVelocityPass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the velocity pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksVelocityPass is > 0 we will flush."));

void FDeferredShadingSceneRenderer::RenderVelocitiesInnerParallel(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	// Parallel rendering requires its own renderpasses so we cannot have an active one at this point
	check(RHICmdList.IsOutsideRenderPass());
	// parallel version
	FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksVelocityPass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		if (View.ShouldRenderView())
		{
			SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

			Scene->UniformBuffers.UpdateViewUniformBuffer(View);

			FSceneTexturesUniformParameters SceneTextureParameters;
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			SetupSceneTextureUniformParameters(SceneContext, View.FeatureLevel, ESceneTextureSetupMode::None, SceneTextureParameters);
			TUniformBufferRef<FSceneTexturesUniformParameters> PassUniformBuffer = TUniformBufferRef<FSceneTexturesUniformParameters>::CreateUniformBufferImmediate(SceneTextureParameters, UniformBuffer_SingleFrame);

			FMeshPassProcessorRenderState DrawRenderState(View, PassUniformBuffer);

			FVelocityPassParallelCommandListSet ParallelCommandListSet(View,
				this,
				RHICmdList,
				CVarRHICmdVelocityPassDeferredContexts.GetValueOnRenderThread() > 0,
				CVarRHICmdFlushRenderThreadTasksVelocityPass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
				DrawRenderState,
				VelocityRT);

			// Draw velocities.
			View.ParallelMeshDrawCommandPasses[EMeshPass::Velocity].DispatchDraw(&ParallelCommandListSet, RHICmdList);
		}
	}
}

void FDeferredShadingSceneRenderer::RenderVelocitiesInner(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	check(RHICmdList.IsInsideRenderPass());
	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		
		FSceneTexturesUniformParameters SceneTextureParameters;
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SetupSceneTextureUniformParameters(SceneContext, View.FeatureLevel, ESceneTextureSetupMode::None, SceneTextureParameters);
		TUniformBufferRef<FSceneTexturesUniformParameters> PassUniformBuffer = TUniformBufferRef<FSceneTexturesUniformParameters>::CreateUniformBufferImmediate(SceneTextureParameters, UniformBuffer_SingleFrame);	

		FMeshPassProcessorRenderState DrawRenderState(View, PassUniformBuffer);

		if (View.ShouldRenderView())
		{
			SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

			Scene->UniformBuffers.UpdateViewUniformBuffer(View);

			SetVelocitiesState(RHICmdList, View, this, DrawRenderState, VelocityRT);

			View.ParallelMeshDrawCommandPasses[EMeshPass::Velocity].DispatchDraw(nullptr, RHICmdList);
		}
	}
}

bool FDeferredShadingSceneRenderer::ShouldRenderVelocities() const
{
	if (!GPixelFormats[PF_G16R16].Supported)
	{
		return false;
	}

	bool bNeedsVelocity = false;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		bool bTemporalAA = (View.AntiAliasingMethod == AAM_TemporalAA) && !View.bCameraCut;
		bool bMotionBlur = IsMotionBlurEnabled(View);
		bool bDistanceFieldAO = ShouldPrepareForDistanceFieldAO();

		bool bSSRTemporal = IsSSRTemporalPassRequired(View);

		bNeedsVelocity |= bMotionBlur || bTemporalAA || bDistanceFieldAO || bSSRTemporal;
	}

	return bNeedsVelocity;
}

void FDeferredShadingSceneRenderer::RenderVelocities(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderVelocities, FColor::Emerald);

	check(FeatureLevel >= ERHIFeatureLevel::SM4);
	SCOPE_CYCLE_COUNTER(STAT_RenderVelocities);

	if (!ShouldRenderVelocities())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, RenderVelocities);
	SCOPED_GPU_STAT(RHICmdList, RenderVelocities);

	FPooledRenderTargetDesc Desc = FVelocityRendering::GetRenderTargetDesc();
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VelocityRT, TEXT("Velocity"));

	{
		static const auto MotionBlurDebugVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurDebug"));

		if(MotionBlurDebugVar->GetValueOnRenderThread())
		{
			UE_LOG(LogEngine, Log, TEXT("r.MotionBlurDebug: FrameNumber=%d Pause=%d"), ViewFamily.FrameNumber, ViewFamily.bWorldIsPaused ? 1 : 0);
		}
	}

	{
		// In this case, basepass also outputs some of the velocities, so append is already started, and don't clear the buffer.
		BeginVelocityRendering(RHICmdList, VelocityRT, !FVelocityRendering::BasePassCanOutputVelocity(FeatureLevel));
	}

	{
		if (IsParallelVelocity())
		{
			// This initial renderpass will just be a clear in the parallel case.
			RHICmdList.EndRenderPass();

			// Now do parallel encoding.
			RenderVelocitiesInnerParallel(RHICmdList, VelocityRT);
		}
		else
		{
			RenderVelocitiesInner(RHICmdList, VelocityRT);
			RHICmdList.EndRenderPass();
		}

		RHICmdList.CopyToResolveTarget(VelocityRT->GetRenderTargetItem().TargetableTexture, VelocityRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	}

	// to be able to observe results with VisualizeTexture
	GVisualizeTexture.SetCheckPoint(RHICmdList, VelocityRT);
}

FPooledRenderTargetDesc FVelocityRendering::GetRenderTargetDesc()
{
	const FIntPoint BufferSize = FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY();
	const FIntPoint VelocityBufferSize = BufferSize;		// full resolution so we can reuse the existing full res z buffer
	return FPooledRenderTargetDesc(FPooledRenderTargetDesc::Create2DDesc(VelocityBufferSize, PF_G16R16, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
}

bool FVelocityRendering::BasePassCanOutputVelocity(EShaderPlatform ShaderPlatform)
{
	return !IsForwardShadingEnabled(ShaderPlatform) && CVarBasePassOutputsVelocity.GetValueOnAnyThread() == 1;
}

bool FVelocityRendering::BasePassCanOutputVelocity(ERHIFeatureLevel::Type FeatureLevel)
{
	EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	return BasePassCanOutputVelocity(ShaderPlatform);
}

bool FVelocityRendering::VertexFactoryOnlyOutputsVelocityInBasePass(EShaderPlatform ShaderPlatform, bool bVertexFactorySupportsStaticLighting)
{
	return BasePassCanOutputVelocity(ShaderPlatform) && !(UseSelectiveBasePassOutputs() && bVertexFactorySupportsStaticLighting);;
}

bool FVelocityRendering::PrimitiveHasVelocity(ERHIFeatureLevel::Type FeatureLevel, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// No velocity if motionblur is off, or if it's a non-moving object (treat as background in that case).
	if (!GPixelFormats[PF_G16R16].Supported || !PrimitiveSceneInfo->Proxy->IsMovable())
	{
		return false;
	}

	// If the base pass is allowed to render velocity in the GBuffer, only mesh with static lighting need the velocity pass.
	const bool bVelocityInGBuffer = FVelocityRendering::BasePassCanOutputVelocity(FeatureLevel) 
		&& !(UseSelectiveBasePassOutputs() && (PrimitiveSceneInfo->Proxy->HasStaticLighting()));

	if (bVelocityInGBuffer)
	{
		return false;
	}

	return true;
}

bool FVelocityRendering::PrimitiveHasVelocityForView(const FViewInfo& View, const FBoxSphereBounds& Bounds, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	if (View.bCameraCut)
	{
		return false;
	}

	const float LODFactorDistanceSquared = (Bounds.Origin - View.ViewMatrices.GetViewOrigin()).SizeSquared() * FMath::Square(View.LODDistanceFactor);

	// The minimum projected screen radius for a primitive to be drawn in the velocity pass, as a fraction of half the horizontal screen width (likely to be 0.08f)
	float MinScreenRadiusForVelocityPass = View.FinalPostProcessSettings.MotionBlurPerObjectSize * (2.0f / 100.0f);
	float MinScreenRadiusForVelocityPassSquared = FMath::Square(MinScreenRadiusForVelocityPass);

	// Skip primitives that only cover a small amount of screenspace, motion blur on them won't be noticeable.
	if (FMath::Square(Bounds.SphereRadius) <= MinScreenRadiusForVelocityPassSquared * LODFactorDistanceSquared)
	{
		return false;
	}

	if (PrimitiveSceneInfo->Proxy->AlwaysHasVelocity())
	{
		return true;
	}

	// check if the primitive has moved
	{
		const FScene* Scene = PrimitiveSceneInfo->Scene;

		const FMatrix& LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
		FMatrix PreviousLocalToWorld = LocalToWorld;
		Scene->VelocityData.GetComponentPreviousLocalToWorld(PrimitiveSceneInfo->PrimitiveComponentId, PreviousLocalToWorld);

		if (LocalToWorld.Equals(PreviousLocalToWorld, 0.0001f))
		{
			// Hasn't moved (treat as background by not rendering any special velocities)
			return false;
		}
	}

	return true;
}


void FVelocityMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	bool bRequiresSeparateVelocity = false;

	if (FVelocityRendering::PrimitiveHasVelocity(FeatureLevel, PrimitiveSceneProxy->GetPrimitiveSceneInfo()))
	{
		bRequiresSeparateVelocity = true;

		// Cached mesh commands have identical check inside MarkRelevant.
		if (ViewIfDynamicMeshCommand)
		{
			checkSlow(ViewIfDynamicMeshCommand->bIsViewInfo);
			FViewInfo* ViewInfo = (FViewInfo*)ViewIfDynamicMeshCommand;

			bRequiresSeparateVelocity = FVelocityRendering::PrimitiveHasVelocityForView(*ViewInfo, PrimitiveSceneProxy->GetBounds(), PrimitiveSceneProxy->GetPrimitiveSceneInfo());
		}
	}

	if (MeshBatch.bUseForMaterial && bRequiresSeparateVelocity)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
		const FMaterial* Material = &MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, MaterialRenderProxy);
		const EBlendMode BlendMode = Material->GetBlendMode();
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, *Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, *Material);

		if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
		{
			if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
			{
				// Default material doesn't handle masked or mesh-mod, and doesn't have the correct bIsTwoSided setting.
				MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
				Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
			}

			if (!MaterialRenderProxy)
			{
				MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
			}

			check(Material && MaterialRenderProxy);

			Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, MeshFillMode, MeshCullMode);
		}
	}
}

void GetVelocityPassShaders(
	const FMaterial& Material,
	FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	FVelocityHS*& HullShader,
	FVelocityDS*& DomainShader,
	FVelocityVS*& VertexShader,
	FVelocityPS*& PixelShader)
{
	const EMaterialTessellationMode MaterialTessellationMode = Material.GetTessellationMode();

	const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[FeatureLevel])
		&& VertexFactoryType->SupportsTessellationShaders()
		&& MaterialTessellationMode != MTM_NoTessellation;

	if (bNeedsHSDS)
	{
		DomainShader = Material.GetShader<FVelocityDS>(VertexFactoryType);
		HullShader = Material.GetShader<FVelocityHS>(VertexFactoryType);
	}

	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
	const bool bUseShaderPipelines = !bNeedsHSDS && CVar && CVar->GetValueOnAnyThread() != 0;

	FShaderPipeline* ShaderPipeline = bUseShaderPipelines ? Material.GetShaderPipeline(&VelocityPipeline, VertexFactoryType, false) : nullptr;
	if (ShaderPipeline)
	{
		VertexShader = ShaderPipeline->GetShader<FVelocityVS>();
		PixelShader = ShaderPipeline->GetShader<FVelocityPS>();
		check(VertexShader && PixelShader);
	}
	else
	{
		VertexShader = Material.GetShader<FVelocityVS>(VertexFactoryType);
		PixelShader = Material.GetShader<FVelocityPS>(VertexFactoryType);
		check(VertexShader && PixelShader);
	}
}

void FVelocityMeshProcessor::Process(
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

	TMeshProcessorShaders<
		FVelocityVS,
		FVelocityHS,
		FVelocityDS,
		FVelocityPS> VelocityPassShaders;

	GetVelocityPassShaders(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		VelocityPassShaders.HullShader,
		VelocityPassShaders.DomainShader,
		VelocityPassShaders.VertexShader,
		VelocityPassShaders.PixelShader
	);

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(VelocityPassShaders.VertexShader, VelocityPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		VelocityPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

FVelocityMeshProcessor::FVelocityMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState = InPassDrawRenderState;
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.VelocityPassUniformBuffer);
}

FMeshPassProcessor* CreateVelocityPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState VelocityPassState;
	VelocityPassState.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	VelocityPassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	return new(FMemStack::Get()) FVelocityMeshProcessor(Scene, InViewIfDynamicMeshCommand, VelocityPassState, InDrawListContext);
}

FRegisterPassProcessorCreateFunction RegisterVelocityPass(&CreateVelocityPassProcessor, EShadingPath::Deferred,  EMeshPass::Velocity, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);