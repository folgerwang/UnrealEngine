// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistortionRendering.cpp: Distortion rendering implementation.
=============================================================================*/

#include "DistortionRendering.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "Materials/Material.h"
#include "UnrealEngine.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "VisualizeTexture.h"
#include "MeshPassProcessor.inl"

DECLARE_GPU_STAT(Distortion);

const uint8 kStencilMaskBit = STENCIL_SANDBOX_MASK;

static TAutoConsoleVariable<int32> CVarDisableDistortion(
														 TEXT("r.DisableDistortion"),
														 0,
														 TEXT("Prevents distortion effects from rendering.  Saves a full-screen framebuffer's worth of memory."),
														 ECVF_Default);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FDistortionPassUniformParameters, "DistortionPass");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileDistortionPassUniformParameters, "MobileDistortionPass");

void SetupDistortionPassUniformBuffer(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FDistortionPassUniformParameters& DistortionPassParameters)
{
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
	SetupSceneTextureUniformParameters(SceneRenderTargets, View.FeatureLevel, ESceneTextureSetupMode::All, DistortionPassParameters.SceneTextures);

	float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();
	DistortionPassParameters.DistortionParams.X = View.ViewMatrices.GetProjectionMatrix().M[0][0];
	DistortionPassParameters.DistortionParams.Y = Ratio;
	DistortionPassParameters.DistortionParams.Z = (float)View.UnscaledViewRect.Width();
	DistortionPassParameters.DistortionParams.W = (float)View.UnscaledViewRect.Height();

	// When ISR is enabled we store two FOVs in the distortion parameters and compute the aspect ratio in the shader instead.
	if ((View.IsInstancedStereoPass() || View.bIsMobileMultiViewEnabled) && View.Family->Views.Num() > 0)
	{
		// When drawing the left eye in a stereo scene, copy the right eye view values into the instanced view uniform buffer.
		const EStereoscopicPass StereoPassIndex = (View.StereoPass != eSSP_FULL) ? eSSP_RIGHT_EYE : eSSP_FULL;

		const FViewInfo& InstancedView = static_cast<const FViewInfo&>(View.Family->GetStereoEyeView(StereoPassIndex));
		DistortionPassParameters.DistortionParams.Y = InstancedView.ViewMatrices.GetProjectionMatrix().M[0][0];
	}
}

void SetupMobileDistortionPassUniformBuffer(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FMobileDistortionPassUniformParameters& DistortionPassParameters)
{
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
	SetupMobileSceneTextureUniformParameters(SceneRenderTargets, View.FeatureLevel, true, DistortionPassParameters.SceneTextures);

	float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();
	DistortionPassParameters.DistortionParams.X = View.ViewMatrices.GetProjectionMatrix().M[0][0];
	DistortionPassParameters.DistortionParams.Y = Ratio;
	DistortionPassParameters.DistortionParams.Z = (float)View.UnscaledViewRect.Width();
	DistortionPassParameters.DistortionParams.W = (float)View.UnscaledViewRect.Height();

	// When ISR is enabled we store two FOVs in the distortion parameters and compute the aspect ratio in the shader instead.
	if ((View.IsInstancedStereoPass() || View.bIsMobileMultiViewEnabled) && View.Family->Views.Num() > 0)
	{
		// When drawing the left eye in a stereo scene, copy the right eye view values into the instanced view uniform buffer.
		const EStereoscopicPass StereoPassIndex = (View.StereoPass != eSSP_FULL) ? eSSP_RIGHT_EYE : eSSP_FULL;

		const FViewInfo& InstancedView = static_cast<const FViewInfo&>(View.Family->GetStereoEyeView(StereoPassIndex));
		DistortionPassParameters.DistortionParams.Y = InstancedView.ViewMatrices.GetProjectionMatrix().M[0][0];
	}
}

/**
* A pixel shader for rendering the full screen refraction pass
*/
template <bool UseMSAA>
class TDistortionApplyScreenPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistortionApplyScreenPS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !UseMSAA || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	TDistortionApplyScreenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		if (UseMSAA)
		{
			DistortionTexture.Bind(Initializer.ParameterMap, TEXT("DistortionMSAATexture"));
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorMSAATexture"));
		}
		else
		{
			DistortionTexture.Bind(Initializer.ParameterMap, TEXT("DistortionTexture"));
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorTexture"));
		}
		DistortionTextureSampler.Bind(Initializer.ParameterMap,TEXT("DistortionTextureSampler"));
		SceneColorTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
	}
	TDistortionApplyScreenPS() {}

	void SetParameters(const FRenderingCompositePassContext& Context, const FViewInfo& View, IPooledRenderTarget& DistortionRT)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		FTextureRHIParamRef DistortionTextureValue = DistortionRT.GetRenderTargetItem().TargetableTexture;
		FTextureRHIParamRef SceneColorTextureValue = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;

		// Here we use SF_Point as in fullscreen the pixels are 1:1 mapped.
		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			DistortionTexture,
			DistortionTextureSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistortionTextureValue
			);

		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			SceneColorTexture,
			SceneColorTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			SceneColorTextureValue
			);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DistortionTexture << DistortionTextureSampler << SceneColorTexture << SceneColorTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/DistortApplyScreenPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

private:
	FShaderResourceParameter DistortionTexture;
	FShaderResourceParameter DistortionTextureSampler;
	FShaderResourceParameter SceneColorTexture;
	FShaderResourceParameter SceneColorTextureSampler;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_MSAA"), UseMSAA ? 1 : 0);
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) \
	typedef TDistortionApplyScreenPS<A> TDistortionApplyScreenPS##A; \
	IMPLEMENT_SHADER_TYPE2(TDistortionApplyScreenPS##A,SF_Pixel)
VARIATION1(false);
VARIATION1(true);
#undef VARIATION1

/**
* A pixel shader that applies the distorted image to the scene
*/
template <bool UseMSAA>
class TDistortionMergePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistortionMergePS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !UseMSAA || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	TDistortionMergePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		if (UseMSAA)
		{
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorMSAATexture"));
		}
		else
		{
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorTexture"));
		}
		SceneColorTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
	}
	TDistortionMergePS() {}

	void SetParameters(const FRenderingCompositePassContext& Context, const FViewInfo& View, const FTextureRHIParamRef& PassTexture)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			SceneColorTexture,
			SceneColorTextureSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			PassTexture
			);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneColorTexture << SceneColorTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/DistortApplyScreenPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Merge");
	}

private:
	FShaderResourceParameter SceneColorTexture;
	FShaderResourceParameter SceneColorTextureSampler;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_MSAA"), UseMSAA ? 1 : 0);
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) \
	typedef TDistortionMergePS<A> TDistortionMergePS##A; \
	IMPLEMENT_SHADER_TYPE2(TDistortionMergePS##A,SF_Pixel)
VARIATION1(false);
VARIATION1(true);
#undef VARIATION1

/**
* A vertex shader for rendering distortion meshes
*/
class FDistortionMeshVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FDistortionMeshVS,MeshMaterial);

protected:

	FDistortionMeshVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		ERHIFeatureLevel::Type FeatureLevel = GetMaxSupportedFeatureLevel((EShaderPlatform)Initializer.Target.Platform);
		if (FSceneInterface::GetShadingPath(FeatureLevel) == EShadingPath::Mobile)
		{
			PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileDistortionPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		}
		else // deferred
		{
			PassUniformBuffer.Bind(Initializer.ParameterMap, FDistortionPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		}
	}

	FDistortionMeshVS()
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material && IsTranslucentBlendMode(Material->GetBlendMode()) && Material->IsDistorted();
	}
};


/**
 * A hull shader for rendering distortion meshes
 */
class FDistortionMeshHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FDistortionMeshHS,MeshMaterial);

protected:

	FDistortionMeshHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseHS(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FDistortionPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FDistortionMeshHS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& Material && IsTranslucentBlendMode(Material->GetBlendMode()) && Material->IsDistorted();
	}
};

/**
 * A domain shader for rendering distortion meshes
 */
class FDistortionMeshDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FDistortionMeshDS,MeshMaterial);

protected:

	FDistortionMeshDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseDS(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FDistortionPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FDistortionMeshDS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& Material && IsTranslucentBlendMode(Material->GetBlendMode()) && Material->IsDistorted();
	}
};


IMPLEMENT_MATERIAL_SHADER_TYPE(, FDistortionMeshVS, TEXT("/Engine/Private/DistortAccumulateVS.usf"), TEXT("Main"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FDistortionMeshHS, TEXT("/Engine/Private/DistortAccumulateVS.usf"), TEXT("MainHull"), SF_Hull);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FDistortionMeshDS, TEXT("/Engine/Private/DistortAccumulateVS.usf"), TEXT("MainDomain"), SF_Domain);


/**
* A pixel shader to render distortion meshes
*/
class FDistortionMeshPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FDistortionMeshPS,MeshMaterial);

public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material && IsTranslucentBlendMode(Material->GetBlendMode()) && Material->IsDistorted();
	}

	FDistortionMeshPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
		ERHIFeatureLevel::Type FeatureLevel = GetMaxSupportedFeatureLevel((EShaderPlatform)Initializer.Target.Platform);
		if (FSceneInterface::GetShadingPath(FeatureLevel) == EShadingPath::Mobile)
		{
			PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileDistortionPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		}
		else // deferred
		{
			PassUniformBuffer.Bind(Initializer.ParameterMap, FDistortionPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		}
	}

	FDistortionMeshPS()
	{
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

//** distortion accumulate pixel shader type implementation */
IMPLEMENT_MATERIAL_SHADER_TYPE(, FDistortionMeshPS,TEXT("/Engine/Private/DistortAccumulatePS.usf"),TEXT("Main"),SF_Pixel);

int32 FSceneRenderer::GetRefractionQuality(const FSceneViewFamily& ViewFamily)
{
	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RefractionQuality"));

	int32 Value = 0;

	if(ViewFamily.EngineShowFlags.Refraction)
	{
		Value = ICVar->GetValueOnRenderThread();
	}

	return Value;
}

template <bool UseMSAA>
static void DrawDistortionApplyScreenPass(FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext, FViewInfo& View, IPooledRenderTarget& DistortionRT) {
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TDistortionApplyScreenPS<UseMSAA>> PixelShader(View.ShaderMap);

	FRenderingCompositePassContext Context(RHICmdList, View);

	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// test against stencil mask
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	RHICmdList.SetStencilRef(kStencilMaskBit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, View, DistortionRT);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);
}

template <bool UseMSAA>
static void DrawDistortionMergePass(FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext, FViewInfo& View, const FTextureRHIParamRef& PassTexture) {
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TDistortionMergePS<UseMSAA>> PixelShader(View.ShaderMap);

	FRenderingCompositePassContext Context(RHICmdList, View);

	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// test against stencil mask and clear it
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Zero,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	RHICmdList.SetStencilRef(kStencilMaskBit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, View, PassTexture);

	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);
}

bool SubmitDistortionMeshDrawCommands(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets);

	bool bDirty = false;

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets_Prims);
		View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].DispatchDraw(nullptr, RHICmdList);
		bDirty |= View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].HasAnyDraw();
	}

	return bDirty;
}

static void SetupDistortionPassView(FRHICommandList& RHICmdList, const FViewInfo& View, const FSceneRenderer* SceneRenderer)
{
	if (!View.IsInstancedStereoPass())
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	}
	else
	{
		if (View.bIsMultiViewEnabled)
		{
			const uint32 LeftMinX = SceneRenderer->Views[0].ViewRect.Min.X;
			const uint32 LeftMaxX = SceneRenderer->Views[0].ViewRect.Max.X;
			const uint32 RightMinX = SceneRenderer->Views[1].ViewRect.Min.X;
			const uint32 RightMaxX = SceneRenderer->Views[1].ViewRect.Max.X;

			const uint32 LeftMaxY = SceneRenderer->Views[0].ViewRect.Max.Y;
			const uint32 RightMaxY = SceneRenderer->Views[1].ViewRect.Max.Y;

			RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0, 0.0f, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, 1.0f);
		}
		else
		{
			RHICmdList.SetViewport(0, 0, 0, SceneRenderer->InstancedStereoWidth, View.ViewRect.Max.Y, 1);
		}
	}
}

/**
 * Renders the scene's distortion
 */
void FSceneRenderer::RenderDistortion(FRHICommandListImmediate& RHICmdList)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion);
	SCOPED_DRAW_EVENT(RHICmdList, Distortion);
	SCOPED_GPU_STAT(RHICmdList, Distortion);

	// do we need to render the distortion pass?
	bool bRender=false;
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		if (View.bHasDistortionPrimitives && View.ShouldRenderView())
		{
			bRender=true;
			break;
		}
	}

	bool bDirty = false;

	TRefCountPtr<IPooledRenderTarget> DistortionRT;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	uint32 MSAACount = SceneContext.SceneDepthZ->GetDesc().NumSamples;

	// Use stencil mask to optimize cases with lower screen coverage.
	// Note: This adds an extra pass which is actually slower as distortion tends towards full-screen.
	//       It could be worth testing object screen bounds then reverting to a target flip and single pass.

	// Render accumulated distortion offsets
	if( bRender)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Render);
		SCOPED_DRAW_EVENT(RHICmdList, DistortionAccum);

		// Create a texture to store the resolved light attenuation values, and a render-targetable surface to hold the unresolved light attenuation values.
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
			Desc.Flags |= GFastVRamConfig.Distortion;
			Desc.NumSamples = MSAACount;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DistortionRT, TEXT("Distortion"));

			// use RGBA8 light target for accumulating distortion offsets
			// R = positive X offset
			// G = positive Y offset
			// B = negative X offset
			// A = negative Y offset
		}

		// DistortionRT==0 should never happen but better we don't crash
		if(DistortionRT)
		{
			FRHIRenderPassInfo RPInfo(DistortionRT->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Clear_Store);
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_DontStore, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilWrite;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderDistortion"));
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

					FViewInfo& View = Views[ViewIndex];
					if (!View.ShouldRenderView())
					{
						continue;
					}

					// viewport to match view size
					SetupDistortionPassView(RHICmdList, View, this);

					Scene->UniformBuffers.UpdateViewUniformBuffer(View);

					FDistortionPassUniformParameters DistortionPassParameters;
					SetupDistortionPassUniformBuffer(RHICmdList, View, DistortionPassParameters);
					Scene->UniformBuffers.DistortionPassUniformBuffer.UpdateUniformBufferImmediate(DistortionPassParameters);

					FMeshPassProcessorRenderState DrawRenderState(View, Scene->UniformBuffers.DistortionPassUniformBuffer);
					DrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);

					// test against depth and write stencil mask
					DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
						false, CF_DepthNearOrEqual,
						true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
						false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
						kStencilMaskBit, kStencilMaskBit>::GetRHI());
					DrawRenderState.SetStencilRef(kStencilMaskBit);

					// additive blending of offsets (or complexity if the shader complexity viewmode is enabled)
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());


					// Draw distortion meshes to accumulate their offsets.
					bDirty |= SubmitDistortionMeshDrawCommands(RHICmdList, View, DrawRenderState);
				}
			}
			RHICmdList.EndRenderPass();

			if (bDirty)
			{
				// Ideally we skip the EliminateFastClear since we don't need pixels with no stencil set to be cleared
				RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, DistortionRT->GetRenderTargetItem().TargetableTexture );
				// to be able to observe results with VisualizeTexture
				GVisualizeTexture.SetCheckPoint(RHICmdList, DistortionRT);
			}
		}
	}

	if (bDirty)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Post);
		SCOPED_DRAW_EVENT(RHICmdList, DistortionApply);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture);

		TRefCountPtr<IPooledRenderTarget> NewSceneColor;
		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, NewSceneColor, TEXT("DistortedSceneColor"));
		const FSceneRenderTargetItem& DestRenderTarget = NewSceneColor->GetRenderTargetItem();

		// Apply distortion and store off-screen
		{
			FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_DontStore, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilRead;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("DistortionApply"));
			{
				for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_PostView1);
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

					FViewInfo& View = Views[ViewIndex];

					if (MSAACount == 1)
					{
						DrawDistortionApplyScreenPass<false>(RHICmdList, SceneContext, View, *DistortionRT);
					}
					else
					{
						DrawDistortionApplyScreenPass<true>(RHICmdList, SceneContext, View, *DistortionRT);
					}
				}
			}
			RHICmdList.EndRenderPass();

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DestRenderTarget.TargetableTexture);
		}

		{
			FRHIRenderPassInfo RPInfo(SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_DontStore, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilWrite;

			TransitionRenderPassTargets(RHICmdList, RPInfo);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("DistortionMerge"));
			{
				for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_PostView2);
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

					FViewInfo& View = Views[ViewIndex];

					if (MSAACount == 1)
					{
						DrawDistortionMergePass<false>(RHICmdList, SceneContext, View, DestRenderTarget.TargetableTexture);
					}
					else
					{
						DrawDistortionMergePass<true>(RHICmdList, SceneContext, View, DestRenderTarget.TargetableTexture);
					}
				}
			}
			RHICmdList.EndRenderPass();
		}
	}
}

void FDistortionMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material.GetShadingModel();
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

		if (bIsTranslucent
			&& (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain())
			&& Material.IsDistorted())
		{
			Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, MaterialRenderProxy, Material, MeshFillMode, MeshCullMode);
		}
	}
}

void GetDistortionPassShaders(
	const FMaterial& Material,
	FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	FDistortionMeshHS*& HullShader,
	FDistortionMeshDS*& DomainShader,
	FDistortionMeshVS*& VertexShader,
	FDistortionMeshPS*& PixelShader)
{
	const EMaterialTessellationMode MaterialTessellationMode = Material.GetTessellationMode();

	const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[FeatureLevel])
		&& VertexFactoryType->SupportsTessellationShaders()
		&& MaterialTessellationMode != MTM_NoTessellation;

	if (bNeedsHSDS)
	{
		DomainShader = Material.GetShader<FDistortionMeshDS>(VertexFactoryType);
		HullShader = Material.GetShader<FDistortionMeshHS>(VertexFactoryType);
	}

	VertexShader = Material.GetShader<FDistortionMeshVS>(VertexFactoryType);
	PixelShader = Material.GetShader<FDistortionMeshPS>(VertexFactoryType);
}

void FDistortionMeshProcessor::Process(
	const FMeshBatch& MeshBatch, 
	uint64 BatchElementMask, 
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FDistortionMeshVS,
		FDistortionMeshHS,
		FDistortionMeshDS,
		FDistortionMeshPS> DistortionPassShaders;

	GetDistortionPassShaders(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		DistortionPassShaders.HullShader,
		DistortionPassShaders.DomainShader,
		DistortionPassShaders.VertexShader,
		DistortionPassShaders.PixelShader
		);

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(DistortionPassShaders.VertexShader, DistortionPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		DistortionPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

FDistortionMeshProcessor::FDistortionMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
{
}

FMeshPassProcessor* CreateDistortionPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState DistortionPassState;
	DistortionPassState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	DistortionPassState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	DistortionPassState.SetPassUniformBuffer(Scene->UniformBuffers.DistortionPassUniformBuffer);
	
	// test against depth and write stencil mask
	DistortionPassState.SetDepthStencilState(TStaticDepthStencilState<
		false, CF_DepthNearOrEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI());

	DistortionPassState.SetStencilRef(kStencilMaskBit);

	// additive blending of offsets (or complexity if the shader complexity viewmode is enabled)
	DistortionPassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

	return new(FMemStack::Get()) FDistortionMeshProcessor(Scene, InViewIfDynamicMeshCommand, DistortionPassState, InDrawListContext);
}

FMeshPassProcessor* CreateMobileDistortionPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState DistortionPassState;
	DistortionPassState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	DistortionPassState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	DistortionPassState.SetPassUniformBuffer(Scene->UniformBuffers.MobileDistortionPassUniformBuffer);

	// We don't have depth, render all pixels, pixel shader will sample SceneDepth from SceneColor.A and discard if occluded
	DistortionPassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	// additive blending of offsets
	DistortionPassState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

	return new(FMemStack::Get()) FDistortionMeshProcessor(Scene, InViewIfDynamicMeshCommand, DistortionPassState, InDrawListContext);
}

FRegisterPassProcessorCreateFunction RegisterDistortionPass(&CreateDistortionPassProcessor, EShadingPath::Deferred, EMeshPass::Distortion, EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileDistortionPass(&CreateMobileDistortionPassProcessor, EShadingPath::Mobile, EMeshPass::Distortion, EMeshPassFlags::MainView);