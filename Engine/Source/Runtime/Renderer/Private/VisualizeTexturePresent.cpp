// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VisualizeTexture.cpp: Post processing visualize texture.
=============================================================================*/

#include "VisualizeTexturePresent.h"
#include "VisualizeTexture.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "UnrealEngine.h"
#include "RenderTargetPool.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "RenderTargetTemp.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"


/** Encapsulates a simple copy pixel shader. */
class FVisualizeTexturePresentPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeTexturePresentPS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeTexturePresentPS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, VisualizeTexture2D)
		SHADER_PARAMETER_SAMPLER(SamplerState, VisualizeTexture2DSampler)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeTexturePresentPS, "/Engine/Private/Tools/VisualizeTexture.usf", "PresentPS", SF_Pixel);


// draw a single pixel sized rectangle using 4 sub elements
static void DrawBorder(FCanvas& Canvas, const FIntRect Rect, FLinearColor Color)
{
	// top
	Canvas.DrawTile(Rect.Min.X, Rect.Min.Y, Rect.Max.X - Rect.Min.X, 1, 0, 0, 1, 1, Color);
	// bottom
	Canvas.DrawTile(Rect.Min.X, Rect.Max.Y - 1, Rect.Max.X - Rect.Min.X, 1, 0, 0, 1, 1, Color);
	// left
	Canvas.DrawTile(Rect.Min.X, Rect.Min.Y + 1, 1, Rect.Max.Y - Rect.Min.Y - 2, 0, 0, 1, 1, Color);
	// right
	Canvas.DrawTile(Rect.Max.X - 1, Rect.Min.Y + 1, 1, Rect.Max.Y - Rect.Min.Y - 2, 0, 0, 1, 1, Color);
}

// helper class to get a consistent layout in multiple functions
// MaxX and Y are the output value that can be requested during or after iteration
// Examples usages:
//    FRenderTargetPoolEventIterator It(RenderTargetPoolEvents, OptionalStartIndex);
//    while(FRenderTargetPoolEvent* Event = It.Iterate()) {}
struct FRenderTargetPoolEventIterator
{
	int32 Index;
	TArray<FRenderTargetPoolEvent>& RenderTargetPoolEvents;
	bool bLineContent;
	uint32 TotalWidth;
	int32 Y;

	// constructor
	FRenderTargetPoolEventIterator(TArray<FRenderTargetPoolEvent>& InRenderTargetPoolEvents, int32 InIndex = 0)
		: Index(InIndex)
		, RenderTargetPoolEvents(InRenderTargetPoolEvents)
		, bLineContent(false)
		, TotalWidth(1)
		, Y(0)
	{
		Touch();
	}

	FRenderTargetPoolEvent* operator*()
	{
		if (Index < RenderTargetPoolEvents.Num())
		{
			return &RenderTargetPoolEvents[Index];
		}

		return 0;
	}

	// @return 0 if end was reached
	FRenderTargetPoolEventIterator& operator++()
	{
		if (Index < RenderTargetPoolEvents.Num())
		{
			++Index;
		}

		Touch();

		return *this;
	}

	int32 FindClosingEventY() const
	{
		FRenderTargetPoolEventIterator It = *this;

		const ERenderTargetPoolEventType StartType = (*It)->GetEventType();

		if (StartType == ERTPE_Alloc)
		{
			int32 PoolEntryId = RenderTargetPoolEvents[Index].GetPoolEntryId();

			++It;

			// search for next Dealloc of the same PoolEntryId
			for (; *It; ++It)
			{
				FRenderTargetPoolEvent* Event = *It;

				if (Event->GetEventType() == ERTPE_Dealloc && Event->GetPoolEntryId() == PoolEntryId)
				{
					break;
				}
			}
		}
		else if (StartType == ERTPE_Phase)
		{
			++It;

			// search for next Phase
			for (; *It; ++It)
			{
				FRenderTargetPoolEvent* Event = *It;

				if (Event->GetEventType() == ERTPE_Phase)
				{
					break;
				}
			}
		}
		else
		{
			check(0);
		}

		return It.Y;
	}

private:

	void Touch()
	{
		if (Index < RenderTargetPoolEvents.Num())
		{
			const FRenderTargetPoolEvent& Event = RenderTargetPoolEvents[Index];

			const ERenderTargetPoolEventType Type = Event.GetEventType();

			if (Type == ERTPE_Alloc)
			{
				// for now they are all equal width
				TotalWidth = FMath::Max(TotalWidth, Event.GetColumnX() + Event.GetColumnSize());
			}
			Y = Event.GetTimeStep();
		}
	}
};

// static
uint32 FVisualizeTexturePresent::ComputeEventDisplayHeight()
{
	FRenderTargetPoolEventIterator It(GRenderTargetPool.RenderTargetPoolEvents);

	for (; *It; ++It)
	{
	}

	return It.Y;
}

// static
void FVisualizeTexturePresent::OnStartRender(const FViewInfo& View)
{
	GVisualizeTexture.FeatureLevel = View.GetFeatureLevel();
	GVisualizeTexture.bEnabled = true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// VisualizeTexture observed render target is set each frame
	GVisualizeTexture.VisualizeTextureContent.SafeRelease();
	GVisualizeTexture.VisualizeTextureDesc = FPooledRenderTargetDesc();
	GVisualizeTexture.VisualizeTextureDesc.DebugName = TEXT("VisualizeTexture");

	GVisualizeTexture.ObservedDebugNameReusedCurrent = 0;

	// only needed for VisualizeTexture (todo: optimize out when possible)
	{
		for (TMap<const TCHAR*, uint32>::TIterator It(GVisualizeTexture.VisualizeTextureCheckpoints); It; ++It)
		{
			uint32& Value = It.Value();

			// 0 as it was not used this frame yet
			Value = 0;
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

// static
void FVisualizeTexturePresent::PresentContent(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (GRenderTargetPool.RenderTargetPoolEvents.Num())
	{
		GRenderTargetPool.AddPhaseEvent(TEXT("FrameEnd"));

		FIntPoint DisplayLeftTop(20, 50);
		// on the right we leave more space to make the mouse tooltip readable
		FIntPoint DisplayExtent(View.ViewRect.Width() - DisplayLeftTop.X * 2 - 140, View.ViewRect.Height() - DisplayLeftTop.Y * 2);

		// if the area is not too small
		if (DisplayExtent.X > 50 && DisplayExtent.Y > 50)
		{
			FRenderTargetPool::SMemoryStats MemoryStats = GRenderTargetPool.ComputeView();

			FRHIRenderPassInfo RPInfos;
			RPInfos.ColorRenderTargets[0].RenderTarget = View.Family->RenderTarget->GetRenderTargetTexture();
			RPInfos.ColorRenderTargets[0].ResolveTarget = View.Family->RenderTarget->GetRenderTargetTexture();
			RPInfos.ColorRenderTargets[0].Action = ERenderTargetActions::Load_Store;
			RHICmdList.BeginRenderPass(RPInfos, TEXT("PresentVisualizeTexture"));
			RHICmdList.SetViewport(0, 0, 0.0f, FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY().X, FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY().Y, 1.0f);


			FRenderTargetTemp TempRenderTarget(View, View.UnconstrainedViewRect.Size());
			FCanvas Canvas(&TempRenderTarget, NULL, View.Family->CurrentRealTime, View.Family->CurrentWorldTime, View.Family->DeltaWorldTime, View.GetFeatureLevel());

			// TinyFont property
			const int32 FontHeight = 12;

			FIntPoint MousePos = View.CursorPos;

			FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);
			FLinearColor PhaseColor = FLinearColor(0.2f, 0.1f, 0.05f, 0.8f);
			FLinearColor ElementColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.9f);
			FLinearColor ElementColorVRam = FLinearColor(0.4f, 0.25f, 0.25f, 0.9f);

			UTexture2D* GradientTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->GradientTexture0;

			// background rectangle
			Canvas.DrawTile(DisplayLeftTop.X, DisplayLeftTop.Y - 1 * FontHeight - 1, DisplayExtent.X, DisplayExtent.Y + FontHeight, 0, 0, 1, 1, BackgroundColor);

			{
				uint32 MB = 1024 * 1024;
				uint32 MBm1 = MB - 1;

				FString Headline = *FString::Printf(TEXT("RenderTargetPool elements(x) over time(y) >= %dKB, Displayed/Total:%d/%dMB"),
					GRenderTargetPool.EventRecordingSizeThreshold,
					(uint32)((MemoryStats.DisplayedUsageInBytes + MBm1) / MB),
					(uint32)((MemoryStats.TotalUsageInBytes + MBm1) / MB));
				Canvas.DrawShadowedString(DisplayLeftTop.X, DisplayLeftTop.Y - 1 * FontHeight - 1, *Headline, GEngine->GetTinyFont(), FLinearColor(1, 1, 1));
			}

			uint32 EventDisplayHeight = FVisualizeTexturePresent::ComputeEventDisplayHeight();

			float ScaleX = DisplayExtent.X / (float)MemoryStats.TotalColumnSize;
			float ScaleY = DisplayExtent.Y / (float)EventDisplayHeight;

			// 0 if none
			FRenderTargetPoolEvent* HighlightedEvent = 0;
			FIntRect HighlightedRect;

			// Phase events
			for (FRenderTargetPoolEventIterator It(GRenderTargetPool.RenderTargetPoolEvents); *It; ++It)
			{
				FRenderTargetPoolEvent* Event = *It;

				if (Event->GetEventType() == ERTPE_Phase)
				{
					int32 Y0 = It.Y;
					int32 Y1 = It.FindClosingEventY();

					FIntPoint PixelLeftTop((int32)(DisplayLeftTop.X), (int32)(DisplayLeftTop.Y + ScaleY * Y0));
					FIntPoint PixelRightBottom((int32)(DisplayLeftTop.X + DisplayExtent.X), (int32)(DisplayLeftTop.Y + ScaleY * Y1));

					bool bHighlight = MousePos.X >= PixelLeftTop.X && MousePos.X < PixelRightBottom.X && MousePos.Y >= PixelLeftTop.Y && MousePos.Y <= PixelRightBottom.Y;

					if (bHighlight)
					{
						HighlightedEvent = Event;
						HighlightedRect = FIntRect(PixelLeftTop, PixelRightBottom);
					}

					// UMax is 0.9f to avoid getting some wrap texture leaking in at the bottom
					Canvas.DrawTile(PixelLeftTop.X, PixelLeftTop.Y, PixelRightBottom.X - PixelLeftTop.X, PixelRightBottom.Y - PixelLeftTop.Y, 0, 0, 1, 0.9f, PhaseColor, GradientTexture->Resource);
				}
			}

			// Alloc / Dealloc events
			for (FRenderTargetPoolEventIterator It(GRenderTargetPool.RenderTargetPoolEvents); *It; ++It)
			{
				FRenderTargetPoolEvent* Event = *It;

				if (Event->GetEventType() == ERTPE_Alloc && Event->GetColumnSize())
				{
					int32 Y0 = It.Y;
					int32 Y1 = It.FindClosingEventY();

					int32 X0 = Event->GetColumnX();
					// for now they are all equal width
					int32 X1 = X0 + Event->GetColumnSize();

					FIntPoint PixelLeftTop((int32)(DisplayLeftTop.X + ScaleX * X0), (int32)(DisplayLeftTop.Y + ScaleY * Y0));
					FIntPoint PixelRightBottom((int32)(DisplayLeftTop.X + ScaleX * X1), (int32)(DisplayLeftTop.Y + ScaleY * Y1));

					bool bHighlight = MousePos.X >= PixelLeftTop.X && MousePos.X < PixelRightBottom.X && MousePos.Y >= PixelLeftTop.Y && MousePos.Y <= PixelRightBottom.Y;

					if (bHighlight)
					{
						HighlightedEvent = Event;
						HighlightedRect = FIntRect(PixelLeftTop, PixelRightBottom);
					}

					FLinearColor Color = ElementColor;

					// Highlight EDRAM/FastVRAM usage
					if (Event->GetDesc().Flags & TexCreate_FastVRAM)
					{
						Color = ElementColorVRam;
					}

					Canvas.DrawTile(
						PixelLeftTop.X, PixelLeftTop.Y,
						PixelRightBottom.X - PixelLeftTop.X - 1, PixelRightBottom.Y - PixelLeftTop.Y - 1,
						0, 0, 1, 1, Color);
				}
			}

			if (HighlightedEvent)
			{
				DrawBorder(Canvas, HighlightedRect, FLinearColor(0.8f, 0, 0, 0.5f));

				// Offset to not intersect with crosshair (in editor) or arrow (in game).
				FIntPoint Pos = MousePos + FIntPoint(12, 4);

				if (HighlightedEvent->GetEventType() == ERTPE_Phase)
				{
					FString PhaseText = *FString::Printf(TEXT("Phase: %s"), *HighlightedEvent->GetPhaseName());

					Canvas.DrawShadowedString(Pos.X, Pos.Y + 0 * FontHeight, *PhaseText, GEngine->GetTinyFont(), FLinearColor(0.5f, 0.5f, 1));
				}
				else
				{
					FString SizeString = FString::Printf(TEXT("%d KB"), (HighlightedEvent->GetSizeInBytes() + 1024) / 1024);

					Canvas.DrawShadowedString(Pos.X, Pos.Y + 0 * FontHeight, HighlightedEvent->GetDesc().DebugName, GEngine->GetTinyFont(), FLinearColor(1, 1, 0));
					Canvas.DrawShadowedString(Pos.X, Pos.Y + 1 * FontHeight, *HighlightedEvent->GetDesc().GenerateInfoString(), GEngine->GetTinyFont(), FLinearColor(1, 1, 0));
					Canvas.DrawShadowedString(Pos.X, Pos.Y + 2 * FontHeight, *SizeString, GEngine->GetTinyFont(), FLinearColor(1, 1, 0));
				}
			}

			Canvas.Flush_RenderThread(RHICmdList);

			GRenderTargetPool.CurrentEventRecordingTime = 0;
			GRenderTargetPool.RenderTargetPoolEvents.Empty();

			RHICmdList.EndRenderPass();
		}
	}

	if (GVisualizeTexture.Mode != 0)
	{
		// old mode is used, lets copy the specified texture to do it similar to the new system
		FPooledRenderTarget* Element = GRenderTargetPool.GetElementById(GVisualizeTexture.Mode - 1);
		if (Element)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> ElementRefCount;

			GVisualizeTexture.CreateContentCapturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(ElementRefCount));
			GraphBuilder.Execute();
		}
	}

	const FTexture2DRHIRef& RenderTargetTexture = View.Family->RenderTarget->GetRenderTargetTexture();

	if (!GVisualizeTexture.VisualizeTextureContent
		|| !IsValidRef(RenderTargetTexture)
		|| !GVisualizeTexture.bEnabled)
	{
		// visualize feature is deactivated
		return;
	}

	FPooledRenderTargetDesc Desc = GVisualizeTexture.VisualizeTextureDesc;

	FIntPoint SrcSize = Desc.Extent;
	FIntRect SrcRect(0, 0, SrcSize.X, SrcSize.Y);
	FIntRect DestRect = SrcRect;
	{

		// set ViewRect
		switch (GVisualizeTexture.UVInputMapping)
		{
		case 0:
		{
			DestRect = SrcRect;
		}
		break;

		case 1:
		{
			DestRect = SrcRect;
		}
		break;

		case 2:
		{
			FIntPoint Center = View.UnconstrainedViewRect.Size() / 2;
			FIntPoint HalfMin = SrcSize / 2;
			FIntPoint HalfMax = SrcSize - HalfMin;

			DestRect = FIntRect(Center - HalfMin, Center + HalfMax);
			break;
		}
		break;

		case 3:
		{
			float SrcAspectRatio = float(Desc.Extent.X) / float(Desc.Extent.Y);

			int32 TargetedHeight = 0.3f * View.UnconstrainedViewRect.Height();
			int32 TargetedWidth = SrcAspectRatio * TargetedHeight;
			int32 OffsetFromBorder = 100;

			DestRect.Min.X = View.UnconstrainedViewRect.Min.X + OffsetFromBorder;
			DestRect.Max.X = DestRect.Min.X + TargetedWidth;
			DestRect.Max.Y = View.UnconstrainedViewRect.Max.Y - OffsetFromBorder;
			DestRect.Min.Y = DestRect.Max.Y - TargetedHeight;
			break;
		}
		break;

		default:
			break;
		}
	}

	auto& RenderTarget = View.Family->RenderTarget->GetRenderTargetTexture();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetRenderTarget(RHICmdList, RenderTarget, FTextureRHIRef(), true);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	RHICmdList.SetViewport(DestRect.Min.X, DestRect.Min.Y, 0.0f, DestRect.Max.X, DestRect.Max.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	auto ShaderMap = View.ShaderMap;
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FVisualizeTexturePresentPS> PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
	{
		FVisualizeTexturePresentPS::FParameters Parameters;
		Parameters.VisualizeTexture2D = GVisualizeTexture.VisualizeTextureContent->GetRenderTargetItem().ShaderResourceTexture;
		Parameters.VisualizeTexture2DSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), Parameters);
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, VisCopyToMain);
		DrawRectangle(
			RHICmdList,
			0, 0,
			DestRect.Width(), DestRect.Height(),
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DestRect.Size(),
			SrcSize,
			*VertexShader,
			EDRF_Default);
	}

	FRenderTargetTemp TempRenderTarget(View, View.UnconstrainedViewRect.Size());
	FCanvas Canvas(&TempRenderTarget, NULL, View.Family->CurrentRealTime, View.Family->CurrentWorldTime, View.Family->DeltaWorldTime, View.GetFeatureLevel());

	float X = 100 + View.UnconstrainedViewRect.Min.X;
	float Y = 160 + View.UnconstrainedViewRect.Min.Y;
	float YStep = 14;

	{
		uint32 ReuseCount = GVisualizeTexture.ObservedDebugNameReusedCurrent;

		FString ExtendedName;
		if (ReuseCount)
		{
			uint32 ReuseGoal = FMath::Min(ReuseCount - 1, GVisualizeTexture.ObservedDebugNameReusedGoal);

			// was reused this frame
			ExtendedName = FString::Printf(TEXT("%s@%d @0..%d"), Desc.DebugName, ReuseGoal, ReuseCount - 1);
		}
		else
		{
			// was not reused this frame but can be referenced
			ExtendedName = FString::Printf(TEXT("%s"), Desc.DebugName);
		}

		FString Channels = TEXT("RGB");
		switch (GVisualizeTexture.SingleChannel)
		{
		case 0: Channels = TEXT("R"); break;
		case 1: Channels = TEXT("G"); break;
		case 2: Channels = TEXT("B"); break;
		case 3: Channels = TEXT("A"); break;
		}
		float Multiplier = (GVisualizeTexture.SingleChannel == -1) ? GVisualizeTexture.RGBMul : GVisualizeTexture.SingleChannelMul;

		FString Line = FString::Printf(TEXT("VisualizeTexture: %d \"%s\" %s*%g UV%d"),
			GVisualizeTexture.Mode,
			*ExtendedName,
			*Channels,
			Multiplier,
			GVisualizeTexture.UVInputMapping);

		Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
	}
	{
		FString Line = FString::Printf(TEXT("   TextureInfoString(): %s"), *(Desc.GenerateInfoString()));
		Canvas.DrawShadowedString(X + 10, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
	}
	{
		FString Line = FString::Printf(TEXT("  BufferSize:(%d,%d)"), FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY().X, FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY().Y);
		Canvas.DrawShadowedString(X + 10, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
	}

	const FSceneViewFamily& ViewFamily = *View.Family;

	for (int32 ViewId = 0; ViewId < ViewFamily.Views.Num(); ++ViewId)
	{
		const FViewInfo* ViewIt = static_cast<const FViewInfo*>(ViewFamily.Views[ViewId]);
		FString Line = FString::Printf(TEXT("   View #%d: (%d,%d)-(%d,%d)"), ViewId + 1,
			ViewIt->UnscaledViewRect.Min.X, ViewIt->UnscaledViewRect.Min.Y, ViewIt->UnscaledViewRect.Max.X, ViewIt->UnscaledViewRect.Max.Y);
		Canvas.DrawShadowedString(X + 10, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
	}

	X += 40;

	if (Desc.Flags & TexCreate_CPUReadback)
	{
		Canvas.DrawShadowedString(X, Y += YStep, TEXT("Content cannot be visualized on the GPU (TexCreate_CPUReadback)"), GetStatsFont(), FLinearColor(1, 1, 0));
	}
	else
	{
		Canvas.DrawShadowedString(X, Y += YStep, TEXT("Blinking Red: <0"), GetStatsFont(), FLinearColor(1, 0, 0));
		Canvas.DrawShadowedString(X, Y += YStep, TEXT("Blinking Blue: NAN or Inf"), GetStatsFont(), FLinearColor(0, 0, 1));

		// add explicit legend for SceneDepth and ShadowDepth as the display coloring is an artificial choice. 
		const bool bDepthTexture = (Desc.TargetableFlags & TexCreate_DepthStencilTargetable) != 0;
		const bool bShadowDepth = (Desc.Format == PF_ShadowDepth);
		if (bShadowDepth)
		{
			Canvas.DrawShadowedString(X, Y += YStep, TEXT("Color Key: Linear with white near and teal distant"), GetStatsFont(), FLinearColor(54.f / 255.f, 117.f / 255.f, 136.f / 255.f));
		}
		else if (bDepthTexture)
		{
			Canvas.DrawShadowedString(X, Y += YStep, TEXT("Color Key: Nonlinear with white distant"), GetStatsFont(), FLinearColor(0.5, 0, 0));
		}
	}

	Canvas.Flush_RenderThread(RHICmdList);
}

struct FSortedLines
{
	FString Line;
	int32 SortIndex;
	uint32 PoolIndex;

	FORCEINLINE bool operator<(const FSortedLines &B) const
	{
		// first large ones
		if (SortIndex < B.SortIndex)
		{
			return true;
		}
		if (SortIndex > B.SortIndex)
		{
			return false;
		}

		return Line < B.Line;
	}
};

// static
void FVisualizeTexturePresent::DebugLog(bool bExtended)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		TArray<FSortedLines> SortedLines;

		for (uint32 i = 0, Num = GRenderTargetPool.GetElementCount(); i < Num; ++i)
		{
			FPooledRenderTarget* RT = GRenderTargetPool.GetElementById(i);

			if (!RT)
			{
				continue;
			}

			FPooledRenderTargetDesc Desc = RT->GetDesc();

			if (GVisualizeTexture.bFullList || (Desc.Flags & TexCreate_HideInVisualizeTexture) == 0)
			{
				uint32 SizeInKB = (RT->ComputeMemorySize() + 1023) / 1024;

				FString UnusedStr;

				if (RT->GetUnusedForNFrames() > 0)
				{
					if (!GVisualizeTexture.bFullList)
					{
						continue;
					}

					UnusedStr = FString::Printf(TEXT(" unused(%d)"), RT->GetUnusedForNFrames());
				}

				FSortedLines Element;

				Element.PoolIndex = i;

				// sort by index
				Element.SortIndex = i;

				FString InfoString = Desc.GenerateInfoString();
				if (GVisualizeTexture.SortOrder == -1)
				{
					// constant works well with the average name length
					const uint32 TotelSpacerSize = 36;
					uint32 SpaceCount = FMath::Max<int32>(0, TotelSpacerSize - InfoString.Len());

					for (uint32 Space = 0; Space < SpaceCount; ++Space)
					{
						InfoString.AppendChar((TCHAR)' ');
					}

					// sort by index
					Element.Line = FString::Printf(TEXT("%s %s %d KB%s"), *InfoString, Desc.DebugName, SizeInKB, *UnusedStr);
				}
				else if (GVisualizeTexture.SortOrder == 0)
				{
					// sort by name
					Element.Line = FString::Printf(TEXT("%s %s %d KB%s"), Desc.DebugName, *InfoString, SizeInKB, *UnusedStr);
					Element.SortIndex = 0;
				}
				else if (GVisualizeTexture.SortOrder == 1)
				{
					// sort by size (large ones first)
					Element.Line = FString::Printf(TEXT("%d KB %s %s%s"), SizeInKB, *InfoString, Desc.DebugName, *UnusedStr);
					Element.SortIndex = -(int32)SizeInKB;
				}
				else
				{
					check(0);
				}

				if (Desc.Flags & TexCreate_FastVRAM)
				{
					FRHIResourceInfo Info;

					FTextureRHIRef Texture = RT->GetRenderTargetItem().ShaderResourceTexture;

					if (!IsValidRef(Texture))
					{
						Texture = RT->GetRenderTargetItem().TargetableTexture;
					}

					if (IsValidRef(Texture))
					{
						RHIGetResourceInfo(Texture, Info);
					}

					if (Info.VRamAllocation.AllocationSize)
					{
						// note we do KB for more readable numbers but this can cause quantization loss
						Element.Line += FString::Printf(TEXT(" VRamInKB(Start/Size):%d/%d"),
							Info.VRamAllocation.AllocationStart / 1024,
							(Info.VRamAllocation.AllocationSize + 1023) / 1024);
					}
					else
					{
						Element.Line += TEXT(" VRamInKB(Start/Size):<NONE>");
					}
				}

				SortedLines.Add(Element);
			}
		}

		SortedLines.Sort();

		{
			for (int32 Index = 0; Index < SortedLines.Num(); Index++)
			{
				const FSortedLines& Entry = SortedLines[Index];

				UE_LOG(LogConsoleResponse, Log, TEXT("   %3d = %s"), Entry.PoolIndex + 1, *Entry.Line);
			}
		}

		// clean flags for next use
		GVisualizeTexture.bFullList = false;
		GVisualizeTexture.SortOrder = -1;
	}

	UE_LOG(LogConsoleResponse, Log, TEXT(""));

	// log names (alternative method to look at the rendertargets)
	if (bExtended)
	{
		UE_LOG(LogConsoleResponse, Log, TEXT("CheckpointName (what was rendered this frame, use <Name>@<Number> to get intermediate versions):"));

		TArray<FString> Entries;

		// sorted by pointer for efficiency, now we want to print sorted alphabetically
		for (TMap<const TCHAR*, uint32>::TIterator It(GVisualizeTexture.VisualizeTextureCheckpoints); It; ++It)
		{
			const TCHAR* Key = It.Key();
			uint32 Value = It.Value();

			/*					if(Value)
						{
							// was reused this frame
							Entries.Add(FString::Printf(TEXT("%s @0..%d"), *Key.GetPlainNameString(), Value - 1));
						}
						else
			*/ {
			// was not reused this frame but can be referenced
				Entries.Add(Key);
			}
		}

		Entries.Sort();

		// that number works well with the name length we have
		const uint32 ColumnCount = 5;
		const uint32 SpaceBetweenColumns = 1;
		uint32 ColumnHeight = FMath::DivideAndRoundUp((uint32)Entries.Num(), ColumnCount);

		// width of the column in characters, init with 0
		uint32 ColumnWidths[ColumnCount] = {};

		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			uint32 Column = Index / ColumnHeight;

			const FString& Entry = *Entries[Index];

			ColumnWidths[Column] = FMath::Max(ColumnWidths[Column], (uint32)Entry.Len());
		}

		// print them sorted, if possible multiple in a line
		{
			FString Line;

			for (int32 OutputIndex = 0; OutputIndex < Entries.Num(); ++OutputIndex)
			{
				// 0..ColumnCount-1
				uint32 Column = OutputIndex % ColumnCount;
				int32 Row = OutputIndex / ColumnCount;

				uint32 Index = Row + Column * ColumnHeight;

				bool bLineEnd = true;

				if (Index < (uint32)Entries.Num())
				{
					bLineEnd = (Column + 1 == ColumnCount);

					// for human readability we order them to be per column
					const FString& Entry = *Entries[Index];

					Line += Entry;

					int32 SpaceCount = ColumnWidths[Column] + SpaceBetweenColumns - Entry.Len();

					// otehrwise a fomer pass was producing bad data
					check(SpaceCount >= 0);

					for (int32 Space = 0; Space < SpaceCount; ++Space)
					{
						Line.AppendChar((TCHAR)' ');
					}
				}

				if (bLineEnd)
				{
					Line.TrimEndInline();
					UE_LOG(LogConsoleResponse, Log, TEXT("   %s"), *Line);
					Line.Empty();
				}
			}
		}
	}

	{
		uint32 WholeCount;
		uint32 WholePoolInKB;
		uint32 UsedInKB;

		GRenderTargetPool.GetStats(WholeCount, WholePoolInKB, UsedInKB);

		UE_LOG(LogConsoleResponse, Log, TEXT("Pool: %d/%d MB (referenced/allocated)"), (UsedInKB + 1023) / 1024, (WholePoolInKB + 1023) / 1024);
	}
#endif
}
