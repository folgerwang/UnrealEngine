// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "SceneViewExtension.h"
#include "UObject/WeakObjectPtr.h"

class AComposurePipelineBaseActor;

/**
 *	
 */
class FComposureViewExtension : public FSceneViewExtensionBase
{
public:
	FComposureViewExtension(const FAutoRegister& AutoRegister, AComposurePipelineBaseActor* Owner);

public:
	//~ ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}
	virtual int32 GetPriority() const override;
	virtual bool IsActiveThisFrame(FViewport* InViewport) const override;

private:
	TWeakObjectPtr<AComposurePipelineBaseActor> AssociatedPipelineObj;
};