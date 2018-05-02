// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DiaphragmDOFGraph.cpp: Wire diaphragm DOF's passes together to convolve
	scene color.
=============================================================================*/

#include "PostProcess/DiaphragmDOF.h"
#include "PostProcess/DiaphragmDOFPasses.h"
#include "PostProcess/PostProcessTemporalAA.h"


namespace
{

enum class EGatheringGraphLayout
{
	SeparateUniqueHalf,
	SeparateHalfEighth,
};


TAutoConsoleVariable<int32> CVarAccumulatorQuality(
	TEXT("r.DOF.Gather.AccumulatorQuality"),
	1,
	TEXT("Controles the quality of the gathering accumulator.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarEnableGatherBokehSettings(
	TEXT("r.DOF.Gather.EnableBokehSettings"),
	1,
	TEXT("Whether to applies bokeh settings on foreground and background gathering.\n")
	TEXT(" 0: Disable;\n 1: Enable (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarPostFilteringMethod(
	TEXT("r.DOF.Gather.PostfilterMethod"),
	1,
	TEXT("Method to use to post filter a gather pass.\n")
	TEXT(" 0: None;\n")
	TEXT(" 1: Per RGB channel median 3x3 (default);\n")
	TEXT(" 2: Per RGB channel max 3x3."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarRingCount(
	TEXT("r.DOF.Gather.RingCount"),
	5,
	TEXT("Number of rings for gathering kernels [[3; 5]]. Default to 5.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);


TAutoConsoleVariable<int32> CVarLowerGatheringPass(
	TEXT("r.DOF.LowerGatheringPass"),
	0,
	TEXT("Wether large coc radius should be accumulated with lower resolution gathering pass.\n")
	TEXT(" 0: No (default);")
	TEXT(" 1: Yes.\n"),
	ECVF_RenderThreadSafe);


TAutoConsoleVariable<int32> CVarHybridScatterForegroundMode(
	TEXT("r.DOF.Scatter.ForegroundCompositing"),
	1,
	TEXT("Compositing mode of the foreground hybrid scattering.\n")
	TEXT(" 0: Disabled;\n")
	TEXT(" 1: Additive (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarHybridScatterBackgroundMode(
	TEXT("r.DOF.Scatter.BackgroundCompositing"),
	2,
	TEXT("Compositing mode of the background hybrid scattering.\n")
	TEXT(" 0: Disabled;\n")
	TEXT(" 1: Additive;\n")
	TEXT(" 2: Gather occlusion (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarEnableScatterBokehSettings(
	TEXT("r.DOF.Scatter.EnableBokehSettings"),
	1,
	TEXT("Whether to enable bokeh settings on scattering.\n")
	TEXT(" 0: Disable;\n 1: Enable (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarScatterMinCocRadius(
	TEXT("r.DOF.Scatter.MinCocRadius"),
	3.0f,
	TEXT("Minimal Coc radius required to be scattered (default = 3)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarScatterMaxSpriteRatio(
	TEXT("r.DOF.Scatter.MaxSpriteRatio"),
	0.1f,
	TEXT("Maximum ratio of scattered pixel quad as sprite, usefull to control DOF's scattering upperbound. ")
	TEXT(" 1 will allow to scatter 100% pixel quads, whereas 0.2 will only allow 20% (default = 0.1)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarEnableRecombineBokehSettings(
	TEXT("r.DOF.Recombine.EnableBokehSettings"),
	1,
	TEXT("Whether to applies bokeh settings on slight out of focus done in recombine pass.\n")
	TEXT(" 0: Disable;\n 1: Enable (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarRecombineQuality(
	TEXT("r.DOF.Recombine.Quality"),
	2,
	TEXT("Configures the quality of the recombine pass.\n")
	TEXT(" 0: No slight out of focus;\n")
	TEXT(" 1: Slight out of focus 24spp;\n")
	TEXT(" 2: Slight out of focus 32spp (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarMinimalFullresBlurRadius(
	TEXT("r.DOF.Recombine.MinFullresBlurRadius"),
	0.1f,
	TEXT("Minimal blurring radius used in full resolution pixel width to actually do ")
	TEXT("DOF  when slight out of focus is enabled (default = 0.1)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarDOFTemporalAAQuality(
	TEXT("r.DOF.TemporalAAQuality"),
	1,
	TEXT("Quality of temporal AA pass done in DOF.\n")
	TEXT(" 0: Faster but lower quality;")
	TEXT(" 1: Higher quality pass (default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);


EDiaphragmDOFPostfilterMethod GetPostfilteringMethod()
{
	int32 i = CVarPostFilteringMethod.GetValueOnRenderThread();
	if (i >= 0 && i < int32(EDiaphragmDOFPostfilterMethod::MAX))
	{
		return EDiaphragmDOFPostfilterMethod(i);
	}
	return EDiaphragmDOFPostfilterMethod::None;
}


enum class EHybridScatterMode
{
	Disabled,
	Additive,
	Occlusion,
};


}


bool DiaphragmDOF::WireSceneColorPasses(FPostprocessContext& Context, const FRenderingCompositeOutputRef& VelocityInput, const FRenderingCompositeOutputRef& SeparateTranslucency)
{
	if (Context.View.Family->EngineShowFlags.VisualizeDOF)
	{
		// no need for this pass
		return false;
	}

	const EShaderPlatform ShaderPlatform = Context.View.GetShaderPlatform();

	// Number of sampling ring in the gathering kernel.
	const int32 HalfResRingCount = FMath::Clamp(CVarRingCount.GetValueOnRenderThread(), FRCPassDiaphragmDOFGather::kMinRingCount, FRCPassDiaphragmDOFGather::MaxRingCount(ShaderPlatform));

	// Post filtering method to do.
	const EDiaphragmDOFPostfilterMethod PostfilterMethod = GetPostfilteringMethod();

	// The mode for hybrid scattering.
	const EHybridScatterMode FgdHybridScatteringMode = EHybridScatterMode(CVarHybridScatterForegroundMode.GetValueOnRenderThread());
	const EHybridScatterMode BgdHybridScatteringMode = EHybridScatterMode(CVarHybridScatterBackgroundMode.GetValueOnRenderThread());
	
	const float MinScatteringCocRadius = FMath::Max(CVarScatterMinCocRadius.GetValueOnRenderThread(), FRCPassDiaphragmDOFHybridScatter::kMinCocRadius);

	// Whether the platform support gather bokeh simmulation.
	const bool bSupportGatheringBokehSimulation = FRCPassDiaphragmDOFGather::SupportsBokehSimmulation(ShaderPlatform);

	// Whether should use shade permutation that does lower quality accumulation.
	const bool bUseLowAccumulatorQuality = CVarAccumulatorQuality.GetValueOnRenderThread() == 0;

	// Setting for scattering budget upper bound.
	const float MaxScatteringRatio = FMath::Clamp(CVarScatterMaxSpriteRatio.GetValueOnRenderThread(), 0.0f, 1.0f);

	// Quality setting for the recombine pass.
	const int32 RecombineQuality = FMath::Clamp(CVarRecombineQuality.GetValueOnRenderThread(), 0, FRCPassDiaphragmDOFRecombine::kMaxQuality);

	// Resolution divisor.
	// TODO: Exposes lower resolution divisor?
	const int32 PrefilteringResolutionDivisor = 2;

	// Minimal absolute Coc radius to spawn a gather pass. Blurring radius under this are considered not great looking.
	// This is assuming the pass is opacity blending with a ramp from 1 to 2. This can not be exposed as a cvar,
	// because the slight out focus's lower res pass uses for full res convolution stability depends on this.
	const float kMinimalAbsGatherPassCocRadius = 1;

	// Minimal CocRadius to wire lower res gathering passes.
	const float BackgroundCocRadiusMaximumForUniquePass = HalfResRingCount * 4.0; // TODO: polish that.

	// Whether the recombine pass does slight out of focus convolution.
	const bool bRecombineDoesSlightOutOfFocus = RecombineQuality > 0;

	// Whether the recombine pass wants separate input buffer for foreground hole filling.
	const bool bRecombineDoesSeparateForegroundHoleFilling = RecombineQuality > 0;

	// Compute the required blurring radius to actually perform depth of field, that depends on whether
	// doing slight out of focus convolution.
	const float MinRequiredBlurringRadius = bRecombineDoesSlightOutOfFocus
		? (CVarMinimalFullresBlurRadius.GetValueOnRenderThread() * 0.5f)
		: kMinimalAbsGatherPassCocRadius;


	// Derives everything needed from the view.
	FSceneViewState* const ViewState = Context.View.ViewState;

	FPhysicalCocModel CocModel;
	CocModel.Compile(Context.View);

	FBokehModel BokehModel;
	BokehModel.Compile(Context.View);

	// Prepare preprocessing TAA pass.
	FTAAPassParameters TAAParameters(Context.View);
	{
		TAAParameters.Pass = ETAAPassConfig::DiaphragmDOF;

		// When using dynamic resolution, the blur introduce by TAA's history resolution changes is quite noticeable on DOF.
		// Therefore we switch for a temporal upsampling technic to maintain the same history resolution.
		if (Context.View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
		{
			TAAParameters.Pass = ETAAPassConfig::DiaphragmDOFUpsampling;
		}

		TAAParameters.SetupViewRect(Context.View, PrefilteringResolutionDivisor);
		TAAParameters.TopLeftCornerViewRects();

		TAAParameters.bUseFast = CVarDOFTemporalAAQuality.GetValueOnRenderThread() == 0;
	}

	// Size of the view in GatherColorSetup.
	FIntPoint PreprocessViewSize = FIntPoint::DivideAndRoundUp(Context.View.ViewRect.Size(), PrefilteringResolutionDivisor);
	FIntPoint GatheringViewSize = PreprocessViewSize;

	if (Context.View.AntiAliasingMethod == AAM_TemporalAA && ViewState)
	{
		PreprocessViewSize = FIntPoint::DivideAndRoundUp(TAAParameters.OutputViewRect.Size(), PrefilteringResolutionDivisor);
	}

	const float PreProcessingToProcessingCocRadiusFactor = float(GatheringViewSize.X) / float(PreprocessViewSize.X);

	const float MaxBackgroundCocRadius = CocModel.ComputeViewMaxBackgroundCocRadius(GatheringViewSize.X);
	const float MinForegroundCocRadius = CocModel.ComputeViewMinForegroundCocRadius(GatheringViewSize.X);
	const float AbsMaxForegroundCocRadius = FMath::Abs(MinForegroundCocRadius);
	const float MaxBluringRadius = FMath::Max(MaxBackgroundCocRadius, AbsMaxForegroundCocRadius);

	// Whether should hybrid scatter for foreground and background.
	bool bForegroundHybridScattering = FgdHybridScatteringMode != EHybridScatterMode::Disabled && AbsMaxForegroundCocRadius > MinScatteringCocRadius && MaxScatteringRatio > 0.0f;
	bool bBackgroundHybridScattering = BgdHybridScatteringMode != EHybridScatterMode::Disabled && MaxBackgroundCocRadius > MinScatteringCocRadius && MaxScatteringRatio > 0.0f;

	// Compute the reference buffer size for PrefilteringResolutionDivisor.
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
	FIntPoint RefBufferSize = FIntPoint::DivideAndRoundUp(SceneContext.GetBufferSizeXY(), PrefilteringResolutionDivisor);

	// If the max blurring radius is too small, do not wire any passes.
	if (MaxBluringRadius < MinRequiredBlurringRadius)
	{
		return false;
	}

	bool bGatherForeground = AbsMaxForegroundCocRadius > kMinimalAbsGatherPassCocRadius;

	// Controles what DOF pass layout should be used.
	EGatheringGraphLayout GatheringLayout = (CVarLowerGatheringPass.GetValueOnRenderThread() != 0
		? EGatheringGraphLayout::SeparateHalfEighth
		: EGatheringGraphLayout::SeparateUniqueHalf);
	if (GatheringLayout == EGatheringGraphLayout::SeparateHalfEighth && MaxBackgroundCocRadius < BackgroundCocRadiusMaximumForUniquePass)
	{
		GatheringLayout = EGatheringGraphLayout::SeparateUniqueHalf;
	}

	FRenderingCompositeOutputRef FullresColorSetup = Context.FinalOutput;
	FRenderingCompositeOutputRef GatherColorSetup;
	TDrawEvent<FRHICommandList>* MainDrawEvent;

	// Setup at lower resolution from full resolution scene color and scene depth.
	{
		FRCPassDiaphragmDOFSetup::FParameters Params;
		Params.CocModel = CocModel;
		Params.bOutputFullResolution = bRecombineDoesSlightOutOfFocus;
		Params.bOutputHalfResolution = true;
		Params.FullResCocRadiusBasis = GatheringViewSize.X;
		Params.HalfResCocRadiusBasis = PreprocessViewSize.X;

		FRCPassDiaphragmDOFSetup* DOFSetup = new(FMemStack::Get()) FRCPassDiaphragmDOFSetup(Params);
		Context.Graph.RegisterPass(DOFSetup);
		DOFSetup->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
		DOFSetup->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));

		if (bRecombineDoesSlightOutOfFocus)
		{
			FullresColorSetup = FRenderingCompositeOutputRef(DOFSetup, ePId_Output0);
		}

		GatherColorSetup = FRenderingCompositeOutputRef(DOFSetup, ePId_Output1);
		MainDrawEvent = &DOFSetup->MainDrawEvent;
	}

	// TAA the setup for the convolution to be temporally stable.);
	if (Context.View.AntiAliasingMethod == AAM_TemporalAA && ViewState)
	{
		// TODO: need to TAA CocSetup with MRT support of TAA.
		FRenderingCompositePass* NodeTemporalAA = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTemporalAA(
			Context, TAAParameters,
			Context.View.PrevViewInfo.DOFPreGatherHistory,
			&ViewState->PendingPrevFrameViewInfo.DOFPreGatherHistory));
		NodeTemporalAA->SetInput(ePId_Input0, GatherColorSetup);
		NodeTemporalAA->SetInput(ePId_Input1, VelocityInput);

		GatherColorSetup = FRenderingCompositeOutputRef(NodeTemporalAA);
	}

	// Generate conservative coc tiles.
	FRenderingCompositeOutputRef CocTileOutput;
	{
		// Flatten half res CoC to lower res tiles.
		FRCPassDiaphragmDOFFlattenCoc::FParameters FlattenParams;
		FlattenParams.InputViewSize = PreprocessViewSize;
		FlattenParams.GatherViewSize = GatheringViewSize;

		FRenderingCompositePass* CocFlatten = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFFlattenCoc(FlattenParams));
		CocFlatten->SetInput(ePId_Input0, FRenderingCompositeOutputRef(GatherColorSetup));
		CocTileOutput = CocFlatten;

		// Parameters for the dilate Coc passes.
		FRCPassDiaphragmDOFDilateCoc::FParameters DilateParams[2];
		{
			// Compute the maximum tile dilation.
			int32 MaximumTileDilation = FMath::CeilToInt(
				MaxBluringRadius / FRCPassDiaphragmDOFFlattenCoc::CocTileResolutionDivisor);

			// There is always at least one dilate pass so that even small Coc radius conservatively dilate on next neighboor.
			DilateParams[0].SampleRadiusCount = FMath::Min(
				MaximumTileDilation, FRCPassDiaphragmDOFDilateCoc::MaxSampleRadiusCount);
			
			// If the theoric radius is too big, setup second dilate pass.
			if (MaximumTileDilation - DilateParams[0].SampleRadiusCount > FRCPassDiaphragmDOFDilateCoc::MaxSampleRadiusCount)
			{
				DilateParams[1].SampleDistanceMultiplier = DilateParams[0].SampleRadiusCount + 1;
				DilateParams[1].SampleRadiusCount = FMath::Min(
					FMath::DivideAndRoundUp(
						MaximumTileDilation - DilateParams[0].SampleRadiusCount,
						DilateParams[1].SampleDistanceMultiplier),
					FRCPassDiaphragmDOFDilateCoc::MaxSampleRadiusCount);
			}
		}

		// Creates the dilates passes.
		for (int32 i = 0; i < ARRAY_COUNT(DilateParams); i++)
		{
			if (!DilateParams[i].SampleRadiusCount && i != 0) break;

			DilateParams[i].GatherViewSize = GatheringViewSize;
			DilateParams[i].PreProcessingToProcessingCocRadiusFactor = PreProcessingToProcessingCocRadiusFactor;

			FRenderingCompositePass* CocDilate = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFDilateCoc(DilateParams[i]));
			CocDilate->SetInput(ePId_Input0, CocTileOutput);
			CocTileOutput = CocDilate;
		}
	}

	// Reduce the gathering input to scale with very large convolutions.
	FRenderingCompositeOutputRef PrefilterOutput;
	{
		FRCPassDiaphragmDOFReduce::FParameters ReduceParams;
		ReduceParams.InputResolutionDivisor = PrefilteringResolutionDivisor;
		ReduceParams.bExtractForegroundHybridScattering = bForegroundHybridScattering;
		ReduceParams.bExtractBackgroundHybridScattering = bBackgroundHybridScattering;
		ReduceParams.InputViewSize = PreprocessViewSize;
		ReduceParams.PreProcessingToProcessingCocRadiusFactor = PreProcessingToProcessingCocRadiusFactor;
		ReduceParams.MinScatteringCocRadius = MinScatteringCocRadius;
		ReduceParams.MaxScatteringRatio = MaxScatteringRatio;

		{
			int32 MipLevelCount = FMath::CeilToInt(FMath::Log2(MaxBluringRadius * 0.5 / HalfResRingCount));

			// Lower accumulator quality uses KERNEL_DENSITY_HEXAWEB_LOWER_IN_CENTER that sample in 
			// one mip level higher.
			if (bUseLowAccumulatorQuality)
			{
				MipLevelCount += 1;
			}

			ReduceParams.MipLevelCount = FMath::Clamp(MipLevelCount, 2, FRCPassDiaphragmDOFReduce::kMaxMipLevelCount);
		}

		// Downsample the gather color setup to have faster neighborhood comparisons.
		FRenderingCompositeOutputRef HybridScatterExtractDownsample;
		if (bForegroundHybridScattering || bBackgroundHybridScattering)
		{
			FRCPassDiaphragmDOFDownsample::FParameters DownsampleParameters;
			DownsampleParameters.InputViewSize = PreprocessViewSize;

			// Reduce pass convert the CocRadius basis at the very begining, and to avoid to do it for every comparing sample
			// in reduce pass as well, we do it on the downsampling pass.
			DownsampleParameters.OutputCocRadiusMultiplier = PreProcessingToProcessingCocRadiusFactor;

			FRenderingCompositePass* GatherColorDownsample = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFDownsample(DownsampleParameters));
			GatherColorDownsample->SetInput(ePId_Input0, GatherColorSetup);
			HybridScatterExtractDownsample = GatherColorDownsample;
		}

		FRenderingCompositePass* Prefilter = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFReduce(ReduceParams));
		Prefilter->SetInput(ePId_Input0, GatherColorSetup);
		Prefilter->SetInput(ePId_Input1, HybridScatterExtractDownsample);
		PrefilterOutput = Prefilter;
	}


	FRenderingCompositeOutputRef ScatteringBokehLUTOutput;
	FRenderingCompositeOutputRef GatheringBokehLUTOutput;
	EDiaphragmDOFBokehSimulation BokehSimulation = EDiaphragmDOFBokehSimulation::Disabled;
	if (BokehModel.BokehShape != EBokehShape::Circle)
	{
		ScatteringBokehLUTOutput = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFBuildBokehLUT(
			BokehModel, FRCPassDiaphragmDOFBuildBokehLUT::EFormat::CocRadiusToBokehEdgeFactor));

		GatheringBokehLUTOutput = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFBuildBokehLUT(
			BokehModel, FRCPassDiaphragmDOFBuildBokehLUT::EFormat::GatherSamplePos));

		BokehSimulation = (BokehModel.DiaphragmBladeCount % 2)
			? EDiaphragmDOFBokehSimulation::GenericBokeh
			: EDiaphragmDOFBokehSimulation::SimmetricBokeh;
	}


	FRenderingCompositeOutputRef ForegroundConvolutionOutput;
	FRenderingCompositeOutputRef ForegroundHoleFillingOutput;
	FRenderingCompositeOutputRef BackgroundConvolutionOutput;
	FRenderingCompositeOutputRef SlightOutOfFocusConvolutionOutput;

	// Generates Foreground, foreground hole filling and background gather passes.
	{
		auto BuildGatherPass = [&](FRCPassDiaphragmDOFGather::FParameters& GatherParameters, int32 ResolutionDivisor)
		{
			GatherParameters.RingCount = HalfResRingCount;
			GatherParameters.InputViewSize = PreprocessViewSize;
			GatherParameters.OutputViewSize = FIntPoint::DivideAndRoundUp(GatheringViewSize, ResolutionDivisor);
			GatherParameters.OutputBufferSize = FIntPoint::DivideAndRoundUp(RefBufferSize, ResolutionDivisor);

			FRenderingCompositePass* GatherPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFGather(GatherParameters));
			GatherPass->SetInput(ePId_Input0, PrefilterOutput);
			GatherPass->SetInput(ePId_Input1, CocTileOutput);

			if (GatherParameters.BokehSimulation != EDiaphragmDOFBokehSimulation::Disabled)
				GatherPass->SetInput(ePId_Input3, GatheringBokehLUTOutput);

			return GatherPass;
		};

		auto BuildPostfilterPass = [&](const FRCPassDiaphragmDOFGather::FParameters& GatherParameters, FRenderingCompositeOutputRef Input)
		{
			if (GatherParameters.PostfilterMethod != EDiaphragmDOFPostfilterMethod::None)
			{
				FRenderingCompositePass* Postfilter = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFPostfilter(GatherParameters));
				Postfilter->SetInput(ePId_Input0, Input);
				Postfilter->SetInput(ePId_Input2, CocTileOutput);
				Input = FRenderingCompositeOutputRef(Postfilter, ePId_Output0);
			}
			return Input;
		};

		const bool bEnableGatherBokehSettings = (
			bSupportGatheringBokehSimulation &&
			CVarEnableGatherBokehSettings.GetValueOnRenderThread() == 1);
		const bool bEnableScatterBokehSettings = CVarEnableScatterBokehSettings.GetValueOnRenderThread() == 1;

		// Wire foreground gathering passes.
		if (bGatherForeground)
		{
			FRCPassDiaphragmDOFGather::FParameters GatherParameters;
			GatherParameters.LayerProcessing = EDiaphragmDOFLayerProcessing::ForegroundOnly;
			GatherParameters.PostfilterMethod = PostfilterMethod;

			if (bEnableGatherBokehSettings)
				GatherParameters.BokehSimulation = BokehSimulation;

			if (bUseLowAccumulatorQuality)
			{
				GatherParameters.QualityConfig = FRCPassDiaphragmDOFGather::EQualityConfig::LowQualityAccumulator;
			}

			FRenderingCompositePass* GatherPass = BuildGatherPass(GatherParameters, /* ResolutionDivisor = */ 1);
			ForegroundConvolutionOutput = BuildPostfilterPass(GatherParameters, GatherPass);

			if (bForegroundHybridScattering)
			{
				FRenderingCompositePass* ScatterPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFHybridScatter(GatherParameters, BokehModel));
				ScatterPass->SetInput(ePId_Input0, ForegroundConvolutionOutput);

				if (bEnableScatterBokehSettings)
					ScatterPass->SetInput(ePId_Input2, ScatteringBokehLUTOutput);

				ForegroundConvolutionOutput = FRenderingCompositeOutputRef(ScatterPass, ePId_Output0);
			}
		}

		// Wire hole filling gathering passes.
		if (bRecombineDoesSeparateForegroundHoleFilling)
		{
			FRCPassDiaphragmDOFGather::FParameters GatherParameters;
			GatherParameters.LayerProcessing = EDiaphragmDOFLayerProcessing::ForegroundHoleFilling;
			GatherParameters.PostfilterMethod = PostfilterMethod;

			ForegroundHoleFillingOutput = BuildGatherPass(GatherParameters, /* ResolutionDivisor = */ 1);
		}

		// Wire background gathering passes.
		{
			FRCPassDiaphragmDOFGather::FParameters GatherParameters;
			GatherParameters.LayerProcessing = EDiaphragmDOFLayerProcessing::BackgroundOnly;
			GatherParameters.PostfilterMethod = PostfilterMethod;

			if (bEnableGatherBokehSettings)
				GatherParameters.BokehSimulation = BokehSimulation;

			if (bUseLowAccumulatorQuality)
			{
				GatherParameters.QualityConfig = FRCPassDiaphragmDOFGather::EQualityConfig::LowQualityAccumulator;
			}

			FRenderingCompositeOutputRef LowerGatherOutput;
			if (GatheringLayout == EGatheringGraphLayout::SeparateHalfEighth)
			{
				FRCPassDiaphragmDOFGather::FParameters LowerGatherParameters = GatherParameters;
				LowerGatherParameters.RecursionMode = EDiaphragmDOFGatherRecursionMode::LowestMip;
				LowerGatherParameters.MinBorderingCocRadius = BackgroundCocRadiusMaximumForUniquePass;

				GatherParameters.RecursionMode = EDiaphragmDOFGatherRecursionMode::HighestMip;
				GatherParameters.MaxBorderingCocRadius = BackgroundCocRadiusMaximumForUniquePass;

				FRenderingCompositePass* LowerGatherPass = BuildGatherPass(LowerGatherParameters, /* ResolutionDivisor = */ 4);
				LowerGatherOutput = BuildPostfilterPass(LowerGatherParameters, LowerGatherPass);
			}
			else if (bBackgroundHybridScattering && BgdHybridScatteringMode == EHybridScatterMode::Occlusion)
			{
				GatherParameters.QualityConfig = FRCPassDiaphragmDOFGather::EQualityConfig::HighQualityWithHybridScatterOcclusion;
			}

			FRenderingCompositePass* GatherPass = BuildGatherPass(GatherParameters, /* ResolutionDivisor = */ 1);
			GatherPass->SetInput(ePId_Input2, LowerGatherOutput);
			BackgroundConvolutionOutput = BuildPostfilterPass(GatherParameters, GatherPass);

			if (bBackgroundHybridScattering)
			{
				FRenderingCompositePass* ScatterPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFHybridScatter(GatherParameters, BokehModel));
				ScatterPass->SetInput(ePId_Input0, BackgroundConvolutionOutput);

				if (bEnableScatterBokehSettings)
					ScatterPass->SetInput(ePId_Input2, ScatteringBokehLUTOutput);

				if (BgdHybridScatteringMode == EHybridScatterMode::Occlusion)
					ScatterPass->SetInput(ePId_Input3, FRenderingCompositeOutputRef(GatherPass, ePId_Output2));

				BackgroundConvolutionOutput = FRenderingCompositeOutputRef(ScatterPass, ePId_Output0);
			}
		}
	}

	// Gather slight out of focus.
	bool bEnableSlightOutOfFocusBokeh = bSupportGatheringBokehSimulation && bRecombineDoesSlightOutOfFocus && CVarEnableRecombineBokehSettings.GetValueOnRenderThread();
	if (bRecombineDoesSlightOutOfFocus)
	{
		FRCPassDiaphragmDOFGather::FParameters GatherParameters;
		GatherParameters.LayerProcessing = EDiaphragmDOFLayerProcessing::SlightOutOfFocus;
		GatherParameters.RingCount = FRCPassDiaphragmDOFGather::kMaxSlightOutOfFocusRingCount;
		GatherParameters.InputViewSize = PreprocessViewSize;
		GatherParameters.OutputViewSize = GatheringViewSize;
		GatherParameters.OutputBufferSize = RefBufferSize;

		if (bEnableSlightOutOfFocusBokeh)
			GatherParameters.BokehSimulation = BokehSimulation;

		FRenderingCompositePass* GatherPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFGather(
			GatherParameters));
		GatherPass->SetInput(ePId_Input0, PrefilterOutput); // TODO: take TAA input instead?
		GatherPass->SetInput(ePId_Input1, CocTileOutput);

		// Slight out of focus gather pass use exact same LUT as scattering because all samples of ther kernel are used.
		if (bEnableSlightOutOfFocusBokeh)
			GatherPass->SetInput(ePId_Input3, ScatteringBokehLUTOutput);

		SlightOutOfFocusConvolutionOutput = FRenderingCompositeOutputRef(GatherPass);
	}

	// Recombine lower res out of focus with full res scene color.
	{
		FRCPassDiaphragmDOFRecombine::FParameters Parameters;
		Parameters.CocModel = CocModel;
		Parameters.MainDrawEvent = MainDrawEvent;
		Parameters.Quality = RecombineQuality;

		if (bEnableSlightOutOfFocusBokeh)
			Parameters.BokehSimulation = BokehSimulation;

		FRenderingCompositePass* Recombine = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFRecombine(Parameters));
		Recombine->SetInput(ePId_Input0, FullresColorSetup);
		Recombine->SetInput(ePId_Input1, SeparateTranslucency);
		Recombine->SetInput(ePId_Input2, ForegroundConvolutionOutput);
		Recombine->SetInput(ePId_Input3, ForegroundHoleFillingOutput);
		Recombine->SetInput(ePId_Input4, BackgroundConvolutionOutput);
		Recombine->SetInput(ePId_Input5, SlightOutOfFocusConvolutionOutput);

		// Full res gathering for slight out of focus needs its dedicated LUT.
		if (bEnableSlightOutOfFocusBokeh && ScatteringBokehLUTOutput.IsValid() && SlightOutOfFocusConvolutionOutput.IsValid())
		{
			FRenderingCompositePass* BokehLUTPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassDiaphragmDOFBuildBokehLUT(
				BokehModel, FRCPassDiaphragmDOFBuildBokehLUT::EFormat::FullResOffsetToCocDistance));
			Recombine->SetInput(ePId_Input6, BokehLUTPass);
		}

		// Replace full res scene color with recombined output.
		Context.FinalOutput = FRenderingCompositeOutputRef(Recombine);
	}

	return true;
}
