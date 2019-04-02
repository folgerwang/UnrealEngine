// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderingCompositionGraph.h"


class FRCPassMitchellNetravaliDownsample : public TRenderingCompositePassBase<1, 1>
{
public:
	struct FParameters
	{
		FIntRect InputViewRect;

		FIntRect OutputViewRect;

		FIntPoint OutputExtent;

	};


	FRCPassMitchellNetravaliDownsample(const FParameters& InParams)
		: Params(InParams)
	{
		bIsComputePass = true;
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	const FParameters Params;
};
