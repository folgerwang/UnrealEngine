// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureFeedback.h"
#include "VisualizeTexture.h"

#include "ClearQuad.h"

FVirtualTextureFeedback::FVirtualTextureFeedback()
	: Size( 0, 0 )
	, GPUWriteIndex(0)
	, CPUReadIndex(0)
	, PendingTargetCount(0)
{
}

void FVirtualTextureFeedback::ReleaseResources()
{
	GRenderTargetPool.FreeUnusedResource( FeedbackTextureGPU );
	for (int i = 0; i < TargetCapacity; ++i)
	{
		if (FeedbackTextureCPU[i].TextureCPU.IsValid())
		{
			GRenderTargetPool.FreeUnusedResource(FeedbackTextureCPU[i].TextureCPU);
		}
		FeedbackTextureCPU[i].GPUFenceRHI.SafeRelease();
	}
	CPUReadIndex = 0u;
	GPUWriteIndex = 0u;
	PendingTargetCount = 0u;
}

void FVirtualTextureFeedback::CreateResourceGPU( FRHICommandListImmediate& RHICmdList, FIntPoint InSize)
{
	Size = InSize;

	FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( Size, PF_R32_UINT, FClearValueBinding::None, TexCreate_None, TexCreate_UAV, false ) );
	GRenderTargetPool.FindFreeElement( RHICmdList, Desc, FeedbackTextureGPU, TEXT("VTFeedbackGPU") );

	// Clear to default value
	const uint32 ClearValue[4] = { ~0u, ~0u, ~0u, ~0u };
	ClearUAV( RHICmdList, FeedbackTextureGPU->GetRenderTargetItem(), ClearValue );
	RHICmdList.TransitionResource( EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToGfx, FeedbackTextureGPU->GetRenderTargetItem().UAV );
}

void FVirtualTextureFeedback::MakeSnapshot(const FVirtualTextureFeedback& SnapshotSource)
{
	Size = SnapshotSource.Size;
	FeedbackTextureGPU = GRenderTargetPool.MakeSnapshot(SnapshotSource.FeedbackTextureGPU);
	for (int i = 0; i < TargetCapacity; ++i)
	{
		FeedbackTextureCPU[i].TextureCPU = GRenderTargetPool.MakeSnapshot(SnapshotSource.FeedbackTextureCPU[i].TextureCPU);
	}
}

void FVirtualTextureFeedback::TransferGPUToCPU( FRHICommandListImmediate& RHICmdList, FIntRect const& Rect )
{
	RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, FeedbackTextureGPU->GetRenderTargetItem().UAV );

	GVisualizeTexture.SetCheckPoint( RHICmdList, FeedbackTextureGPU );
	
	if (PendingTargetCount >= TargetCapacity)
	{
		// If we have too many pending transfers, start throwing away the oldest

		// will need to allocate a new fence,
		// since the previous fence will still be set on the old CopyToResolveTarget command (which we will not ignore/discard)
		FeedBackItem& DiscardFeedbackEntry = FeedbackTextureCPU[CPUReadIndex];
		DiscardFeedbackEntry.GPUFenceRHI.SafeRelease();

		--PendingTargetCount;
		CPUReadIndex = (CPUReadIndex + 1) % TargetCapacity;
	}

	FeedBackItem& FeedbackEntryCPU = FeedbackTextureCPU[GPUWriteIndex];
	FeedbackEntryCPU.Rect.Min = FIntPoint(Rect.Min.X / 16, Rect.Min.Y / 16);
	FeedbackEntryCPU.Rect.Max = FIntPoint((Rect.Max.X + 15) / 16, (Rect.Max.Y + 15) / 16);

	const FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Size, PF_R32_UINT, FClearValueBinding::None, TexCreate_CPUReadback | TexCreate_HideInVisualizeTexture, TexCreate_None, false));

	const TCHAR* DebugNames[TargetCapacity] = { TEXT("VTFeedbackCPU_0") , TEXT("VTFeedbackCPU_1") , TEXT("VTFeedbackCPU_2") , TEXT("VTFeedbackCPU_3") };
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, FeedbackEntryCPU.TextureCPU, DebugNames[GPUWriteIndex]);

	if (!FeedbackEntryCPU.GPUFenceRHI)
	{
		FeedbackEntryCPU.GPUFenceRHI = RHICmdList.CreateGPUFence(DebugNames[GPUWriteIndex]);
	}

	// Transfer memory GPU -> CPU
	FeedbackEntryCPU.GPUFenceRHI->Clear();
	RHICmdList.CopyToResolveTarget(
		FeedbackTextureGPU->GetRenderTargetItem().TargetableTexture,
		FeedbackEntryCPU.TextureCPU->GetRenderTargetItem().ShaderResourceTexture,
		FResolveParams());
	RHICmdList.WriteGPUFence(FeedbackEntryCPU.GPUFenceRHI);
	
	GRenderTargetPool.FreeUnusedResource( FeedbackTextureGPU );

	GPUWriteIndex = (GPUWriteIndex + 1) % TargetCapacity;
	++PendingTargetCount;
}

bool FVirtualTextureFeedback::Map( FRHICommandListImmediate& RHICmdList, MapResult& OutResult )
{
	const FeedBackItem& FeedbackEntryCPU = FeedbackTextureCPU[CPUReadIndex];
	if (PendingTargetCount > 0u &&
		FeedbackEntryCPU.TextureCPU.IsValid() &&
		FeedbackEntryCPU.GPUFenceRHI->Poll())
	{
		OutResult.MapHandle = CPUReadIndex;
		OutResult.Rect = FeedbackEntryCPU.Rect;

		int32 LockHeight = 0;
		// We can avoid flushing RHI thread here, since we wait for GPU fence to complete
		const bool bFlushRHIThread = false;
		RHICmdList.MapStagingSurface(FeedbackEntryCPU.TextureCPU->GetRenderTargetItem().ShaderResourceTexture, *(void**)&OutResult.Buffer, OutResult.Pitch, LockHeight, bFlushRHIThread);

		check(PendingTargetCount > 0u);
		--PendingTargetCount;
		CPUReadIndex = (CPUReadIndex + 1) % TargetCapacity;

		return true;
	}

	return false;
}

void FVirtualTextureFeedback::Unmap( FRHICommandListImmediate& RHICmdList, int32 MapHandle )
{
	check(FeedbackTextureCPU[MapHandle].TextureCPU.IsValid());

	const bool bFlushRHIThread = false;
	RHICmdList.UnmapStagingSurface(FeedbackTextureCPU[MapHandle].TextureCPU->GetRenderTargetItem().ShaderResourceTexture, bFlushRHIThread);
	GRenderTargetPool.FreeUnusedResource(FeedbackTextureCPU[MapHandle].TextureCPU);
}
