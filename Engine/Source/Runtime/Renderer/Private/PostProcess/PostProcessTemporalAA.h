// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

	MAX
};


static FORCEINLINE bool IsTAAUpsamplingConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::MainUpsampling || Pass == ETAAPassConfig::DiaphragmDOFUpsampling;
}

static FORCEINLINE bool IsMainTAAConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::Main || Pass == ETAAPassConfig::MainUpsampling;
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

	// Viewport rectangle of the input and output of TAA at ResolutionDivisor == 1.
	FIntRect InputViewRect;
	FIntRect OutputViewRect;

	// Resolution divisor.
	int32 ResolutionDivisor;


	FTAAPassParameters(const FViewInfo& View)
		: Pass(ETAAPassConfig::Main)
		, bUseFast(false)
		, bIsComputePass(false)
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
		InputViewRect -= InputViewRect.Min;
		OutputViewRect -= OutputViewRect.Min;
	}
};


// ePId_Input0: Full Res Scene color (point)
// ePId_Input2: Velocity (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessTemporalAA : public TRenderingCompositePassBase<2, 1>
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

private:
	const FTAAPassParameters Parameters;

	FIntPoint OutputExtent;

	FComputeFenceRHIRef AsyncEndFence;

	const FTemporalAAHistory& InputHistory;
	FTemporalAAHistory* OutputHistory;
};
