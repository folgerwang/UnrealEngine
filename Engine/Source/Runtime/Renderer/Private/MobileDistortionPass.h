// Copyright 1998-2019 Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FRCDistortionAccumulatePassES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCDistortionAccumulatePassES2(FIntPoint InPrePostSourceViewportSize, FScene* InScene) 
		: PrePostSourceViewportSize(InPrePostSourceViewportSize) 
		, Scene(InScene)
	{ }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
	virtual const TCHAR* GetDebugName() { return TEXT("FRCDistortionAccumulatePassES2"); }
private:
	FIntPoint PrePostSourceViewportSize;
	FScene* Scene; 
};

class FRCDistortionMergePassES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCDistortionMergePassES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
	virtual const TCHAR* GetDebugName() { return TEXT("FRCDistortionMergePassES2"); }
private:
	FIntPoint PrePostSourceViewportSize;
};

// Returns whether distortion is enabled and there primitives to draw
bool IsMobileDistortionActive(const FViewInfo& View);
