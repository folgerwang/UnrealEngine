// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderTargetPool.h: Scene render target pool manager.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"


/**
 * Pools all resources for the render graph.
 */
class RENDERCORE_API FRenderGraphResourcePool : public FRenderResource
{
public:
	FRenderGraphResourcePool();

	/** Allocate a buffer from a given descriptor. */
	void FindFreeBuffer(
		FRHICommandList& RHICmdList,
		const FRDGBufferDesc& Desc,
		TRefCountPtr<FPooledRDGBuffer>& Out,
		const TCHAR* InDebugName);

	/** Free renderer resources */
	virtual void ReleaseDynamicRHI() override;

	/** Good to call between levels or before memory intense operations. */
	void FreeUnusedResources();

private:
	/** Elements can be 0, we compact the buffer later. */
	TArray< TRefCountPtr<FPooledRDGBuffer> > AllocatedBuffers;
};

/** The global render targets for easy shading. */
extern RENDERCORE_API TGlobalResource<FRenderGraphResourcePool> GRenderGraphResourcePool;
