// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaCapture.h"

#include "Engine/GameEngine.h"
#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "MediaIOCoreModule.h"
#include "MediaOutput.h"
#include "Misc/App.h"
#include "RendererInterface.h"
#include "RenderUtils.h"
#include "Slate/SceneViewport.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#endif //WITH_EDITOR

/* namespace MediaCaptureDetails definition
*****************************************************************************/

namespace MediaCaptureDetails
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport);
}

/* UMediaCapture
*****************************************************************************/

UMediaCapture::UMediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MediaState(EMediaCaptureState::Stopped)
	, CurrentResolvedTargetIndex(0)
	, NumberOfCaptureFrame(2)
	, DesiredSize(1280, 720)
	, DesiredPixelFormat(EPixelFormat::PF_A2B10G10R10)
	, bResolvedTargetInitialized(false)
	, bWaitingForResolveCommandExecution(false)
{
}

void UMediaCapture::BeginDestroy()
{
	StopCapture(false);

	Super::BeginDestroy();
}

FString UMediaCapture::GetDesc()
{
	if (MediaOutput)
	{
		return FString::Printf(TEXT("%s [%s]"), *Super::GetDesc(), *MediaOutput->GetDesc());
	}
	return FString::Printf(TEXT("%s [none]"), *Super::GetDesc());
}

bool UMediaCapture::CaptureActiveSceneViewport()
{
	StopCapture(false);
	check(IsInGameThread());

	TSharedPtr<FSceneViewport> FoundSceneViewport;
	if (!MediaCaptureDetails::FindSceneViewportAndLevel(FoundSceneViewport) || !FoundSceneViewport.IsValid())
	{
		UE_LOG(LogMediaIOCore, Warning, TEXT("Can not start the capture. No viewport could be found. Play in 'Standalone' or in 'New Editor Window PIE'."));
		return false;
	}

	return CaptureSceneViewport(FoundSceneViewport);
}

bool UMediaCapture::CaptureSceneViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	StopCapture(false);
	check(IsInGameThread());

	if (!InSceneViewport.IsValid())
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Scene Viewport is invalid."));
		return false;
	}

	if (!ValidateMediaOutput())
	{
		return false;
	}

	DesiredSize = MediaOutput->GetRequestedSize();
	DesiredPixelFormat = MediaOutput->GetRequestedPixelFormat();
	
	FIntPoint SceneViewportSize = InSceneViewport->GetRenderTargetTextureSizeXY();
	if (DesiredSize.X != SceneViewportSize.X || DesiredSize.Y != SceneViewportSize.Y)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Render Target size doesn't match with the requested size. SceneViewport: %d,%d  MediaOutput: %d,%d")
			, SceneViewportSize.X, SceneViewportSize.Y
			, DesiredSize.X, DesiredSize.Y);
		return false;
	}

	static const auto CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
	EPixelFormat SceneTargetFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread()));
	if (DesiredPixelFormat != SceneTargetFormat)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Render Target pixel format doesn't match with the requested pixel format. SceneViewport: %s MediaOutput: %s")
			, GetPixelFormatString(SceneTargetFormat)
			, GetPixelFormatString(DesiredPixelFormat));
		return false;
	}

	MediaState = EMediaCaptureState::Preparing;
	if (!CaptureSceneViewportImpl(InSceneViewport))
	{
		MediaState = EMediaCaptureState::Stopped;
		return false;
	}

	CapturingSceneViewport = InSceneViewport;
	InitializeResolveTarget(MediaOutput->NumberOfTextureBuffers);
	CurrentResolvedTargetIndex = 0;
	FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);

	return true;
}

bool UMediaCapture::CaptureTextureRenderTarget2D(UTextureRenderTarget2D* InRenderTarget2D)
{
	StopCapture(false);

	check(IsInGameThread());
	if (InRenderTarget2D == nullptr)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Couldn't start the capture. The Render Target is invalid."));
		return false;
	}

	if (!ValidateMediaOutput())
	{
		return false;
	}

	DesiredSize = MediaOutput->GetRequestedSize();
	DesiredPixelFormat = MediaOutput->GetRequestedPixelFormat();

	if (DesiredSize.X != InRenderTarget2D->SizeX || DesiredSize.Y != InRenderTarget2D->SizeY)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Render Target size doesn't match with the requested size. RenderTarget: %d,%d  MediaOutput: %d,%d")
			, InRenderTarget2D->SizeX, InRenderTarget2D->SizeY
			, DesiredSize.X, DesiredSize.Y);
		return false;
	}

	if (DesiredPixelFormat != InRenderTarget2D->GetFormat())
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Render Target pixel format doesn't match with the requested pixel format. RenderTarget: %s MediaOutput: %s")
			, GetPixelFormatString(InRenderTarget2D->GetFormat())
			, GetPixelFormatString(DesiredPixelFormat));
		return false;
	}

	if (!CaptureRenderTargetImpl(InRenderTarget2D))
	{
		return false;
	}

	CapturingRenderTarget = InRenderTarget2D;
	InitializeResolveTarget(MediaOutput->NumberOfTextureBuffers);
	CurrentResolvedTargetIndex = 0;
	FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);
	MediaState = EMediaCaptureState::Preparing;

	return true;
}

void UMediaCapture::StopCapture(bool bAllowPendingFrameToBeProcess)
{
	check(IsInGameThread());
	if (bAllowPendingFrameToBeProcess)
	{
		if (MediaState != EMediaCaptureState::Stopped && MediaState != EMediaCaptureState::StopRequested)
		{
			MediaState = EMediaCaptureState::StopRequested;
		}
	}
	else
	{
		if (MediaState != EMediaCaptureState::Stopped)
		{
			MediaState = EMediaCaptureState::Stopped;

			FCoreDelegates::OnEndFrame.RemoveAll(this);

			if (bWaitingForResolveCommandExecution || !bResolvedTargetInitialized)
			{
				FlushRenderingCommands();
			}
			StopCaptureImpl(bAllowPendingFrameToBeProcess);

			CapturingRenderTarget = nullptr;
			CapturingSceneViewport.Reset();
			CaptureFrames.Reset();
			DesiredSize = FIntPoint(1280, 720);
			DesiredPixelFormat = EPixelFormat::PF_A2B10G10R10;
		}
	}
}

void UMediaCapture::SetMediaOutput(UMediaOutput* InMediaOutput)
{
	if (GetState() == EMediaCaptureState::Stopped)
	{
		MediaOutput = InMediaOutput;
	}
}

bool UMediaCapture::HasFinishedProcessing() const
{
	return bWaitingForResolveCommandExecution == false
		|| MediaState == EMediaCaptureState::Error
		|| MediaState == EMediaCaptureState::Stopped;
}

void UMediaCapture::InitializeResolveTarget(int32 InNumberOfBuffers)
{
	NumberOfCaptureFrame = InNumberOfBuffers;
	check(CaptureFrames.Num() == 0);
	CaptureFrames.AddDefaulted(InNumberOfBuffers);

	auto RenderCommand = [this](FRHICommandListImmediate& RHICmdList)
	{
		FRHIResourceCreateInfo CreateInfo;
		for (int32 Index = 0; Index < NumberOfCaptureFrame; ++Index)
		{
			CaptureFrames[Index].ReadbackTexture = RHICreateTexture2D(
				DesiredSize.X,
				DesiredSize.Y,
				DesiredPixelFormat,
				1,
				1,
				TexCreate_CPUReadback,
				CreateInfo
				);
		}
		bResolvedTargetInitialized = true;
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		MediaOutputCaptureFrameCreateTexture,
		decltype(RenderCommand), InRenderCommand, RenderCommand,
		{
			InRenderCommand(RHICmdList);
		});
}

bool UMediaCapture::ValidateMediaOutput() const
{
	if (MediaOutput == nullptr)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Media Output is invalid."));
		return false;
	}

	FString FailureReason;
	if (!MediaOutput->Validate(FailureReason))
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. %s."), *FailureReason);
		return false;
	}

	return true;
}

void UMediaCapture::OnEndFrame_GameThread()
{
	if (!bResolvedTargetInitialized)
	{
		FlushRenderingCommands();
	}

	if (!MediaOutput)
	{
		return;
	}

	if (GetState() == EMediaCaptureState::Error)
	{
		StopCapture(false);
	}

	if (GetState() != EMediaCaptureState::Capturing && GetState() != EMediaCaptureState::StopRequested)
	{
		return;
	}

	CurrentResolvedTargetIndex = (CurrentResolvedTargetIndex+1) % NumberOfCaptureFrame;
	int32 ReadyFrameIndex = (CurrentResolvedTargetIndex+1) % NumberOfCaptureFrame; // Next one in the buffer queue

	FCaptureFrame* ReadyFrame = (CaptureFrames[ReadyFrameIndex].bResolvedTargetRequested) ? &CaptureFrames[ReadyFrameIndex] : nullptr;
	FCaptureFrame* CapturingFrame = (GetState() != EMediaCaptureState::StopRequested) ? &CaptureFrames[CurrentResolvedTargetIndex] : nullptr;

	if (ReadyFrame == nullptr && GetState() == EMediaCaptureState::StopRequested)
	{
		// All the requested frames have been captured.
		StopCapture(false);
		return;
	}

	if (CapturingFrame)
	{
		CapturingFrame->CaptureBaseData.SourceFrameTimecode = FApp::GetTimecode();
		CapturingFrame->CaptureBaseData.SourceFrameNumberRenderThread = GFrameNumberRenderThread;
		CapturingFrame->UserData = GetCaptureFrameUserData_GameThread();
	}

	bWaitingForResolveCommandExecution = true;

	// RenderCommand to be executed on the RenderThread
	auto RenderCommand = [this](FRHICommandListImmediate& RHICmdList, FCaptureFrame* InCapturingFrame, FCaptureFrame* InReadyFrame)
	{
		FTexture2DRHIRef SourceTexture;
		{
			UTextureRenderTarget2D* InCapturingRenderTarget = nullptr;
			FPlatformAtomics::InterlockedExchangePtr((void**)(&InCapturingRenderTarget), CapturingRenderTarget);
			TSharedPtr<FSceneViewport> InSceneViewportPtr = CapturingSceneViewport.Pin();
			if (InSceneViewportPtr.IsValid())
			{
				SourceTexture = InSceneViewportPtr->GetRenderTargetTexture();
				if (!SourceTexture.IsValid() && InSceneViewportPtr->GetViewportRHI())
				{
					SourceTexture = RHICmdList.GetViewportBackBuffer(InSceneViewportPtr->GetViewportRHI());
				}
			}
			else if (InCapturingRenderTarget)
			{
				if (InCapturingRenderTarget->GetRenderTargetResource() != nullptr && InCapturingRenderTarget->GetRenderTargetResource()->GetTextureRenderTarget2DResource() != nullptr)
				{
					SourceTexture = InCapturingRenderTarget->GetRenderTargetResource()->GetTextureRenderTarget2DResource()->GetTextureRHI();
				}
			}
		}

		if (!SourceTexture.IsValid())
		{
			MediaState = EMediaCaptureState::Error;
			UMediaOutput* InMediaOutput = nullptr;
			FPlatformAtomics::InterlockedExchangePtr((void**)(&InMediaOutput), MediaOutput);
			UE_LOG(LogMediaIOCore, Error, TEXT("Can't grab the Texture to capture for '%s'."), InMediaOutput ? *InMediaOutput->GetName() : TEXT("[undefined]"));
		}
		else if (InCapturingFrame)
		{
			if (InCapturingFrame->ReadbackTexture->GetSizeX() != SourceTexture->GetSizeX()
				|| InCapturingFrame->ReadbackTexture->GetSizeY() != SourceTexture->GetSizeY())
			{
				MediaState = EMediaCaptureState::Error;
				UMediaOutput* InMediaOutput = nullptr;
				FPlatformAtomics::InterlockedExchangePtr((void**)(&InMediaOutput), MediaOutput);
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
					, InMediaOutput ? *InMediaOutput->GetName() : TEXT("[undefined]")
					, InCapturingFrame->ReadbackTexture->GetSizeX(), InCapturingFrame->ReadbackTexture->GetSizeY()
					, SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
			}
			else if (InCapturingFrame->ReadbackTexture->GetFormat() != SourceTexture->GetFormat())
			{
				MediaState = EMediaCaptureState::Error;
				UMediaOutput* InMediaOutput = nullptr;
				FPlatformAtomics::InterlockedExchangePtr((void**)(&InMediaOutput), MediaOutput);
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source pixel format doesn't match with the user requested pixel format. Requested: %s Source: %s")
					, InMediaOutput ? *InMediaOutput->GetName() : TEXT("[undefined]")
					, GetPixelFormatString(InCapturingFrame->ReadbackTexture->GetFormat())
					, GetPixelFormatString(SourceTexture->GetFormat()));
			}
		}

		if (InCapturingFrame && MediaState != EMediaCaptureState::Error)
		{
			FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
				FIntPoint(SourceTexture->GetSizeX(), SourceTexture->GetSizeY()),
				SourceTexture->GetFormat(),
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_RenderTargetable,
				false);
			TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
			GetRendererModule().RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("MediaCapture"));
			const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

			// Asynchronously copy target from GPU to GPU
			RHICmdList.CopyToResolveTarget(SourceTexture, DestRenderTarget.TargetableTexture, FResolveParams());

			// Asynchronously copy duplicate target from GPU to System Memory
			RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, InCapturingFrame->ReadbackTexture, FResolveParams());

			InCapturingFrame->bResolvedTargetRequested = true;
		}

		if (InReadyFrame && MediaState != EMediaCaptureState::Error)
		{
			check(InReadyFrame->ReadbackTexture.IsValid());

			// Lock & read
			void* ColorDataBuffer = nullptr;
			int32 Width = 0, Height = 0;
			RHICmdList.MapStagingSurface(InReadyFrame->ReadbackTexture, ColorDataBuffer, Width, Height);

			OnFrameCaptured_RenderingThread(InReadyFrame->CaptureBaseData, InReadyFrame->UserData, ColorDataBuffer, Width, Height);
			InReadyFrame->bResolvedTargetRequested = false;

			RHICmdList.UnmapStagingSurface(InReadyFrame->ReadbackTexture);
		}

		bWaitingForResolveCommandExecution = false;
	};


	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		MediaOutputCaptureFrameCreateTexture,
		FCaptureFrame*, InCapturingFrame, CapturingFrame,
		FCaptureFrame*, InPreviousFrame, ReadyFrame,
		decltype(RenderCommand), InRenderCommand, RenderCommand,
		{
			InRenderCommand(RHICmdList, InCapturingFrame, InPreviousFrame);
		});
}

/* namespace MediaCaptureDetails implementation
*****************************************************************************/
namespace MediaCaptureDetails
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE)
				{
					UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
					FSlatePlayInEditorInfo& Info = EditorEngine->SlatePlayInEditorMap.FindChecked(Context.ContextHandle);
					if (Info.SlatePlayInEditorWindowViewport.IsValid())
					{
						OutSceneViewport = Info.SlatePlayInEditorWindowViewport;
						return true;
					}
				}
			}
			return false;
		}
		else
#endif
		{
			UGameEngine* GameEngine = CastChecked<UGameEngine>(GEngine);
			OutSceneViewport = GameEngine->SceneViewport;
			return true;
		}
	}
}
