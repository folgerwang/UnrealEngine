// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

TGlobalResource<TBoundShaderStateHistory<10000>> FMetalRHICommandContext::BoundShaderStateHistory;

FMetalDeviceContext& GetMetalDeviceContext()
{
	FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
	check(Context);
	return ((FMetalDeviceContext&)Context->GetInternalContext());
}

void SafeReleaseMetalObject(id Object)
{
	if(GIsMetalInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseObject(Object);
			return;
		}
	}
	[Object release];
}

void SafeReleaseMetalTexture(FMetalTexture& Object)
{
	if(GIsMetalInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseTexture(Object);
			return;
		}
	}
}

void SafeReleaseMetalBuffer(FMetalBuffer& Buffer)
{
	if(GIsMetalInitialized && GDynamicRHI && Buffer)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseBuffer(Buffer);
		}
	}
}

void SafeReleaseMetalFence(id Object)
{
	if(GIsMetalInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseFence((id<MTLFence>)Object);
			return;
		}
	}
}

FMetalRHICommandContext::FMetalRHICommandContext(class FMetalProfiler* InProfiler, FMetalContext* WrapContext)
: Context(WrapContext)
, Profiler(InProfiler)
, PendingVertexDataStride(0)
, PendingIndexDataStride(0)
, PendingPrimitiveType(0)
, PendingNumPrimitives(0)
{
	check(Context);
}

FMetalRHICommandContext::~FMetalRHICommandContext()
{
	delete Context;
}

FMetalRHIComputeContext::FMetalRHIComputeContext(class FMetalProfiler* InProfiler, FMetalContext* WrapContext)
: FMetalRHICommandContext(InProfiler, WrapContext)
{
}

FMetalRHIComputeContext::~FMetalRHIComputeContext()
{
}

void FMetalRHIComputeContext::RHISetAsyncComputeBudget(EAsyncComputeBudget Budget)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	FMetalRHICommandContext::RHISetAsyncComputeBudget(Budget);
}

void FMetalRHIComputeContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	FMetalRHICommandContext::RHISetComputeShader(ComputeShader);
}

void FMetalRHIComputeContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	FMetalRHICommandContext::RHISetComputePipelineState(ComputePipelineState);
}

void FMetalRHIComputeContext::RHISubmitCommandsHint()
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	Context->FinishFrame();
	
#if ENABLE_METAL_GPUPROFILE
	FMetalContext::MakeCurrent(&GetMetalDeviceContext());
#endif
}

FMetalRHIImmediateCommandContext::FMetalRHIImmediateCommandContext(class FMetalProfiler* InProfiler, FMetalContext* WrapContext)
	: FMetalRHICommandContext(InProfiler, WrapContext)
{
}

void FMetalRHICommandContext::RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
{
	// Fallback...
	InInfo.Validate();
	
	if (InInfo.bGeneratingMips)
	{
		FRHITexture* Textures[MaxSimultaneousRenderTargets];
		FRHITexture** LastTexture = Textures;
		for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
		{
			if (!InInfo.ColorRenderTargets[Index].RenderTarget)
			{
				break;
			}
			
			*LastTexture = InInfo.ColorRenderTargets[Index].RenderTarget;
			++LastTexture;
		}
		
		//Use RWBarrier since we don't transition individual subresources.  Basically treat the whole texture as R/W as we walk down the mip chain.
		int32 NumTextures = (int32)(LastTexture - Textures);
		if (NumTextures)
		{
			IRHICommandContext::RHITransitionResources(EResourceTransitionAccess::ERWSubResBarrier, Textures, NumTextures);
		}
	}
	
	if (InInfo.bOcclusionQueries)
	{
		Context->GetCommandList().SetParallelIndex(0, 0);
	}

	FRHISetRenderTargetsInfo RenderTargetsInfo;
	InInfo.ConvertToRenderTargetsInfo(RenderTargetsInfo);

	@autoreleasepool {
	bool bHasTarget = (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr);
	if (Context->GetCommandQueue().SupportsFeature(EMetalFeaturesGraphicsUAVs))
	{
		for (uint32 i = 0; !bHasTarget && i < RenderTargetsInfo.NumUAVs; i++)
		{
			if (IsValidRef(RenderTargetsInfo.UnorderedAccessView[i]))
			{
				bHasTarget = true;
			}
		}
	}
	else
	{
		checkf(RenderTargetsInfo.NumUAVs == 0, TEXT("Calling SetRenderTargets with UAVs is not supported in this Metal standard"));
	}
	
	for (uint32 i = 0; bHasTarget == false && i < RenderTargetsInfo.NumColorRenderTargets; i++)
	{
		bHasTarget = (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr);
	}

	// Ignore any attempt to "clear" the render-targets as that is senseless with the way MetalRHI has to try and coalesce passes.
	if (bHasTarget)
	{
		Context->SetRenderTargetsInfo(RenderTargetsInfo);

		// Set the viewport to the full size of render target 0.
		if (RenderTargetsInfo.ColorRenderTarget[0].Texture)
		{
			const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[0];
			FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);

			uint32 Width = FMath::Max((uint32)(RenderTarget->Texture.GetWidth() >> RenderTargetView.MipIndex), (uint32)1);
			uint32 Height = FMath::Max((uint32)(RenderTarget->Texture.GetHeight() >> RenderTargetView.MipIndex), (uint32)1);

			RHISetViewport(0, 0, 0.0f, Width, Height, 1.0f);
		}
	}
	}
	
	RenderPassInfo = InInfo;
	if (InInfo.bOcclusionQueries)
	{
		RHIBeginOcclusionQueryBatch(InInfo.NumOcclusionQueries);
	}
}

void FMetalRHICommandContext::RHIEndRenderPass()
{
	if (RenderPassInfo.bOcclusionQueries)
	{
		RHIEndOcclusionQueryBatch();
	}
	
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		if (!RenderPassInfo.ColorRenderTargets[Index].RenderTarget)
		{
			break;
		}
		if (RenderPassInfo.ColorRenderTargets[Index].ResolveTarget)
		{
			RHICopyToResolveTarget(RenderPassInfo.ColorRenderTargets[Index].RenderTarget, RenderPassInfo.ColorRenderTargets[Index].ResolveTarget, RenderPassInfo.ResolveParameters);
		}
	}
	
	if (RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget && RenderPassInfo.DepthStencilRenderTarget.ResolveTarget)
	{
		RHICopyToResolveTarget(RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget, RenderPassInfo.DepthStencilRenderTarget.ResolveTarget, RenderPassInfo.ResolveParameters);
	}
}

void FMetalRHICommandContext::RHIBeginComputePass(const TCHAR* InName)
{
	RHISetRenderTargets(0, nullptr, nullptr, 0, nullptr);
}

void FMetalRHICommandContext::RHIEndComputePass()
{
}
