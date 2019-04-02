// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MobileDistortionPass.cpp - Mobile specific rendering of primtives with refraction
=============================================================================*/

#include "MobileDistortionPass.h"
#include "TranslucentRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"

bool IsMobileDistortionActive(const FViewInfo& View)
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DisableDistortion"));
	int32 DisableDistortion = CVar->GetInt();

	// Distortion on mobile requires SceneDepth information in SceneColor.A channel
	const EMobileHDRMode HDRMode = GetMobileHDRMode();
	const bool bVisiblePrims = View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].HasAnyDraw();

	return
		HDRMode == EMobileHDRMode::EnabledFloat16 &&
		View.Family->EngineShowFlags.Translucency &&
		bVisiblePrims &&
		FSceneRenderer::GetRefractionQuality(*View.Family) > 0 &&
		DisableDistortion == 0;
}

void FRCDistortionAccumulatePassES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DistortionAccumulatePass);

	FViewInfo& View = const_cast<FViewInfo &>(Context.View);
	FSceneRenderTargets& SceneTargets = FSceneRenderTargets::Get(Context.RHICmdList);
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Clear_Store);
	Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("DistortionAccumulate"));
	{

		Context.SetViewportAndCallRHI(View.ViewRect);

		if (Scene->UniformBuffers.UpdateViewUniformBuffer(View))
		{
			FMobileDistortionPassUniformParameters Parameters;
			SetupMobileDistortionPassUniformBuffer(Context.RHICmdList, View, Parameters);
			Scene->UniformBuffers.MobileDistortionPassUniformBuffer.UpdateUniformBufferImmediate(Parameters);
		}

		View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].DispatchDraw(nullptr, Context.RHICmdList);
	}
	Context.RHICmdList.EndRenderPass();
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, FResolveParams());
}

FPooledRenderTargetDesc FRCDistortionAccumulatePassES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("DistortionAccumulatePass");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Transparent);
	return Ret;
}

class FDistortionMergePS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDistortionMergePS_ES2, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !IsConsolePlatform(Parameters.Platform);
	}

	FDistortionMergePS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FDistortionMergePS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FDistortionMergePS_ES2, TEXT("/Engine/Private/DistortApplyScreenPS.usf"), TEXT("Merge_ES2"), SF_Pixel);

void FRCDistortionMergePassES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DistortionMergePass);

	const FViewInfo& View = Context.View;
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc& OutputDesc = PassOutputs[0].RenderTargetDesc;
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Clear_Store);
	Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("DistortionMerge"));
	{
		FIntRect SrcRect = View.ViewRect;
		FIntRect DestRect = View.ViewRect;
		FIntPoint SrcSize = InputDesc->Extent;
		FIntPoint DstSize = OutputDesc.Extent;

		Context.SetViewportAndCallRHI(View.ViewRect);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FDistortionMergePS_ES2> PixelShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context);

		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			DstSize.X, DstSize.Y,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	Context.RHICmdList.EndRenderPass();
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, FResolveParams());
}

FPooledRenderTargetDesc FRCDistortionMergePassES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("DistortionMergePass");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}
