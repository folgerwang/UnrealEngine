// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderingCompositionGraph.h"


/** Lists of TAA configurations. */
enum class ETAAPassConfig
{
	LegacyDepthOfField,
	Main,
	ScreenSpaceReflections,
	LightShaft,
	MainUpsampling,
	DiaphragmDOF,
	DiaphragmDOFUpsampling,
	MainSuperSampling,

	MAX
};


static FORCEINLINE bool IsTAAUpsamplingConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::MainUpsampling || Pass == ETAAPassConfig::DiaphragmDOFUpsampling || Pass == ETAAPassConfig::MainSuperSampling;
}

static FORCEINLINE bool IsMainTAAConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::Main || Pass == ETAAPassConfig::MainUpsampling || Pass == ETAAPassConfig::MainSuperSampling;
}

static FORCEINLINE bool IsDOFTAAConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::DiaphragmDOF || Pass == ETAAPassConfig::DiaphragmDOFUpsampling;
}


/** Configuration of TAA. */
struct FTAAPassParameters
{
	// TAA pass to run.
	ETAAPassConfig Pass;

	// Whether to use the faster shader permutation.
	bool bUseFast;

	// Whether to do compute or not.
	bool bIsComputePass;

	// Whether downsampled (box filtered, half resolution) frame should be written out.
	// Only used when bIsComputePass is true.
	bool bDownsample;
	EPixelFormat DownsampleOverrideFormat;

	// Viewport rectangle of the input and output of TAA at ResolutionDivisor == 1.
	FIntRect InputViewRect;
	FIntRect OutputViewRect;

	// Resolution divisor.
	int32 ResolutionDivisor;


	FTAAPassParameters(const FViewInfo& View)
		: Pass(ETAAPassConfig::Main)
		, bUseFast(false)
		, bIsComputePass(false)
		, bDownsample(false)
		, DownsampleOverrideFormat(PF_Unknown)
		, InputViewRect(View.ViewRect)
		, OutputViewRect(View.ViewRect)
		, ResolutionDivisor(1)
	{ }


	// Customises the view rectangles for input and output.
	FORCEINLINE void SetupViewRect(const FViewInfo& View, int32 InResolutionDivisor = 1)
	{
		ResolutionDivisor = InResolutionDivisor;

		InputViewRect = View.ViewRect;

		// When upsampling, always upsampling to top left corner to reuse same RT as before upsampling.
		if (IsTAAUpsamplingConfig(Pass))
		{
			OutputViewRect.Min = FIntPoint(0, 0);
			OutputViewRect.Max =  View.GetSecondaryViewRectSize();
		}
		else
		{
			OutputViewRect = InputViewRect;
		}
	}

	// Shifts input and output view rect to top left corner
	FORCEINLINE void TopLeftCornerViewRects()
	{
		InputViewRect.Max -= InputViewRect.Min;
		InputViewRect.Min = FIntPoint::ZeroValue;
		OutputViewRect.Max -= OutputViewRect.Min;
		OutputViewRect.Min = FIntPoint::ZeroValue;
	}
};


// ePId_Input0: Full Res Scene color (point)
// ePId_Input2: Velocity (point)
// ePId_Output0: Antialiased color
// ePId_Output1: Downsampled antialiased color (only when FTAAConfig::bDownsample is true)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessTemporalAA : public TRenderingCompositePassBase<3, 3>
{
public:
	FRCPassPostProcessTemporalAA(
		const class FPostprocessContext& Context,
		const FTAAPassParameters& Parameters,
		const FTemporalAAHistory& InInputHistory,
		FTemporalAAHistory* OutOutputHistory);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	virtual FComputeFenceRHIParamRef GetComputePassEndFence() const override { return AsyncEndFence; }

	bool IsDownsamplePossible() const { return bDownsamplePossible; }

private:
	const FTAAPassParameters Parameters;

	FIntPoint OutputExtent;

	FComputeFenceRHIRef AsyncEndFence;

	const FTemporalAAHistory& InputHistory;
	FTemporalAAHistory* OutputHistory;

	bool bDownsamplePossible = false;
};
