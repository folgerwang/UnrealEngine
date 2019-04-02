// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.cpp: Post process MotionBlur implementation.
=============================================================================*/

#include "PostProcess/PostProcessMitchellNetravali.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "PostProcess/PostProcessTonemap.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "PostProcessing.h"




class FMitchellNetravaliDownsamplCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMitchellNetravaliDownsamplCS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Parameters.Platform);
	}

	FMitchellNetravaliDownsamplCS() {}
	FMitchellNetravaliDownsamplCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		Output.Bind(Initializer.ParameterMap, TEXT("Output0"));
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		DispatchThreadToInputBufferUV.Bind(Initializer.ParameterMap, TEXT("DispatchThreadToInputBufferUV"));
		DownscaleFactor.Bind(Initializer.ParameterMap, TEXT("DownscaleFactor"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << Output << EyeAdaptation << DispatchThreadToInputBufferUV << DownscaleFactor;
		return bShaderHasOutdatedParameters;
	}

	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter Output;
	FShaderResourceParameter EyeAdaptation;
	
	FShaderParameter DispatchThreadToInputBufferUV;
	FShaderParameter DownscaleFactor;
};

IMPLEMENT_GLOBAL_SHADER(FMitchellNetravaliDownsamplCS, "/Engine/Private/PostProcessMitchellNetravali.usf", "DownsampleMainCS", SF_Compute);


void FRCPassMitchellNetravaliDownsample::Process(FRenderingCompositePassContext& Context)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Src rectangle.
	FIntRect SrcRect = Params.InputViewRect;
	FIntRect DestRect = Params.OutputViewRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, MitchellNetravaliDownsample, TEXT("MitchellNetravaliDownsample %dx%d -> %dx%d"),
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Width(), DestRect.Height());

	// Common setup
	UnbindRenderTargets(Context.RHICmdList);
	Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);

	FTextureRHIRef EyeAdaptationTex = GWhiteTexture->TextureRHI;
	if (Context.View.HasValidEyeAdaptation())
	{
		EyeAdaptationTex = Context.View.GetEyeAdaptation(Context.RHICmdList)->GetRenderTargetItem().TargetableTexture;
	}

	{
		Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);

		auto ShaderMap = Context.GetShaderMap();
		TShaderMapRef<FMitchellNetravaliDownsamplCS> Shader(ShaderMap);
		const FComputeShaderRHIParamRef ShaderRHI = Shader->GetComputeShader();

		Context.RHICmdList.SetComputeShader(ShaderRHI);

		// Parameters plumbing.
		{
			Shader->PostprocessParameter.SetCS(ShaderRHI, Context, Context.RHICmdList);

			Context.RHICmdList.SetUAVParameter(ShaderRHI, Shader->Output.GetBaseIndex(), DestRenderTarget.MipUAVs[0]);

			// For global samplers.
			Shader->SetParameters<FViewUniformShaderParameters>(
				Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

			FVector4 DispatchThreadToInputBufferUV;
			DispatchThreadToInputBufferUV.X = SrcRect.Width() / float(DestRect.Width() * SrcSize.X);
			DispatchThreadToInputBufferUV.Y = SrcRect.Height() / float(DestRect.Height() * SrcSize.Y);
			DispatchThreadToInputBufferUV.Z = DispatchThreadToInputBufferUV.X * (0.5f + SrcRect.Min.X);
			DispatchThreadToInputBufferUV.W = DispatchThreadToInputBufferUV.Y * (0.5f + SrcRect.Min.Y);

			SetShaderValue(Context.RHICmdList, ShaderRHI, Shader->DispatchThreadToInputBufferUV, DispatchThreadToInputBufferUV);

			FVector2D DownscaleFactor(DestRect.Width() / float(SrcRect.Width()), SrcRect.Width() / float(DestRect.Width()));
			SetShaderValue(Context.RHICmdList, ShaderRHI, Shader->DownscaleFactor, DownscaleFactor);
		}

		FIntPoint DestSize(DestRect.Width(), DestRect.Height());
		uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, 8);
		uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, 8);
		DispatchComputeShader(Context.RHICmdList, *Shader, GroupSizeX, GroupSizeY, 1);

		// Unset.
		{
			Context.RHICmdList.SetUAVParameter(ShaderRHI, Shader->Output.GetBaseIndex(), nullptr);
		}

		Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV);
	}

	Context.SceneColorViewRect = DestRect;
	Context.ReferenceBufferSize = Params.OutputExtent;
}

FPooledRenderTargetDesc FRCPassMitchellNetravaliDownsample::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Ret.Reset();
	Ret.Format = PF_FloatRGBA;
	Ret.DebugName = TEXT("MitchellNetravaliDownsample");
	Ret.AutoWritable = false;
	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;

	Ret.Extent = Params.OutputExtent;

	return Ret;
}
