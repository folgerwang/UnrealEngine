// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeHDR.cpp: Post processing VisualizeHDR implementation.
=============================================================================*/

#include "PostProcess/PostProcessVisualizeHDR.h"
#include "EngineGlobals.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessHistogram.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/PostProcessTonemap.h"
#include "PipelineStateCache.h"
#include "Curves/CurveFloat.h"

/** Encapsulates the post processing eye adaptation pixel shader. */
class FPostProcessVisualizeHDRPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeHDRPS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"), 1);
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"), 1);
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"), 1);
		OutEnvironment.SetDefine(TEXT("USE_APPROXIMATE_SRGB"), (uint32)0);
		OutEnvironment.SetDefine(TEXT("EYE_ADAPTATION_PARAMS_SIZE"), (uint32)EYE_ADAPTATION_PARAMS_SIZE);
	}

	/** Default constructor. */
	FPostProcessVisualizeHDRPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter EyeAdaptationParams;
	FShaderResourceParameter MiniFontTexture;
	FShaderParameter InverseGamma;
	FShaderParameter HistogramParams;
	FShaderParameter ViewportCenterUV;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	FShaderResourceParameter EyeAdaptationTexture;

	/** Initialization constructor. */
	FPostProcessVisualizeHDRPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptationParams.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationParams"));
		MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));
		HistogramParams.Bind(Initializer.ParameterMap, TEXT("HistogramParams"));
		ViewportCenterUV.Bind(Initializer.ParameterMap, TEXT("ViewportCenterUV"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));

		EyeAdaptationTexture.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationTexture"));
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		{
			FVector4 Temp[EYE_ADAPTATION_PARAMS_SIZE];

			FRCPassPostProcessEyeAdaptation::ComputeEyeAdaptationParamsValue(Context.View, Temp);
			if (GetAutoExposureMethod(Context.View) == EAutoExposureMethod::AEM_Basic)
			{
				Temp[2].W = GetBasicAutoExposureFocus();
			}
			else
			{
				Temp[2].W = 0.0f;
			}

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, EyeAdaptationParams, Temp, EYE_ADAPTATION_PARAMS_SIZE);
		}

		SetTextureParameter(RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);

		// Load Current Eye Adaptation value.
		if (EyeAdaptationTexture.IsBound())
		{
			if (Context.View.HasValidEyeAdaptation())
			{
				IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation(Context.RHICmdList);
				SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptationTexture, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
			}
			else
			{
				SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptationTexture, GWhiteTexture->TextureRHI);
			}
		}

		{
			FIntPoint CenterPixelCoord = Context.SceneColorViewRect.Min + Context.SceneColorViewRect.Size() / 2;

			FVector2D Value;
			Value.X = (CenterPixelCoord.X + 0.5f) / float(Context.ReferenceBufferSize.X);
			Value.Y = (CenterPixelCoord.Y + 0.5f) / float(Context.ReferenceBufferSize.Y);

			SetShaderValue(RHICmdList, ShaderRHI, ViewportCenterUV, Value);
		}

		{
			FIntPoint GatherExtent = FRCPassPostProcessHistogram::ComputeGatherExtent(Context);

			uint32 TexelPerThreadGroupX = FRCPassPostProcessHistogram::ThreadGroupSizeX * FRCPassPostProcessHistogram::LoopCountX;
			uint32 TexelPerThreadGroupY = FRCPassPostProcessHistogram::ThreadGroupSizeY * FRCPassPostProcessHistogram::LoopCountY;

			FIntRect Value(GatherExtent, FIntPoint(TexelPerThreadGroupX, TexelPerThreadGroupY));

			SetShaderValue(RHICmdList, ShaderRHI, HistogramParams, Value);
		}

		{
			float InvDisplayGammaValue = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();

			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];
			FilmPostSetConstants(Constants, &Context.View.FinalPostProcessSettings,
				/* bMobile = */ false,
				/* UseColorMatrix = */ true,
				/* UseShadowTint = */ true,
				/* UseContrast = */ true);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptationParams << MiniFontTexture << InverseGamma << HistogramParams << ViewportCenterUV
			<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2 << ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 
			<< ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2 << EyeAdaptationTexture;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeHDRPS,TEXT("/Engine/Private/PostProcessVisualizeHDR.usf"),TEXT("MainPS"),SF_Pixel);

FString LogToString(float LogValue)
{
	if(LogValue > 0)
	{
		return FString::Printf(TEXT("%.0f"), FMath::Pow(2.0f, LogValue));
	}
	else
	{
		return FString::Printf(TEXT("1/%.0f"), FMath::Pow(2.0f, -LogValue));
	}
}

void FRCPassPostProcessVisualizeHDR::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessVisualizeHDR);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	static const auto VarDefaultAutoExposureExtendDefaultLuminanceRange = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	const bool bExtendedLuminanceRange = VarDefaultAutoExposureExtendDefaultLuminanceRange->GetValueOnRenderThread() == 1;

	const FViewInfo& View = Context.View;
	const FViewInfo& ViewInfo = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	const EAutoExposureMethod AutoExposureMethod = GetAutoExposureMethod(ViewInfo);

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	FIntRect SrcRect = Context.SceneColorViewRect;
	FIntRect DestRect = Context.GetSceneColorDestRect(DestRenderTarget);
	FIntPoint SrcSize = InputDesc->Extent;

	// Set the view family's render target/viewport.
	FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Load_Store);
	Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("VisualizeHDR"));
	{
		Context.SetViewportAndCallRHI(DestRect);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessVisualizeHDRPS> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetPS(Context.RHICmdList, Context);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			DestRect.Width(), DestRect.Height(),
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DestRect.Size(),
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	Context.RHICmdList.EndRenderPass();

	FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
	FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

	float X = DestRect.Min.X + 30;
	float Y = DestRect.Min.Y + 28;
	const float YStep = 14;
	const float ColumnWidth = 250;

	FString Line;

	Line = FString::Printf(TEXT("HDR Histogram (EV100, max of RGB)"));
	Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

	Y += 160;

	float MinX = DestRect.Min.X + 64 + 10;
	float MaxY = DestRect.Max.Y - 64;
	float SizeX = DestRect.Size().X - 64 * 2 - 20;

	for (uint32 i = 0; i <= 4; ++i)
	{
		int XAdd = (int)(i * SizeX / 4);
		float HistogramPosition = i / 4.0f;
		float EV100Value = FMath::Lerp(View.FinalPostProcessSettings.HistogramLogMin, View.FinalPostProcessSettings.HistogramLogMax, HistogramPosition);
		if (!bExtendedLuminanceRange)
		{
			// In this case the post process settings are actually Log2 values.
			EV100Value = Log2ToEV100(EV100Value);
		}

		Line = FString::Printf(TEXT("%.2g"), EV100Value);
		Canvas.DrawShadowedString(MinX + XAdd - 5, MaxY + YStep, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));
	}
	Y += 3 * YStep;
	if (AutoExposureMethod == EAutoExposureMethod::AEM_Basic)
	{
		Line = FString::Printf(TEXT("Basic"));
	}
	else
	{
		Line = FString::Printf(TEXT("Histogram"));
	}
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("Auto Exposure Method:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

	Line = FString::Printf(TEXT("%g%% .. %g%%"), View.FinalPostProcessSettings.AutoExposureLowPercent, View.FinalPostProcessSettings.AutoExposureHighPercent);
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("Percent Low/High:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

	if (bExtendedLuminanceRange)
	{
		Line = FString::Printf(TEXT("%.1f .. %.1f"), View.FinalPostProcessSettings.AutoExposureMinBrightness, View.FinalPostProcessSettings.AutoExposureMaxBrightness);
	}
	else
	{
		Line = FString::Printf(TEXT("%.1f .. %.1f"), LuminanceToEV100(View.FinalPostProcessSettings.AutoExposureMinBrightness), LuminanceToEV100(View.FinalPostProcessSettings.AutoExposureMaxBrightness));
	}
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("EV100 Min/Max"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(0.3f, 0.3f, 1));

	Line = FString::Printf(TEXT("%g / %g"), View.FinalPostProcessSettings.AutoExposureSpeedUp, View.FinalPostProcessSettings.AutoExposureSpeedDown);
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("Speed Up/Down:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

	float AutoExposureBias = View.FinalPostProcessSettings.AutoExposureBias;
	if (View.FinalPostProcessSettings.AutoExposureBiasCurve)
	{
		float AverageSceneLuminance = View.GetLastAverageSceneLuminance();
		if (AverageSceneLuminance > 0)
		{
			AutoExposureBias += View.FinalPostProcessSettings.AutoExposureBiasCurve->GetFloatValue(LuminanceToEV100(AverageSceneLuminance));
		}
	}

	Line = FString::Printf(TEXT("%.2g"), AutoExposureBias);
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("Exposure Compensation: "), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));

	if (AutoExposureMethod == AEM_Basic || AutoExposureMethod == AEM_Histogram)
	{
		Line = FString::Printf(TEXT("%g%%"), AutoExposureMethod == AEM_Basic ? View.FinalPostProcessSettings.AutoExposureCalibrationConstant : 100.f);
		Canvas.DrawShadowedString(X, Y += YStep, TEXT("Calibration Constant: "), GetStatsFont(), FLinearColor(1, 1, 1));
		Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));
	}

	if (bExtendedLuminanceRange)
	{
		Line = FString::Printf(TEXT("%.1f .. %.1f"), View.FinalPostProcessSettings.HistogramLogMin, View.FinalPostProcessSettings.HistogramLogMax);
	}
	else
	{
		Line = FString::Printf(TEXT("%.1f .. %.1f"), Log2ToEV100(View.FinalPostProcessSettings.HistogramLogMin), Log2ToEV100(View.FinalPostProcessSettings.HistogramLogMax));
	}

	Canvas.DrawShadowedString(X, Y += YStep, TEXT("Histogram EV100 Min/Max:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(0.3f, 0.3f, 1));

	if (AutoExposureMethod == EAutoExposureMethod::AEM_Basic)
	{
		Line = FString::Printf(TEXT("%g"), GetBasicAutoExposureFocus());
		Canvas.DrawShadowedString(X, Y += YStep, TEXT("Weighting Focus: "), GetStatsFont(), FLinearColor(1, 1, 1));
		Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));
	}

	Canvas.Flush_RenderThread(Context.RHICmdList);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeHDR::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeHDR");

	return Ret;
}
