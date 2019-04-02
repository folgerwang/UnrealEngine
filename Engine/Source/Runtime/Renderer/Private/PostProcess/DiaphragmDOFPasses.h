// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DiaphragmDOFPasses.h: Passes of diaphragm DOF.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "DiaphragmDOF.h"


/** Defines which layer to process. */
enum class EDiaphragmDOFLayerProcessing
{
	// Foreground layer only.
	ForegroundOnly,

	// Foreground hole filling.
	ForegroundHoleFilling,

	// Background layer only.
	BackgroundOnly,

	// Both foreground and background layers.
	ForegroundAndBackground,

	// Slight out of focus layer.
	SlightOutOfFocus,

	MAX
};



/** Defines which layer to process. */
enum class EDiaphragmDOFPostfilterMethod
{
	// Disable post filtering.
	None,

	// Per RGB channel median on 3x3 neighborhood.
	RGBMedian3x3,

	// Per RGB channel max on 3x3 neighborhood.
	RGBMax3x3,

	MAX
};


/** Modes to simulate a bokeh. */
enum class EDiaphragmDOFBokehSimulation
{
	// No bokeh simulation.
	Disabled,

	// Symmetric bokeh (even number of blade).
	SimmetricBokeh,

	// Generic bokeh.
	GenericBokeh,

	MAX,
};


// 
// ePId_Input0: SceneDepth
class FRCPassDiaphragmDOFFlattenCoc : public TRenderingCompositePassBase<1, 2>
{
public:
	/** Resolution divisor of the Coc tiles. */
	static constexpr int32 CocTileResolutionDivisor = 8;

	/** Configuration parameters of the dilate pass. */
	struct FParameters
	{
		/** Size of the view input. */
		FIntPoint InputViewSize;

		/** Size of the gathering view */
		FIntPoint GatherViewSize;

		FParameters()
		{ }
	};


	FRCPassDiaphragmDOFFlattenCoc(const FParameters& InParams)
		: Params(InParams)
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};

// 
// ePId_Input0: SceneDepth
class FRCPassDiaphragmDOFDilateCoc : public TRenderingCompositePassBase<4, 2>
{
public:
	/** Resolution divisor of the Coc tiles. */
	static constexpr int32 MaxSampleRadiusCount = 3;

	/** Dilate mode of the pass. */
	enum class EMode
	{
		// One single dilate pass.
		StandAlone,

		// Dilate min foreground and max background coc radius.
		MinForegroundAndMaxBackground,

		// Dilate everything else from dilated min foreground and max background coc radius.
		MinimalAbsoluteRadiuses,

		MAX
	};

	/** Configuration parameters of the dilate pass. */
	struct FParameters
	{
		/** Mode of teh dilate. */
		EMode Mode;

		/** Radius in number of samples. 0 means the dilate pass should not be run. */
		int32 SampleRadiusCount;

		/** Multiplier between samples. */
		int32 SampleDistanceMultiplier;

		/** Size of the gathering view */
		FIntPoint GatherViewSize;

		/** Convert pre-processing coc radius to processing coc radius */
		float PreProcessingToProcessingCocRadiusFactor;

		// Error introduced by the random offset of the gathering kernel.
		float BluringRadiusErrorMultiplier;

		FParameters()
			: Mode(EMode::StandAlone)
			, SampleRadiusCount(0)
			, SampleDistanceMultiplier(1)
			, PreProcessingToProcessingCocRadiusFactor(1.0f)
			, BluringRadiusErrorMultiplier(1.0f)
		{ }
	};

	FRCPassDiaphragmDOFDilateCoc(const FParameters& InParams)
		: Params(InParams)
	{
		check(Params.SampleRadiusCount > 0);
		check(Params.SampleDistanceMultiplier == 1 || Params.Mode != EMode::StandAlone);
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};

// 
// ePId_Input0: SceneColor
// ePId_Input0: SceneDepth
class FRCPassDiaphragmDOFSetup : public TRenderingCompositePassBase<2, 3>
{
public:
	/** Configuration parameters of the dilate pass. */
	struct FParameters
	{
		/** Coc model. */
		DiaphragmDOF::FPhysicalCocModel CocModel;

		bool bOutputFullResolution;
		bool bOutputHalfResolution;

		/** Basis of the CocRadius for the full resolution and half resolution outputs of the pass. */
		float FullResCocRadiusBasis;
		float HalfResCocRadiusBasis;

		FParameters()
			: bOutputFullResolution(false)
			, bOutputHalfResolution(false)
			, FullResCocRadiusBasis(1.0f)
			, HalfResCocRadiusBasis(1.0f)
		{ }
	};

	FRCPassDiaphragmDOFSetup(const FParameters& InParams)
		: Params(InParams)
	{ }

	// Main draw even that contains all DiaphragmDOF's passes.
	TDrawEvent<FRHICommandList> MainDrawEvent;

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};

// 
class FRCPassDiaphragmDOFReduce : public TRenderingCompositePassBase<3, 2>
{
public:
	/** Resolution divisor of the Coc tiles. */
	static constexpr int32 kMaxMipLevelCount = 4;

	/** Configuration parameters of the dilate pass. */
	struct FParameters
	{
		/** Input's resolution divisor. */
		int32 InputResolutionDivisor;

		/** Radius in number of samples. 0 means the dilate pass should not be run. */
		int32 MipLevelCount;

		// Whether should extract hybrid scattered pixels.
		bool bExtractForegroundHybridScattering;
		bool bExtractBackgroundHybridScattering;

		// The minimum coc radius to scatter a bokeh.
		float MinScatteringCocRadius;

		// Ratio of pixel quad allowed to be scattered.
		float MaxScatteringRatio;

		// Size of the input view that is also the size of the output view in mip 0.
		FIntPoint InputViewSize;

		/** Convert pre-processing coc radius to processing coc radius */
		float PreProcessingToProcessingCocRadiusFactor;

		/** Whether to use R11G11B10 + separate C0C buffer. */
		bool bRGBBufferSeparateCocBuffer;

		FParameters()
			: InputResolutionDivisor(2)
			, MipLevelCount(FRCPassDiaphragmDOFReduce::kMaxMipLevelCount)
			, bExtractForegroundHybridScattering(false)
			, bExtractBackgroundHybridScattering(false)
			, MinScatteringCocRadius(3.0f)
			, MaxScatteringRatio(0.1f)
			, PreProcessingToProcessingCocRadiusFactor(1.0f)
			, bRGBBufferSeparateCocBuffer(false)
		{ }
	};


	FRCPassDiaphragmDOFReduce(const FParameters& InParams)
		: Params(InParams)
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};

// 
// ePId_Input0: (SceneColor.rgb; Coc)
class FRCPassDiaphragmDOFDownsample : public TRenderingCompositePassBase<2, 2>
{
public:
	/** Configuration parameters of the dilate pass. */
	struct FParameters
	{
		// Size of the input view that is also the size of the output view in mip 0.
		FIntPoint InputViewSize;

		/** Multiplier to apply to the CocRadius. */
		float OutputCocRadiusMultiplier;

		/** Whether to use R11G11B10 only. */
		bool bRGBBufferOnly;

		FParameters()
			: OutputCocRadiusMultiplier(1.0f)
			, bRGBBufferOnly(false)
		{ }
	};

	FRCPassDiaphragmDOFDownsample(const FParameters& InParams) 
		: Params(InParams)
	{ }

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};

// 
// ePId_Input0: (SceneColor.rgb; Coc)
class FRCPassDiaphragmDOFPrefilter : public TRenderingCompositePassBase<2, 2>
{
public:
	FRCPassDiaphragmDOFPrefilter(void) {}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// 
// ePId_Input0: DOFPrefilter's Output0
// ePId_Input1: DOFPrefilter's Output1
// ePId_Input2: CocGather
class FRCPassDiaphragmDOFGather : public TRenderingCompositePassBase<5, 3>
{
public:
	/** Minimum number of ring. */
	static constexpr int32 kMinRingCount = 3;

	/** Maximum number of ring for slight out of focus pass. Same as USH's MAX_RECOMBINE_ABS_COC_RADIUS. */
	static constexpr int32 kMaxSlightOutOfFocusRingCount = 3;

	/** Returns the number maximum number of ring available. */
	static FORCEINLINE int32 MaxRingCount(EShaderPlatform ShaderPlatform)
	{
		if (IsPCPlatform(ShaderPlatform))
		{
			return 5;
		}
		return 4;
	}

	/** Returns whether the shader for bokeh simulation are compiled. */
	static FORCEINLINE bool SupportsBokehSimmulation(EShaderPlatform ShaderPlatform)
	{
		// Shaders of gathering pass are big, so only compile them on desktop.
		return IsPCPlatform(ShaderPlatform);
	}

	/** Returns whether separate coc buffer is supported. */
	static FORCEINLINE bool SupportRGBColorBuffer(EShaderPlatform ShaderPlatform)
	{
		// There is no point when alpha channel is supported because needs 4 channel anyway for fast gathering tiles.
		if (FPostProcessing::HasAlphaChannelSupport())
		{
			return false;
		}

		// There is high number of UAV to write in reduce pass.
		return ShaderPlatform == SP_PS4 || ShaderPlatform == SP_XBOXONE_D3D12 || ShaderPlatform == SP_VULKAN_SM5;
	}

	/** Quality configurations for gathering passes. */
	enum class EQualityConfig
	{
		// Lower but faster accumulator.
		LowQualityAccumulator,

		// High quality accumulator.
		HighQuality,

		// High quality accumulator with hybrid scatter occlusion buffer output.
		// TODO: distinct shader permutation dimension for hybrid scatter occlusion?
		HighQualityWithHybridScatterOcclusion,

		// High quality accumulator, with layered full disks and hybrid scatter occlusion.
		Cinematic,

		MAX,
	};

	/** Configuration parameters of the gathering pass. */
	struct FParameters
	{
		/** Which layer to gather. */
		EDiaphragmDOFLayerProcessing LayerProcessing;

		/** Configuration of the pass. */
		EQualityConfig QualityConfig;

		/** Number of sampling rings on the gathering kernel, excluding the center sample. */
		int32 RingCount;

		/** Min and max coc radius. */
		float MinBorderingCocRadius;
		float MaxBorderingCocRadius;

		/** Postfilter method to apply on this gather pass. */
		EDiaphragmDOFPostfilterMethod PostfilterMethod;

		/** Bokeh simulation to do. */
		EDiaphragmDOFBokehSimulation BokehSimulation;

		/** Size of the view input. */
		FIntPoint InputViewSize;

		/** Size of the gathering view. */
		FIntPoint OutputViewSize;

		FIntPoint OutputBufferSize;

		/** Whether to use R11G11B10 + separate COC buffer. */
		bool bRGBBufferSeparateCocBuffer;

		FParameters()
			: LayerProcessing(EDiaphragmDOFLayerProcessing::ForegroundAndBackground)
			, QualityConfig(EQualityConfig::HighQuality)
			, RingCount(3)
			, MinBorderingCocRadius(0.0f)
			, MaxBorderingCocRadius(0.0f)
			, PostfilterMethod(EDiaphragmDOFPostfilterMethod::None)
			, BokehSimulation(EDiaphragmDOFBokehSimulation::Disabled)
			, bRGBBufferSeparateCocBuffer(false)
		{ }
	};

	FRCPassDiaphragmDOFGather(const FParameters& InParams)
		: Params(InParams)
	{ }

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};

// 
class FRCPassDiaphragmDOFPostfilter : public TRenderingCompositePassBase<4, 2>
{
public:
	/** Configuration parameters of the gather pass to post filter.
	 * Only considers LayerProcessing and ResolutionDivisor.
	 */
	using FParameters = FRCPassDiaphragmDOFGather::FParameters;

	FRCPassDiaphragmDOFPostfilter(const FParameters& InParams)
		: Params(InParams)
	{ }

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};


/** Compile a LUT for bokeh simulation. */
class FRCPassDiaphragmDOFBuildBokehLUT : public TRenderingCompositePassBase<0, 1>
{
public:
	/** Width and height of the look up table. */
	static constexpr int32 kLUTSize = 32;


	/** Format of the LUT to generate. */
	enum class EFormat
	{
		// Look up table that stores a factor to transform a CocRadius to a BokehEdge distance.
		// Used for scattering and low res focus gathering.
		CocRadiusToBokehEdgeFactor,

		// Look up table that stores Coc distance to compare against neighbor's CocRadius.
		// Used exclusively for full res gathering in recombine pass.
		FullResOffsetToCocDistance,


		// Look up table to stores the gathering sample pos within the kernel.
		// Used for low res back and foreground gathering.
		GatherSamplePos,

		MAX,
	};


	FRCPassDiaphragmDOFBuildBokehLUT(const DiaphragmDOF::FBokehModel& InBokehModel, EFormat InFormat)
		: BokehModel(InBokehModel)
		, Format(InFormat)
	{ }

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const DiaphragmDOF::FBokehModel BokehModel;
	const EFormat Format;
};

/** Draw the scattered pixel directly on the gather output. 
 *
 * Note: Input0=Output0.
 */
class FRCPassDiaphragmDOFHybridScatter : public TRenderingCompositePassBase<4, 1>
{
public:
	/** Absolute minim coc radius required for a bokeh to be scattered */
	static constexpr float kMinCocRadius = 3.0f;

	/** Returns whether hybrid scattering is supported. */
	static FORCEINLINE bool IsSupported(EShaderPlatform ShaderPlatform)
	{
		return !IsSwitchPlatform(ShaderPlatform);
	}

	// TODO: pixel shader post filtering to have hierarchical RT layout.
	/** Configuration parameters of the gather pass to post filter.
	 */
	using FParameters = FRCPassDiaphragmDOFPostfilter::FParameters;

	FRCPassDiaphragmDOFHybridScatter(const FParameters& InParams, const DiaphragmDOF::FBokehModel& InBokehModel)
		: BokehModel(InBokehModel)
		, Params(InParams)
	{ }

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const DiaphragmDOF::FBokehModel BokehModel;
	const FParameters Params;
};


// 
// ePId_Input0: SceneColor
// ePId_Input1: half res foreground
// ePId_Input2: half res background
class FRCPassDiaphragmDOFRecombine : public TRenderingCompositePassBase<11, 1>
{
public:

	/** Maximum quality level of the recombine pass. */
	static constexpr int32 kMaxQuality = 2;

	/** Configuration parameters of the gathering pass. */
	struct FParameters
	{
		/** Coc model to use. */
		DiaphragmDOF::FPhysicalCocModel CocModel;

		/** Bokeh simulation to do. */
		EDiaphragmDOFBokehSimulation BokehSimulation;

		/** Poiter of the main's draw event for diaphragm DOF to end. */
		TDrawEvent<FRHICommandList>* MainDrawEvent;

		/** Quality of the recombine. */
		int32 Quality;

		/** Size of the gathering viewport. */
		FIntPoint GatheringViewSize;

		FParameters()
			: BokehSimulation(EDiaphragmDOFBokehSimulation::Disabled)
			, MainDrawEvent(nullptr)
			, Quality(kMaxQuality)
		{ }
	};


	FRCPassDiaphragmDOFRecombine(const FParameters& InParams)
		: Params(InParams)
	{ }

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};
