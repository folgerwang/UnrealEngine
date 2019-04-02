// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneHitProxyRendering.cpp: Scene hit proxy rendering.
=============================================================================*/

#include "SceneHitProxyRendering.h"
#include "RendererInterface.h"
#include "BatchedElements.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "DynamicPrimitiveDrawing.h"
#include "ClearQuad.h"
#include "VisualizeTexture.h"
#include "MeshPassProcessor.inl"
#include "GPUScene.h"

class FHitProxyShaderElementData : public FMeshMaterialShaderElementData
{
public:

	FHitProxyShaderElementData(FHitProxyId InBatchHitProxyId)
		: BatchHitProxyId(InBatchHitProxyId)
	{
	}

	FHitProxyId BatchHitProxyId;
};

/**
 * A vertex shader for rendering the depth of a mesh.
 */
class FHitProxyVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FHitProxyVS,MeshMaterial);

public:

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile the hit proxy vertex shader on PC
		return IsPCPlatform(Platform)
			// and only compile for the default material or materials that are masked.
			&& (Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() || Material->MaterialMayModifyMeshPosition() || Material->IsTwoSided());
	}

protected:

	FHitProxyVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}
	FHitProxyVS() {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyVS,TEXT("/Engine/Private/HitProxyVertexShader.usf"),TEXT("Main"),SF_Vertex); 

/**
 * A hull shader for rendering the depth of a mesh.
 */
class FHitProxyHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FHitProxyHS,MeshMaterial);
protected:

	FHitProxyHS() {}

	FHitProxyHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBaseHS(Initializer)
	{}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& FHitProxyVS::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}
};

/**
 * A domain shader for rendering the depth of a mesh.
 */
class FHitProxyDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FHitProxyDS,MeshMaterial);

protected:

	FHitProxyDS() {}

	FHitProxyDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBaseDS(Initializer)
	{}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& FHitProxyVS::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyHS,TEXT("/Engine/Private/HitProxyVertexShader.usf"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyDS,TEXT("/Engine/Private/HitProxyVertexShader.usf"),TEXT("MainDomain"),SF_Domain);

/**
 * A pixel shader for rendering the HitProxyId of an object as a unique color in the scene.
 */
class FHitProxyPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FHitProxyPS,MeshMaterial);
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile the hit proxy vertex shader on PC
		return IsPCPlatform(Platform) 
			// and only compile for default materials or materials that are masked.
			&& (Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() || Material->MaterialMayModifyMeshPosition() || Material->IsTwoSided());
	}

	FHitProxyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		HitProxyId.Bind(Initializer.ParameterMap,TEXT("HitProxyId"), SPF_Mandatory);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FHitProxyPS() {}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FHitProxyShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);


		FHitProxyId hitProxyId = ShaderElementData.BatchHitProxyId;

		if (PrimitiveSceneProxy && ShaderElementData.BatchHitProxyId == FHitProxyId())
		{
			hitProxyId = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->DefaultDynamicHitProxyId;
		}

		// Per-instance hitproxies are supplied by the vertex factory.
		if (PrimitiveSceneProxy && PrimitiveSceneProxy->HasPerInstanceHitProxies())
		{
			hitProxyId = FColor(0);
		}

		ShaderBindings.Add(HitProxyId, hitProxyId.GetColor().ReinterpretAsLinear());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << HitProxyId;
		return bShaderHasOutdatedParameters;
	}


private:
	FShaderParameter HitProxyId;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyPS,TEXT("/Engine/Private/HitProxyPixelShader.usf"),TEXT("Main"),SF_Pixel);

#if WITH_EDITOR

void InitHitProxyRender(FRHICommandListImmediate& RHICmdList, const FSceneRenderer* SceneRenderer, TRefCountPtr<IPooledRenderTarget>& OutHitProxyRT, TRefCountPtr<IPooledRenderTarget>& OutHitProxyDepthRT)
{
	check(!RHICmdList.IsInsideRenderPass());

	auto& ViewFamily = SceneRenderer->ViewFamily;
	auto FeatureLevel = ViewFamily.Scene->GetFeatureLevel();

	// Initialize global system textures (pass-through if already initialized).
	GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	// Allocate the maximum scene render target space for the current view family.
	SceneContext.Allocate(RHICmdList, SceneRenderer);

	// Create a texture to store the resolved light attenuation values, and a render-targetable surface to hold the unresolved light attenuation values.
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutHitProxyRT, TEXT("HitProxy"));

		// create non-MSAA version for hit proxies on PC if needed
		const EShaderPlatform CurrentShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
		FPooledRenderTargetDesc DepthDesc = SceneContext.SceneDepthZ->GetDesc();

		if (DepthDesc.NumSamples > 1 && RHISupportsSeparateMSAAAndResolveTextures(CurrentShaderPlatform))
		{
			DepthDesc.NumSamples = 1;
			GRenderTargetPool.FindFreeElement(RHICmdList, DepthDesc, OutHitProxyDepthRT, TEXT("NoMSAASceneDepthZ"));
		}
		else
		{
			OutHitProxyDepthRT = SceneContext.SceneDepthZ;
		}
	}
}

static void BeginHitProxyRenderpass(FRHICommandListImmediate& RHICmdList, const FSceneRenderer* SceneRenderer, TRefCountPtr<IPooledRenderTarget> HitProxyRT, TRefCountPtr<IPooledRenderTarget> HitProxyDepthRT)
{
	FRHIRenderPassInfo RPInfo(HitProxyRT->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_Store);
	RPInfo.DepthStencilRenderTarget.Action = EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil;
	RPInfo.DepthStencilRenderTarget.DepthStencilTarget = HitProxyDepthRT->GetRenderTargetItem().TargetableTexture;
	RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
	TransitionRenderPassTargets(RHICmdList, RPInfo);

	RHICmdList.BeginRenderPass(RPInfo, TEXT("Clear_HitProxies"));
	{
		// Clear color for each view.
		auto& Views = SceneRenderer->Views;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			DrawClearQuad(RHICmdList, true, FLinearColor::White, false, 0, false, 0, HitProxyRT->GetDesc().Extent, FIntRect());
		}
	}
}

static void DoRenderHitProxies(FRHICommandListImmediate& RHICmdList, const FSceneRenderer* SceneRenderer, TRefCountPtr<IPooledRenderTarget> HitProxyRT, TRefCountPtr<IPooledRenderTarget> HitProxyDepthRT)
{
	BeginHitProxyRenderpass(RHICmdList, SceneRenderer, HitProxyRT, HitProxyDepthRT);

	auto & ViewFamily = SceneRenderer->ViewFamily;
	auto & Views = SceneRenderer->Views;

	const auto FeatureLevel = SceneRenderer->FeatureLevel;

	const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[SceneRenderer->FeatureLevel]);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		const FScene* LocalScene = SceneRenderer->Scene;

		SceneRenderer->Scene->UniformBuffers.UpdateViewUniformBuffer(View);

		FSceneTexturesUniformParameters SceneTextureParameters;
		SetupSceneTextureUniformParameters(SceneContext, View.FeatureLevel, ESceneTextureSetupMode::None, SceneTextureParameters);
		SceneRenderer->Scene->UniformBuffers.HitProxyPassUniformBuffer.UpdateUniformBufferImmediate(SceneTextureParameters);

		FMeshPassProcessorRenderState DrawRenderState(View, SceneRenderer->Scene->UniformBuffers.HitProxyPassUniformBuffer);

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		// Clear the depth buffer for each DPG.
		DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, HitProxyDepthRT->GetDesc().Extent, FIntRect());

		// Depth tests + writes, no alpha blending.
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

		const bool bHitTesting = true;

		// Adjust the visibility map for this view
		if (View.bAllowTranslucentPrimitivesInHitProxy)
		{
			View.ParallelMeshDrawCommandPasses[EMeshPass::HitProxy].DispatchDraw(nullptr, RHICmdList);
		}
		else
		{
			View.ParallelMeshDrawCommandPasses[EMeshPass::HitProxyOpaqueOnly].DispatchDraw(nullptr, RHICmdList);
		}

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState, LocalScene](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FHitProxyMeshProcessor PassMeshProcessor(
				LocalScene,
				&View,
				View.bAllowTranslucentPrimitivesInHitProxy,
				DrawRenderState,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (int32 MeshIndex = 0; MeshIndex < View.DynamicEditorMeshElements.Num(); MeshIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicEditorMeshElements[MeshIndex];

				if (MeshBatchAndRelevance.Mesh->bSelectable)
				{
					PassMeshProcessor.AddMeshBatch(*MeshBatchAndRelevance.Mesh, DefaultBatchElementMask, MeshBatchAndRelevance.PrimitiveSceneProxy);
				}
			}
		});

		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::All, SDPG_World);
		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::All, SDPG_Foreground);

		View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::All, SDPG_World);
		View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::All, SDPG_Foreground);

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState, LocalScene](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FHitProxyMeshProcessor PassMeshProcessor(
				LocalScene,
				&View,
				View.bAllowTranslucentPrimitivesInHitProxy,
				DrawRenderState,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
			{
				const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
				PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
			}
		});

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState, LocalScene](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FHitProxyMeshProcessor PassMeshProcessor(
				LocalScene,
				&View,
				View.bAllowTranslucentPrimitivesInHitProxy,
				DrawRenderState,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
			{
				const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
				PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
			}
		});


		// Draw the view's batched simple elements(lines, sprites, etc).
		View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, true);

		// Some elements should never be occluded (e.g. gizmos).
		// So we render those twice, first to overwrite potentially nearer objects,
		// then again to allows proper occlusion within those elements.
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState, LocalScene](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FHitProxyMeshProcessor PassMeshProcessor(
					LocalScene,
					&View, 
					View.bAllowTranslucentPrimitivesInHitProxy, 
					DrawRenderState,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;
					
				for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

		View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, true);

		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState, LocalScene](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FHitProxyMeshProcessor PassMeshProcessor(
					LocalScene,
					&View, 
					View.bAllowTranslucentPrimitivesInHitProxy, 
					DrawRenderState,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;
					
				for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

		View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, true);
	}
	// Was started in init, but ends here.
	RHICmdList.EndRenderPass();

	// Finish drawing to the hit proxy render target.
	RHICmdList.CopyToResolveTarget(HitProxyRT->GetRenderTargetItem().TargetableTexture, HitProxyRT->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
	RHICmdList.CopyToResolveTarget(SceneContext.GetSceneDepthSurface(), SceneContext.GetSceneDepthSurface(), FResolveParams());

	// to be able to observe results with VisualizeTexture
	GVisualizeTexture.SetCheckPoint(RHICmdList, HitProxyRT);

	//
	// Copy the hit proxy buffer into the view family's render target.
	//
	
	// Set up a FTexture that is used to draw the hit proxy buffer to the view family's render target.
	FTexture HitProxyRenderTargetTexture;
	HitProxyRenderTargetTexture.TextureRHI = HitProxyRT->GetRenderTargetItem().ShaderResourceTexture;
	HitProxyRenderTargetTexture.SamplerStateRHI = TStaticSamplerState<>::GetRHI();

	// Generate the vertices and triangles mapping the hit proxy RT pixels into the view family's RT pixels.
	FBatchedElements BatchedElements;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();
		float InvBufferSizeX = 1.0f / BufferSize.X;
		float InvBufferSizeY = 1.0f / BufferSize.Y;

		const float U0 = View.ViewRect.Min.X * InvBufferSizeX;
		const float V0 = View.ViewRect.Min.Y * InvBufferSizeY;
		const float U1 = View.ViewRect.Max.X * InvBufferSizeX;
		const float V1 = View.ViewRect.Max.Y * InvBufferSizeY;

		// Note: High DPI .  We are drawing to the size of the unscaled view rect because that is the size of the views render target
		// if we do not do this clicking would be off.
		const int32 V00 = BatchedElements.AddVertex(FVector4(View.UnscaledViewRect.Min.X,	View.UnscaledViewRect.Min.Y, 0, 1),FVector2D(U0, V0),	FLinearColor::White, FHitProxyId());
		const int32 V10 = BatchedElements.AddVertex(FVector4(View.UnscaledViewRect.Max.X,	View.UnscaledViewRect.Min.Y,	0, 1),FVector2D(U1, V0),	FLinearColor::White, FHitProxyId());
		const int32 V01 = BatchedElements.AddVertex(FVector4(View.UnscaledViewRect.Min.X,	View.UnscaledViewRect.Max.Y,	0, 1),FVector2D(U0, V1),	FLinearColor::White, FHitProxyId());
		const int32 V11 = BatchedElements.AddVertex(FVector4(View.UnscaledViewRect.Max.X,	View.UnscaledViewRect.Max.Y,	0, 1),FVector2D(U1, V1),	FLinearColor::White, FHitProxyId());

		BatchedElements.AddTriangle(V00,V10,V11,&HitProxyRenderTargetTexture,BLEND_Opaque);
		BatchedElements.AddTriangle(V00,V11,V01,&HitProxyRenderTargetTexture,BLEND_Opaque);
	}

	// Generate a transform which maps from view family RT pixel coordinates to Normalized Device Coordinates.
	FIntPoint RenderTargetSize = ViewFamily.RenderTarget->GetSizeXY();

	const FMatrix PixelToView =
		FTranslationMatrix(FVector(0, 0, 0)) *
			FMatrix(
				FPlane(	1.0f / ((float)RenderTargetSize.X / 2.0f),	0.0,										0.0f,	0.0f	),
				FPlane(	0.0f,										-GProjectionSignY / ((float)RenderTargetSize.Y / 2.0f),	0.0f,	0.0f	),
				FPlane(	0.0f,										0.0f,										1.0f,	0.0f	),
				FPlane(	-1.0f,										GProjectionSignY,										0.0f,	1.0f	)
				);

	{
		// Draw the triangles to the view family's render target.
		FRHIRenderPassInfo RPInfo(ViewFamily.RenderTarget->GetRenderTargetTexture(), ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("HitProxies"));
		{
			FSceneView SceneView = FBatchedElements::CreateProxySceneView(PixelToView, FIntRect(0, 0, RenderTargetSize.X, RenderTargetSize.Y));
			FMeshPassProcessorRenderState DrawRenderState(SceneView);

			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

			BatchedElements.Draw(
				RHICmdList,
				DrawRenderState,
				FeatureLevel,
				bNeedToSwitchVerticalAxis,
				SceneView,
				false,
				1.0f
			);
		}
		RHICmdList.EndRenderPass();
	}

	RHICmdList.EndScene();
}
#endif

void FMobileSceneRenderer::RenderHitProxies(FRHICommandListImmediate& RHICmdList)
{
	PrepareViewRectsForRendering();

#if WITH_EDITOR
	TRefCountPtr<IPooledRenderTarget> HitProxyRT;
	TRefCountPtr<IPooledRenderTarget> HitProxyDepthRT;
	InitHitProxyRender(RHICmdList, this, HitProxyRT, HitProxyDepthRT);
	// HitProxyRT==0 should never happen but better we don't crash
	if (HitProxyRT)
	{
		// Find the visible primitives.
		InitViews(RHICmdList);

		GEngine->GetPreRenderDelegate().Broadcast();

		// Global dynamic buffers need to be committed before rendering.
		DynamicIndexBuffer.Commit();
		DynamicVertexBuffer.Commit();
		DynamicReadBuffer.Commit();

		::DoRenderHitProxies(RHICmdList, this, HitProxyRT, HitProxyDepthRT);
	}

	check(RHICmdList.IsOutsideRenderPass());

#endif
}

void FDeferredShadingSceneRenderer::RenderHitProxies(FRHICommandListImmediate& RHICmdList)
{
	PrepareViewRectsForRendering();

#if WITH_EDITOR
	// HitProxyRT==0 should never happen but better we don't crash
	TRefCountPtr<IPooledRenderTarget> HitProxyRT;
	TRefCountPtr<IPooledRenderTarget> HitProxyDepthRT;
	InitHitProxyRender(RHICmdList, this, HitProxyRT, HitProxyDepthRT);
	if (HitProxyRT)
	{
		// Find the visible primitives.
		FGraphEventArray UpdateViewCustomDataEvents;
		FILCUpdatePrimTaskData ILCTaskData;
		bool bDoInitViewAftersPrepass = InitViews(RHICmdList, FExclusiveDepthStencil::DepthWrite_StencilWrite, ILCTaskData, UpdateViewCustomDataEvents);
		if (bDoInitViewAftersPrepass)
		{
			InitViewsPossiblyAfterPrepass(RHICmdList, ILCTaskData, UpdateViewCustomDataEvents);
		}

		UpdateGPUScene(RHICmdList, *Scene);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			UploadDynamicPrimitiveShaderDataForView(RHICmdList, *Scene, Views[ViewIndex]);
		}	

		if (UpdateViewCustomDataEvents.Num())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AsyncUpdateViewCustomData_Wait);
			FTaskGraphInterface::Get().WaitUntilTasksComplete(UpdateViewCustomDataEvents, ENamedThreads::GetRenderThread());
		}

		GEngine->GetPreRenderDelegate().Broadcast();

		// Global dynamic buffers need to be committed before rendering.
		DynamicIndexBufferForInitViews.Commit();
		DynamicVertexBufferForInitViews.Commit();
		DynamicReadBufferForInitViews.Commit();

		::DoRenderHitProxies(RHICmdList, this, HitProxyRT, HitProxyDepthRT);
		ClearPrimitiveSingleFrameIndirectLightingCacheBuffers();
	}
	check(RHICmdList.IsOutsideRenderPass());
#endif
}

#if WITH_EDITOR

void FHitProxyMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (ViewIfDynamicMeshCommand && ViewIfDynamicMeshCommand->bAllowTranslucentPrimitivesInHitProxy != bAllowTranslucentPrimitivesInHitProxy)
	{
		return;
	}

	if (MeshBatch.bUseForMaterial && MeshBatch.bSelectable && Scene->RequiresHitProxies() && (!PrimitiveSceneProxy || PrimitiveSceneProxy->IsSelectable()))
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
		const FMaterial* Material = &MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, MaterialRenderProxy);
		const EBlendMode BlendMode = Material->GetBlendMode();
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, *Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, *Material);

		if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
		{
			// Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
			MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
		}

		if (!MaterialRenderProxy)
		{
			MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		}

		check(Material && MaterialRenderProxy);

		if (bAllowTranslucentPrimitivesInHitProxy || !IsTranslucentBlendMode(BlendMode))
		{
			Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, MeshFillMode, MeshCullMode);
		}
	}
}

void GetHitProxyPassShaders(
	const FMaterial& Material,
	FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	FHitProxyHS*& HullShader,
	FHitProxyDS*& DomainShader,
	FHitProxyVS*& VertexShader,
	FHitProxyPS*& PixelShader)
{
	const EMaterialTessellationMode MaterialTessellationMode = Material.GetTessellationMode();

	const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[FeatureLevel])
		&& VertexFactoryType->SupportsTessellationShaders()
		&& MaterialTessellationMode != MTM_NoTessellation;

	if (bNeedsHSDS)
	{
		DomainShader = Material.GetShader<FHitProxyDS>(VertexFactoryType);
		HullShader = Material.GetShader<FHitProxyHS>(VertexFactoryType);
	}

	VertexShader = Material.GetShader<FHitProxyVS>(VertexFactoryType);
	PixelShader = Material.GetShader<FHitProxyPS>(VertexFactoryType);
}

void FHitProxyMeshProcessor::Process(
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
		FHitProxyVS,
		FHitProxyHS,
		FHitProxyDS,
		FHitProxyPS> HitProxyPassShaders;

	GetHitProxyPassShaders(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		HitProxyPassShaders.HullShader,
		HitProxyPassShaders.DomainShader,
		HitProxyPassShaders.VertexShader,
		HitProxyPassShaders.PixelShader
	);

	FHitProxyShaderElementData ShaderElementData(MeshBatch.BatchHitProxyId);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(HitProxyPassShaders.VertexShader, HitProxyPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		HitProxyPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

FHitProxyMeshProcessor::FHitProxyMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, bool InbAllowTranslucentPrimitivesInHitProxy, const FMeshPassProcessorRenderState& InRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InRenderState)
	, bAllowTranslucentPrimitivesInHitProxy(InbAllowTranslucentPrimitivesInHitProxy)
{
}

FMeshPassProcessor* CreateHitProxyPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.HitProxyPassUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	return new(FMemStack::Get()) FHitProxyMeshProcessor(Scene, InViewIfDynamicMeshCommand, true, PassDrawRenderState, InDrawListContext);
}

FMeshPassProcessor* CreateHitProxyOpaqueOnlyPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.HitProxyPassUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	return new(FMemStack::Get()) FHitProxyMeshProcessor(Scene, InViewIfDynamicMeshCommand, false, PassDrawRenderState, InDrawListContext);
}

FRegisterPassProcessorCreateFunction RegisterHitProxyPass(&CreateHitProxyPassProcessor, EShadingPath::Deferred, EMeshPass::HitProxy, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterHitProxyOpaqueOnlyPass(&CreateHitProxyOpaqueOnlyPassProcessor, EShadingPath::Deferred, EMeshPass::HitProxyOpaqueOnly, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileHitProxyPass(&CreateHitProxyPassProcessor, EShadingPath::Mobile, EMeshPass::HitProxy, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileHitProxyOpaqueOnlyPass(&CreateHitProxyOpaqueOnlyPassProcessor, EShadingPath::Mobile, EMeshPass::HitProxyOpaqueOnly, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);


void FEditorSelectionMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial 
		&& MeshBatch.bUseSelectionOutline 
		&& PrimitiveSceneProxy->WantsSelectionOutline() 
		&& (PrimitiveSceneProxy->IsSelected() || PrimitiveSceneProxy->IsHovered()))
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
		const FMaterial* Material = &MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, MaterialRenderProxy);

		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, *Material);
		const ERasterizerCullMode MeshCullMode = CM_None;

		if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
		{
			// Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
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

void FEditorSelectionMeshProcessor::Process(
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
		FHitProxyVS,
		FHitProxyHS,
		FHitProxyDS,
		FHitProxyPS> HitProxyPassShaders;

	GetHitProxyPassShaders(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		HitProxyPassShaders.HullShader,
		HitProxyPassShaders.DomainShader,
		HitProxyPassShaders.VertexShader,
		HitProxyPassShaders.PixelShader
	);

	const int32 StencilRef = GetStencilValue(ViewIfDynamicMeshCommand, PrimitiveSceneProxy);
	PassDrawRenderState.SetStencilRef(StencilRef);

	FHitProxyId DummyId;
	FHitProxyShaderElementData ShaderElementData(DummyId);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(HitProxyPassShaders.VertexShader, HitProxyPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		HitProxyPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

int32 FEditorSelectionMeshProcessor::GetStencilValue(const FSceneView* View, const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	const bool bActorSelectionColorIsSubdued = View->bHasSelectedComponents;

	const int32* ExistingStencilValue = PrimitiveSceneProxy->IsIndividuallySelected() ? ProxyToStencilIndex.Find(PrimitiveSceneProxy) : ActorNameToStencilIndex.Find(PrimitiveSceneProxy->GetOwnerName());

	int32 StencilValue = 0;

	if (PrimitiveSceneProxy->GetOwnerName() == NAME_BSP)
	{
		StencilValue = 1;
	}
	else if (ExistingStencilValue != nullptr)
	{
		StencilValue = *ExistingStencilValue;
	}
	else if (PrimitiveSceneProxy->IsIndividuallySelected())
	{
		// Any component that is individually selected should have a stencil value of < 128 so that it can have a unique color.  We offset the value by 2 because 0 means no selection and 1 is for bsp
		StencilValue = ProxyToStencilIndex.Num() % 126 + 2;
		ProxyToStencilIndex.Add(PrimitiveSceneProxy, StencilValue);
	}
	else
	{
		// If we are subduing actor color highlight then use the top level bits to indicate that to the shader.  
		StencilValue = bActorSelectionColorIsSubdued ? ActorNameToStencilIndex.Num() % 128 + 128 : ActorNameToStencilIndex.Num() % 126 + 2;
		ActorNameToStencilIndex.Add(PrimitiveSceneProxy->GetOwnerName(), StencilValue);
	}

	return StencilValue;
}

FEditorSelectionMeshProcessor::FEditorSelectionMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	checkf(InViewIfDynamicMeshCommand, TEXT("Editor selection mesh process required dynamic mesh command mode."));

	ActorNameToStencilIndex.Add(NAME_BSP, 1);

	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI());
	PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI());
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.EditorSelectionPassUniformBuffer);
}

FMeshPassProcessor* CreateEditorSelectionPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	return new(FMemStack::Get()) FEditorSelectionMeshProcessor(Scene, InViewIfDynamicMeshCommand, InDrawListContext);
}

FRegisterPassProcessorCreateFunction RegisterEditorSelectionPass(&CreateEditorSelectionPassProcessor, EShadingPath::Deferred, EMeshPass::EditorSelection, EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileEditorSelectionPass(&CreateEditorSelectionPassProcessor, EShadingPath::Mobile, EMeshPass::EditorSelection, EMeshPassFlags::MainView);

#endif