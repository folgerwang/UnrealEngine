// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Slate3DRenderer.h"
#include "Fonts/FontCache.h"
#include "Widgets/SWindow.h"
#include "SceneUtils.h"
#include "SlateRHIRenderer.h"
#include "Rendering/ElementBatcher.h"

DECLARE_GPU_STAT_NAMED(Slate3D, TEXT("Slate 3D"));

FSlate3DRenderer::FSlate3DRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, bool bUseGammaCorrection )
	: SlateFontServices( InSlateFontServices )
	, ResourceManager( InResourceManager )
{
	const int32 InitialBufferSize = 200;
	RenderTargetPolicy = MakeShareable( new FSlateRHIRenderingPolicy( SlateFontServices, ResourceManager, InitialBufferSize ) );
	RenderTargetPolicy->SetUseGammaCorrection( bUseGammaCorrection );

	ElementBatcher = MakeShareable(new FSlateElementBatcher(RenderTargetPolicy.ToSharedRef()));
}

void FSlate3DRenderer::Cleanup()
{
	if ( RenderTargetPolicy.IsValid() )
	{
		RenderTargetPolicy->ReleaseResources();
	}

	if (IsInGameThread())
	{
		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_RENDER_COMMAND(FSlate3DRenderer_Cleanup)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				DepthStencil.SafeRelease();
			}
		);
	}
	else
	{
		DepthStencil.SafeRelease();
	}

	BeginCleanup(this);
}

void FSlate3DRenderer::SetUseGammaCorrection(bool bUseGammaCorrection)
{
	RenderTargetPolicy->SetUseGammaCorrection(bUseGammaCorrection);
}

FSlateDrawBuffer& FSlate3DRenderer::GetDrawBuffer()
{
	FreeBufferIndex = (FreeBufferIndex + 1) % NUM_DRAW_BUFFERS;
	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];

	while (!Buffer->Lock())
	{
		FlushRenderingCommands();

		UE_LOG(LogSlate, Log, TEXT("Slate: Had to block on waiting for a draw buffer"));
		FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;

		Buffer = &DrawBuffers[FreeBufferIndex];
	}

	Buffer->ClearBuffer();

	return *Buffer;
}

void FSlate3DRenderer::DrawWindow_GameThread(FSlateDrawBuffer& DrawBuffer)
{
	check( IsInGameThread() );

	const TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetGameThreadFontCache();

	const TArray<TSharedRef<FSlateWindowElementList>>& WindowElementLists = DrawBuffer.GetWindowElementLists();

	for (int32 WindowIndex = 0; WindowIndex < WindowElementLists.Num(); WindowIndex++)
	{
		FSlateWindowElementList& ElementList = *WindowElementLists[WindowIndex];

		SWindow* Window = ElementList.GetPaintWindow();

		if (Window)
		{
			const FVector2D WindowSize = Window->GetSizeInScreen();
			if (WindowSize.X > 0 && WindowSize.Y > 0)
			{
				// Add all elements for this window to the element batcher
				ElementBatcher->AddElements(ElementList);

				// Update the font cache with new text after elements are batched
				FontCache->UpdateCache();

				// All elements for this window have been batched and rendering data updated
				ElementBatcher->ResetBatches();
			}
		}
	}
}

template<typename TKeepAliveType>
struct TKeepAliveCommand final : public FRHICommand < TKeepAliveCommand<TKeepAliveType> >
{
	TKeepAliveType Value;
	
	TKeepAliveCommand(TKeepAliveType InValue) : Value(InValue) {}

	void Execute(FRHICommandListBase& CmdList) {}
};

void FSlate3DRenderer::DrawWindowToTarget_RenderThread(FRHICommandListImmediate& InRHICmdList, const FRenderThreadUpdateContext& Context)
{
	check(IsInRenderingThread());
	QUICK_SCOPE_CYCLE_COUNTER(Stat_Slate_WidgetRendererRenderThread);
	SCOPED_DRAW_EVENT( InRHICmdList, SlateRenderToTarget );
	SCOPED_GPU_STAT(InRHICmdList, Slate3D);

	checkSlow( Context.RenderTargetResource );

	const TArray<TSharedRef<FSlateWindowElementList>>& WindowsToDraw = Context.WindowDrawBuffer->GetWindowElementLists();

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	RenderTargetPolicy->BeginDrawingWindows();

	FTextureRenderTarget2DResource* RenderTargetResource = static_cast<FTextureRenderTarget2DResource*>(Context.RenderTargetResource);
	// Set render target and clear.
	FTexture2DRHIRef RTTextureRHI = RenderTargetResource->GetTextureRHI();
	InRHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, RTTextureRHI);
	
	FRHIRenderPassInfo RPInfo(RTTextureRHI, ERenderTargetActions::Load_Store);
	if (Context.bClearTarget)
	{
		RPInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
	}
	InRHICmdList.BeginRenderPass(RPInfo, TEXT("Slate3D"));
	{

		for (int32 WindowIndex = 0; WindowIndex < WindowsToDraw.Num(); WindowIndex++)
		{
			FSlateWindowElementList& WindowElementList = *WindowsToDraw[WindowIndex];

			FSlateBatchData& BatchData = WindowElementList.GetBatchData();
			FElementBatchMap& RootBatchMap = WindowElementList.GetRootDrawLayer().GetElementBatchMap();

			WindowElementList.PreDraw_ParallelThread();

			BatchData.CreateRenderBatches(RootBatchMap);

			if (BatchData.GetRenderBatches().Num() > 0)
			{
				RenderTargetPolicy->UpdateVertexAndIndexBuffers(InRHICmdList, BatchData);

				FVector2D DrawOffset = Context.WindowDrawBuffer->ViewOffset;

				FMatrix ProjectionMatrix = FSlateRHIRenderer::CreateProjectionMatrix(RTTextureRHI->GetSizeX(), RTTextureRHI->GetSizeY());
				FMatrix ViewOffset = FTranslationMatrix::Make(FVector(DrawOffset, 0));
				ProjectionMatrix = ViewOffset * ProjectionMatrix;

				FSlateBackBuffer BackBufferTarget(RenderTargetResource->GetTextureRHI(), FIntPoint(RTTextureRHI->GetSizeX(), RTTextureRHI->GetSizeY()));

				FSlateRenderingParams DrawOptions(ProjectionMatrix, Context.WorldTimeSeconds, Context.DeltaTimeSeconds, Context.RealTimeSeconds);
				// The scene renderer will handle it in this case
				DrawOptions.bAllowSwitchVerticalAxis = false;
				DrawOptions.ViewOffset = DrawOffset;

				FTexture2DRHIRef ColorTarget = RenderTargetResource->GetTextureRHI();

				if (BatchData.IsStencilClippingRequired())
				{
					if (!DepthStencil.IsValid() || ColorTarget->GetSizeXY() != DepthStencil->GetSizeXY())
					{
						DepthStencil.SafeRelease();

						FTexture2DRHIRef ShaderResourceUnused;
						FRHIResourceCreateInfo CreateInfo(FClearValueBinding::DepthZero);
						RHICreateTargetableShaderResource2D(ColorTarget->GetSizeX(), ColorTarget->GetSizeY(), PF_DepthStencil, 1, TexCreate_None, TexCreate_DepthStencilTargetable, false, CreateInfo, DepthStencil, ShaderResourceUnused);
						check(IsValidRef(DepthStencil));
					}
				}

				RenderTargetPolicy->DrawElements(
					InRHICmdList,
					BackBufferTarget,
					ColorTarget,
					DepthStencil,
					BatchData.GetRenderBatches(),
					DrawOptions
				);
			}
		}
	}
	InRHICmdList.EndRenderPass();

	FSlateEndDrawingWindowsCommand::EndDrawingWindows(InRHICmdList, Context.WindowDrawBuffer, *RenderTargetPolicy);
	InRHICmdList.CopyToResolveTarget(RenderTargetResource->GetTextureRHI(), RTTextureRHI, FResolveParams());

	ISlate3DRendererPtr Self = SharedThis(this);

	ALLOC_COMMAND_CL(InRHICmdList, TKeepAliveCommand<ISlate3DRendererPtr>)(Self);
}
