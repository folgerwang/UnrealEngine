// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaCapture.h"

#include "Async/Async.h"
#include "Engine/GameEngine.h"
#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "MediaIOCoreModule.h"
#include "MediaOutput.h"
#include "MediaShaders.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "PipelineStateCache.h"
#include "RendererInterface.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "Slate/SceneViewport.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "MediaCapture"

/* namespace MediaCaptureDetails definition
*****************************************************************************/

/** Time spent in media capture sending a frame. */
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread CopyToResolve"), STAT_MediaCapture_RenderThread_CopyToResolve, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread MapStaging"), STAT_MediaCapture_RenderThread_MapStaging, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread Callback"), STAT_MediaCapture_RenderThread_Callback, STATGROUP_Media);

namespace MediaCaptureDetails
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport);

	//Validation for the source of a capture
	bool ValidateSceneViewport(const TSharedPtr<FSceneViewport>& SceneViewport, const FMediaCaptureOptions& CaptureOption, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing);
	bool ValidateTextureRenderTarget2D(const UTextureRenderTarget2D* RenderTarget, const FMediaCaptureOptions& CaptureOption, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing);

	//Validation that there is a capture 
	bool ValidateIsCapturing(const UMediaCapture& CaptureToBeValidated);

	void ShowSlateNotification();
}


/* UMediaCapture::FCaptureBaseData
*****************************************************************************/
UMediaCapture::FCaptureBaseData::FCaptureBaseData()
	: SourceFrameNumberRenderThread(0)
{

}

/* UMediaCapture::FCaptureFrame
*****************************************************************************/
UMediaCapture::FCaptureFrame::FCaptureFrame()
	: bResolvedTargetRequested(false)
{

}

/* FMediaCaptureOptions
*****************************************************************************/
FMediaCaptureOptions::FMediaCaptureOptions()
	: Crop(EMediaCaptureCroppingType::None)
	, CustomCapturePoint(FIntPoint::ZeroValue)
	, bResizeSourceBuffer(false)
{

}


/* UMediaCapture
*****************************************************************************/

UMediaCapture::UMediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentResolvedTargetIndex(0)
	, NumberOfCaptureFrame(2)
	, MediaState(EMediaCaptureState::Stopped)
	, DesiredSize(1280, 720)
	, DesiredPixelFormat(EPixelFormat::PF_A2B10G10R10)
	, DesiredOutputSize(1280, 720)
	, DesiredOutputPixelFormat(EPixelFormat::PF_A2B10G10R10)
	, ConversionOperation(EMediaCaptureConversionOperation::NONE)
	, MediaOutputName(TEXT("[undefined]"))
	, bUseRequestedTargetSize(false)
	, bResolvedTargetInitialized(false)
	, bShouldCaptureRHITexture(false)
	, bViewportHasFixedViewportSize(false)
	, WaitingForResolveCommandExecutionCounter(0)
{
}

void UMediaCapture::BeginDestroy()
{
	if (GetState() == EMediaCaptureState::Capturing || GetState() == EMediaCaptureState::Preparing)
	{
		UE_LOG(LogMediaIOCore, Warning, TEXT("%s will be destroyed and the capture was not stopped."), *GetName());
	}
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

bool UMediaCapture::CaptureActiveSceneViewport(FMediaCaptureOptions CaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	TSharedPtr<FSceneViewport> FoundSceneViewport;
	if (!MediaCaptureDetails::FindSceneViewportAndLevel(FoundSceneViewport) || !FoundSceneViewport.IsValid())
	{
		UE_LOG(LogMediaIOCore, Warning, TEXT("Can not start the capture. No viewport could be found. Play in 'Standalone' or in 'New Editor Window PIE'."));
		return false;
	}

	return CaptureSceneViewport(FoundSceneViewport, CaptureOptions);
}

bool UMediaCapture::CaptureSceneViewport(TSharedPtr<FSceneViewport>& InSceneViewport, FMediaCaptureOptions InCaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	if (!ValidateMediaOutput())
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	DesiredCaptureOptions = InCaptureOptions;
	CacheMediaOutput(EMediaCaptureSourceType::SCENE_VIEWPORT);

	if (bUseRequestedTargetSize)
	{
		DesiredSize = InSceneViewport->GetSize();
	}
	else if (DesiredCaptureOptions.bResizeSourceBuffer)
	{
		SetFixedViewportSize(InSceneViewport);
	}

	CacheOutputOptions();

	const bool bCurrentlyCapturing = false;
	if (!MediaCaptureDetails::ValidateSceneViewport(InSceneViewport, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	SetState(EMediaCaptureState::Preparing);
	if (!CaptureSceneViewportImpl(InSceneViewport))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		SetState(EMediaCaptureState::Stopped);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	//no lock required, the command on the render thread is not active
	CapturingSceneViewport = InSceneViewport;

	InitializeResolveTarget(MediaOutput->NumberOfTextureBuffers);
	CurrentResolvedTargetIndex = 0;
	FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);

	return true;
}

bool UMediaCapture::CaptureTextureRenderTarget2D(UTextureRenderTarget2D* InRenderTarget2D, FMediaCaptureOptions CaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	if (!ValidateMediaOutput())
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	DesiredCaptureOptions = CaptureOptions;
	CacheMediaOutput(EMediaCaptureSourceType::RENDER_TARGET);

	if (bUseRequestedTargetSize)
	{
		DesiredSize = FIntPoint(InRenderTarget2D->SizeX, InRenderTarget2D->SizeY);
	}
	else if (DesiredCaptureOptions.bResizeSourceBuffer)
	{
		InRenderTarget2D->ResizeTarget(DesiredSize.X, DesiredSize.Y);
	}

	CacheOutputOptions();

	const bool bCurrentlyCapturing = false;
	if (!MediaCaptureDetails::ValidateTextureRenderTarget2D(InRenderTarget2D, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	SetState(EMediaCaptureState::Preparing);
	if (!CaptureRenderTargetImpl(InRenderTarget2D))
	{
		SetState(EMediaCaptureState::Stopped);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	//no lock required the command on the render thread is not active yet
	CapturingRenderTarget = InRenderTarget2D;

	InitializeResolveTarget(MediaOutput->NumberOfTextureBuffers);
	CurrentResolvedTargetIndex = 0;
	FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);

	return true;
}

void UMediaCapture::CacheMediaOutput(EMediaCaptureSourceType InSourceType)
{
	check(MediaOutput);
	DesiredSize = MediaOutput->GetRequestedSize();
	DesiredPixelFormat = MediaOutput->GetRequestedPixelFormat();
	ConversionOperation = MediaOutput->GetConversionOperation(InSourceType);
}

void UMediaCapture::CacheOutputOptions()
{
	DesiredOutputSize = GetOutputSize(DesiredSize, ConversionOperation);
	DesiredOutputPixelFormat = GetOutputPixelFormat(DesiredPixelFormat, ConversionOperation);
	MediaOutputName = *MediaOutput->GetName();
	bShouldCaptureRHITexture = ShouldCaptureRHITexture();
}

FIntPoint UMediaCapture::GetOutputSize(const FIntPoint & InSize, const EMediaCaptureConversionOperation & InConversionOperation) const
{	
	switch (InConversionOperation)
	{
	case EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT:
		return FIntPoint(InSize.X / 2, InSize.Y);
	case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
		// Padding aligned on 48 (16 and 6 at the same time)
		return FIntPoint((((InSize.X + 47) / 48) * 48) / 6, InSize.Y);
	case EMediaCaptureConversionOperation::NONE:
	default:
		return InSize;
	}
}

EPixelFormat UMediaCapture::GetOutputPixelFormat(const EPixelFormat & InPixelFormat, const EMediaCaptureConversionOperation & InConversionOperation) const
{
	switch (InConversionOperation)
	{
	case EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT:
		return EPixelFormat::PF_B8G8R8A8;
	case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
		return EPixelFormat::PF_R32G32B32A32_UINT;
	case EMediaCaptureConversionOperation::NONE:
	default:
		return InPixelFormat;
	}
}

bool UMediaCapture::UpdateSceneViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	if (!MediaCaptureDetails::ValidateIsCapturing(*this))
	{
		StopCapture(false);
		return false;
	}

	check(IsInGameThread());

	if (!bUseRequestedTargetSize && DesiredCaptureOptions.bResizeSourceBuffer)
	{
		SetFixedViewportSize(InSceneViewport);
	}

	const bool bCurrentlyCapturing = true;
	if (!MediaCaptureDetails::ValidateSceneViewport(InSceneViewport, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	if (!UpdateSceneViewportImpl(InSceneViewport))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	{
		FScopeLock Lock(&AccessingCapturingSource);
		ResetFixedViewportSize(CapturingSceneViewport.Pin(), true);
		CapturingSceneViewport = InSceneViewport;
		CapturingRenderTarget = nullptr;
	}

	return true;
}

bool UMediaCapture::UpdateTextureRenderTarget2D(UTextureRenderTarget2D * InRenderTarget2D)
{
	if (!MediaCaptureDetails::ValidateIsCapturing(*this))
	{
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	check(IsInGameThread());

	if (!bUseRequestedTargetSize && DesiredCaptureOptions.bResizeSourceBuffer)
	{
		InRenderTarget2D->ResizeTarget(DesiredSize.X, DesiredSize.Y);
	}

	const bool bCurrentlyCapturing = true;
	if (!MediaCaptureDetails::ValidateTextureRenderTarget2D(InRenderTarget2D, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	if (!UpdateRenderTargetImpl(InRenderTarget2D))
	{
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	{
		FScopeLock Lock(&AccessingCapturingSource);
		ResetFixedViewportSize(CapturingSceneViewport.Pin(), true);
		CapturingRenderTarget = InRenderTarget2D;
		CapturingSceneViewport.Reset();
	}

	return true;
}

void UMediaCapture::StopCapture(bool bAllowPendingFrameToBeProcess)
{
	check(IsInGameThread());

	if (GetState() != EMediaCaptureState::StopRequested && GetState() != EMediaCaptureState::Capturing)
	{
		bAllowPendingFrameToBeProcess = false;
	}

	if (bAllowPendingFrameToBeProcess)
	{
		if (GetState() != EMediaCaptureState::Stopped && GetState() != EMediaCaptureState::StopRequested)
		{
			SetState(EMediaCaptureState::StopRequested);
		}
	}
	else
	{
		if (GetState() != EMediaCaptureState::Stopped)
		{
			SetState(EMediaCaptureState::Stopped);

			FCoreDelegates::OnEndFrame.RemoveAll(this);

			while (WaitingForResolveCommandExecutionCounter != 0 || !bResolvedTargetInitialized)
			{
				FlushRenderingCommands();
			}
			StopCaptureImpl(bAllowPendingFrameToBeProcess);
			ResetFixedViewportSize(CapturingSceneViewport.Pin(), false);

			CapturingRenderTarget = nullptr;
			CapturingSceneViewport.Reset();
			CaptureFrames.Reset();
			DesiredSize = FIntPoint(1280, 720);
			DesiredPixelFormat = EPixelFormat::PF_A2B10G10R10;
			DesiredOutputSize = FIntPoint(1280, 720);
			DesiredOutputPixelFormat = EPixelFormat::PF_A2B10G10R10;
			DesiredCaptureOptions = FMediaCaptureOptions();
			ConversionOperation = EMediaCaptureConversionOperation::NONE;
			MediaOutputName.Reset();
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

void UMediaCapture::SetState(EMediaCaptureState InNewState)
{
	if (MediaState != InNewState)
	{
		MediaState = InNewState;
		if (IsInGameThread())
		{
			BroadcastStateChanged();
		}
		else
		{
			TWeakObjectPtr<UMediaCapture> Self = this;
			AsyncTask(ENamedThreads::GameThread, [Self]
			{
				UMediaCapture* MediaCapture = Self.Get();
				if (UObjectInitialized() && MediaCapture)
				{
					MediaCapture->BroadcastStateChanged();
				}
			});
		}
	}
}

void UMediaCapture::BroadcastStateChanged()
{
	OnStateChanged.Broadcast();
	OnStateChangedNative.Broadcast();
}

void UMediaCapture::SetFixedViewportSize(TSharedPtr<FSceneViewport> InSceneViewport)
{
	InSceneViewport->SetFixedViewportSize(DesiredSize.X, DesiredSize.Y);
	bViewportHasFixedViewportSize = true;
}

void UMediaCapture::ResetFixedViewportSize(TSharedPtr<FSceneViewport> InViewport, bool bInFlushRenderingCommands)
{
	if (bViewportHasFixedViewportSize && InViewport.IsValid())
	{
		if (bInFlushRenderingCommands && WaitingForResolveCommandExecutionCounter > 0)
		{
			FlushRenderingCommands();
		}
		InViewport->SetFixedViewportSize(0, 0);
		bViewportHasFixedViewportSize = false;
	}
}

bool UMediaCapture::HasFinishedProcessing() const
{
	return WaitingForResolveCommandExecutionCounter == 0
		|| GetState() == EMediaCaptureState::Error
		|| GetState() == EMediaCaptureState::Stopped;
}

void UMediaCapture::InitializeResolveTarget(int32 InNumberOfBuffers)
{
	if (bShouldCaptureRHITexture)
	{
		// No buffer is needed if the callback is with the RHI Texture
		InNumberOfBuffers = 1;
	}

	NumberOfCaptureFrame = InNumberOfBuffers;
	check(CaptureFrames.Num() == 0);
	CaptureFrames.AddDefaulted(InNumberOfBuffers);

	// Only create CPU readback texture when we are using the CPU callback
	if (!bShouldCaptureRHITexture)
	{
		UMediaCapture* This = this;
		ENQUEUE_RENDER_COMMAND(MediaOutputCaptureFrameCreateTexture)(
			[This](FRHICommandListImmediate& RHICmdList)
			{
				FRHIResourceCreateInfo CreateInfo;
				for (int32 Index = 0; Index < This->NumberOfCaptureFrame; ++Index)
				{
					This->CaptureFrames[Index].ReadbackTexture = RHICreateTexture2D(
						This->DesiredOutputSize.X,
						This->DesiredOutputSize.Y,
						This->DesiredOutputPixelFormat,
						1,
						1,
						TexCreate_CPUReadback,
						CreateInfo
					);
				}
				This->bResolvedTargetInitialized = true;
			});
	}
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

	int32 ReadyFrameIndex = (CurrentResolvedTargetIndex) % NumberOfCaptureFrame; // Next one in the buffer queue
	CurrentResolvedTargetIndex = (CurrentResolvedTargetIndex + 1) % NumberOfCaptureFrame;

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
		//Verify if game thread is overrunning the render thread. 
		if (CapturingFrame->bResolvedTargetRequested)
		{
			FlushRenderingCommands();
		}

		CapturingFrame->CaptureBaseData.SourceFrameTimecode = FApp::GetTimecode();
		CapturingFrame->CaptureBaseData.SourceFrameTimecodeFramerate = FApp::GetTimecodeFrameRate();
		CapturingFrame->CaptureBaseData.SourceFrameNumberRenderThread = GFrameNumber;
		CapturingFrame->UserData = GetCaptureFrameUserData_GameThread();
	}

	++WaitingForResolveCommandExecutionCounter;

	// Init variables for ENQUEUE_RENDER_COMMAND
	TWeakPtr<FSceneViewport> InCapturingSceneViewport = CapturingSceneViewport;
	FTextureRenderTargetResource* InTextureRenderTargetResource = nullptr;
	FIntPoint InDesiredSize = DesiredSize;
	FMediaCaptureStateChangedSignature InOnStateChanged = OnStateChanged;
	UMediaCapture* InMediaCapture = this;

	{
		FScopeLock Lock(&AccessingCapturingSource);
		if (CapturingRenderTarget)
		{
			InTextureRenderTargetResource = CapturingRenderTarget->GameThread_GetRenderTargetResource();
		}
	}

	// RenderCommand to be executed on the RenderThread
	ENQUEUE_RENDER_COMMAND(FMediaOutputCaptureFrameCreateTexture)(
		[CapturingFrame, ReadyFrame, InCapturingSceneViewport, InTextureRenderTargetResource, InDesiredSize, InOnStateChanged, InMediaCapture](FRHICommandListImmediate& RHICmdList)
	{
		FTexture2DRHIRef SourceTexture;
		{
			TSharedPtr<FSceneViewport> SceneViewportPtr = InCapturingSceneViewport.Pin();
			if (SceneViewportPtr)
			{
#if WITH_EDITOR
				if (!IsRunningGame())
				{
					// PIE, PIE in windows, editor viewport
					SourceTexture = SceneViewportPtr->GetRenderTargetTexture();
					if (!SourceTexture.IsValid() && SceneViewportPtr->GetViewportRHI())
					{
						SourceTexture = RHICmdList.GetViewportBackBuffer(SceneViewportPtr->GetViewportRHI());
					}
				}
				else
#endif
				if (SceneViewportPtr->GetViewportRHI())
				{
					// Standalone and packaged
					SourceTexture = RHICmdList.GetViewportBackBuffer(SceneViewportPtr->GetViewportRHI());
				}
			}
			else if (InTextureRenderTargetResource && InTextureRenderTargetResource->GetTextureRenderTarget2DResource())
			{
				SourceTexture = InTextureRenderTargetResource->GetTextureRenderTarget2DResource()->GetTextureRHI();
			}
		}

		if (!SourceTexture.IsValid())
		{
			InMediaCapture->SetState(EMediaCaptureState::Error);
			UE_LOG(LogMediaIOCore, Error, TEXT("Can't grab the Texture to capture for '%s'."), *InMediaCapture->MediaOutputName);
		}
		else if (CapturingFrame)
		{
			if (InMediaCapture->DesiredPixelFormat != SourceTexture->GetFormat())
			{
				InMediaCapture->SetState(EMediaCaptureState::Error);
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source pixel format doesn't match with the user requested pixel format. Requested: %s Source: %s")
					, *InMediaCapture->MediaOutputName
					, GetPixelFormatString(InMediaCapture->DesiredPixelFormat)
					, GetPixelFormatString(SourceTexture->GetFormat()));
			}
			else if (InMediaCapture->DesiredCaptureOptions.Crop == EMediaCaptureCroppingType::None)
			{
				if (InDesiredSize.X != SourceTexture->GetSizeX() || InDesiredSize.Y != SourceTexture->GetSizeY())
				{
					InMediaCapture->SetState(EMediaCaptureState::Error);
					UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
						, *InMediaCapture->MediaOutputName
						, InDesiredSize.X, InDesiredSize.Y
						, SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
				}
			}
			else
			{
				FIntPoint StartCapturePoint = FIntPoint::ZeroValue;
				if (InMediaCapture->DesiredCaptureOptions.Crop == EMediaCaptureCroppingType::Custom)
				{
					StartCapturePoint = InMediaCapture->DesiredCaptureOptions.CustomCapturePoint;
				}

				if ((uint32)(InDesiredSize.X + StartCapturePoint.X) > SourceTexture->GetSizeX() || (uint32)(InDesiredSize.Y + StartCapturePoint.Y) > SourceTexture->GetSizeY())
				{
					InMediaCapture->SetState(EMediaCaptureState::Error);
					UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
						, *InMediaCapture->MediaOutputName
						, InDesiredSize.X, InDesiredSize.Y
						, SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
				}
			}
		}

		if (CapturingFrame && InMediaCapture->GetState() != EMediaCaptureState::Error)
		{
			SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_CopyToResolve);

			FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
				InMediaCapture->DesiredOutputSize,
				InMediaCapture->DesiredOutputPixelFormat,
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_RenderTargetable,
				false);
			TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
			GetRendererModule().RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("MediaCapture"));
			const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

			// Do we need to crop
			float ULeft = 0.0f;
			float URight = 1.0f;
			float VTop = 0.0f;
			float VBottom = 1.0f;
			FResolveParams ResolveParams;
			if (InMediaCapture->DesiredCaptureOptions.Crop != EMediaCaptureCroppingType::None)
			{
				switch (InMediaCapture->DesiredCaptureOptions.Crop)
				{
				case EMediaCaptureCroppingType::Center:
					ResolveParams.Rect = FResolveRect((SourceTexture->GetSizeX() - InDesiredSize.X) / 2, (SourceTexture->GetSizeY() - InDesiredSize.Y) / 2, 0, 0);
					ResolveParams.Rect.X2 = ResolveParams.Rect.X1 + InDesiredSize.X;
					ResolveParams.Rect.Y2 = ResolveParams.Rect.Y1 + InDesiredSize.Y;
					break;
				case EMediaCaptureCroppingType::TopLeft:
					ResolveParams.Rect = FResolveRect(0, 0, InDesiredSize.X, InDesiredSize.Y);
					break;
				case EMediaCaptureCroppingType::Custom:
					ResolveParams.Rect = FResolveRect(InMediaCapture->DesiredCaptureOptions.CustomCapturePoint.X, InMediaCapture->DesiredCaptureOptions.CustomCapturePoint.Y, 0, 0);
					ResolveParams.Rect.X2 = ResolveParams.Rect.X1 + InDesiredSize.X;
					ResolveParams.Rect.Y2 = ResolveParams.Rect.Y1 + InDesiredSize.Y;
					break;
				}

				ResolveParams.DestRect.X1 = 0;
				ResolveParams.DestRect.X2 = InDesiredSize.X;
				ResolveParams.DestRect.Y1 = 0;
				ResolveParams.DestRect.Y2 = InDesiredSize.Y;

				ULeft = (float)ResolveParams.Rect.X1 / (float)SourceTexture->GetSizeX();
				URight = (float)ResolveParams.Rect.X2 / (float)SourceTexture->GetSizeX();
				VTop = (float)ResolveParams.Rect.Y1 / (float)SourceTexture->GetSizeY();
				VBottom = (float)ResolveParams.Rect.Y2 / (float)SourceTexture->GetSizeY();
			}

			{
				SCOPED_DRAW_EVENTF(RHICmdList, MediaCapture, TEXT("MediaCapture"));

				if (InMediaCapture->ConversionOperation == EMediaCaptureConversionOperation::NONE)
				{
					// Asynchronously copy target from GPU to GPU
					RHICmdList.CopyToResolveTarget(SourceTexture, DestRenderTarget.TargetableTexture, ResolveParams);
				}
				else
				{
					// convert the source with a draw call
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					FRHITexture* RenderTarget = DestRenderTarget.TargetableTexture.GetReference();
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					SetRenderTargets(RHICmdList, 1, &RenderTarget, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);
					PRAGMA_ENABLE_DEPRECATION_WARNINGS

					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

					// configure media shaders
					auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
					TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);

					const bool bDoLinearToSRGB = false;

					switch (InMediaCapture->ConversionOperation)
					{
					case EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT:
						{
							TShaderMapRef<FRGB8toUYVY8ConvertPS> ConvertShader(ShaderMap);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
							ConvertShader->SetParameters(RHICmdList, SourceTexture, MediaShaders::RgbToYuvRec709Full, MediaShaders::YUVOffset8bits, bDoLinearToSRGB);
						}
						break;
					case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
						{
							TShaderMapRef<FRGB10toYUVv210ConvertPS> ConvertShader(ShaderMap);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
							ConvertShader->SetParameters(RHICmdList, SourceTexture, MediaShaders::RgbToYuvRec709Full, MediaShaders::YUVOffset10bits, bDoLinearToSRGB);
						}
						break;
					case EMediaCaptureConversionOperation::INVERT_ALPHA:
						{
							TShaderMapRef<FInvertAlphaPS> ConvertShader(ShaderMap);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
							ConvertShader->SetParameters(RHICmdList, SourceTexture);
						}
						break;
					case EMediaCaptureConversionOperation::SET_ALPHA_ONE:
						{
							TShaderMapRef<FSetAlphaOnePS> ConvertShader(ShaderMap);
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
							ConvertShader->SetParameters(RHICmdList, SourceTexture);
						}
						break;
					}

					// draw full size quad into render target
					FVertexBufferRHIRef VertexBuffer = CreateTempMediaVertexBuffer(ULeft, URight, VTop, VBottom);
					RHICmdList.SetStreamSource(0, VertexBuffer, 0);

					// set viewport to RT size
					RHICmdList.SetViewport(0, 0, 0.0f, InMediaCapture->DesiredOutputSize.X, InMediaCapture->DesiredOutputSize.Y, 1.0f);
					RHICmdList.DrawPrimitive(0, 2, 1);
					RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DestRenderTarget.TargetableTexture);
				}
			}

			if (InMediaCapture->bShouldCaptureRHITexture)
			{
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_Callback);
				InMediaCapture->OnRHITextureCaptured_RenderingThread(CapturingFrame->CaptureBaseData, CapturingFrame->UserData, DestRenderTarget.TargetableTexture);
				CapturingFrame->bResolvedTargetRequested = false;
			}
			else
			{
				// Asynchronously copy duplicate target from GPU to System Memory
				RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, CapturingFrame->ReadbackTexture, FResolveParams());
				CapturingFrame->bResolvedTargetRequested = true;
			}
		}

		if (!InMediaCapture->bShouldCaptureRHITexture && ReadyFrame && InMediaCapture->GetState() != EMediaCaptureState::Error)
		{
			check(ReadyFrame->ReadbackTexture.IsValid());

			// Lock & read
			void* ColorDataBuffer = nullptr;
			int32 Width = 0, Height = 0;
			{
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_MapStaging);
				RHICmdList.MapStagingSurface(ReadyFrame->ReadbackTexture, ColorDataBuffer, Width, Height);
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_Callback);
				InMediaCapture->OnFrameCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ColorDataBuffer, Width, Height);
			}
			ReadyFrame->bResolvedTargetRequested = false;

			RHICmdList.UnmapStagingSurface(ReadyFrame->ReadbackTexture);
		}

		--InMediaCapture->WaitingForResolveCommandExecutionCounter;
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

	bool ValidateSize(const FIntPoint TargetSize, const FIntPoint& DesiredSize, const FMediaCaptureOptions& CaptureOptions, const bool bCurrentlyCapturing)
	{
		if (CaptureOptions.Crop == EMediaCaptureCroppingType::None)
		{
			if (DesiredSize.X != TargetSize.X || DesiredSize.Y != TargetSize.Y)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target size doesn't match with the requested size. SceneViewport: %d,%d  MediaOutput: %d,%d")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, TargetSize.X, TargetSize.Y
					, DesiredSize.X, DesiredSize.Y);
				return false;
			}
		}
		else
		{
			FIntPoint StartCapturePoint = FIntPoint::ZeroValue;
			if (CaptureOptions.Crop == EMediaCaptureCroppingType::Custom)
			{
				if (CaptureOptions.CustomCapturePoint.X < 0 || CaptureOptions.CustomCapturePoint.Y < 0)
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The start capture point is negatif. Start Point: %d,%d")
						, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
						, StartCapturePoint.X, StartCapturePoint.Y);
					return false;
				}
				StartCapturePoint = CaptureOptions.CustomCapturePoint;
			}

			if (DesiredSize.X + StartCapturePoint.X > TargetSize.X || DesiredSize.Y + StartCapturePoint.Y > TargetSize.Y)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target size is too small for the requested cropping options. SceneViewport: %d,%d  MediaOutput: %d,%d Start Point: %d,%d")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, TargetSize.X, TargetSize.Y
					, DesiredSize.X, DesiredSize.Y
					, StartCapturePoint.X, StartCapturePoint.Y);
				return false;
			}
		}

		return true;
	}

	bool ValidateSceneViewport(const TSharedPtr<FSceneViewport>& SceneViewport, const FMediaCaptureOptions& CaptureOptions, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing)
	{
		if (!SceneViewport.IsValid())
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Scene Viewport is invalid.")
				, bCurrentlyCapturing ? TEXT("continue") : TEXT("start"));
			return false;
		}

		const FIntPoint SceneViewportSize = SceneViewport->GetRenderTargetTextureSizeXY();
		if (!ValidateSize(SceneViewportSize, DesiredSize, CaptureOptions, bCurrentlyCapturing))
		{
			return false;
		}

		static const auto CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		EPixelFormat SceneTargetFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread()));
		if (DesiredPixelFormat != SceneTargetFormat)
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target pixel format doesn't match with the requested pixel format. SceneViewport: %s MediaOutput: %s")
				, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
				, GetPixelFormatString(SceneTargetFormat)
				, GetPixelFormatString(DesiredPixelFormat));
			return false;
		}

		return true;
	}

	bool ValidateTextureRenderTarget2D(const UTextureRenderTarget2D* InRenderTarget2D, const FMediaCaptureOptions& CaptureOptions, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing)
	{
		if (InRenderTarget2D == nullptr)
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Couldn't %s the capture. The Render Target is invalid.")
				, bCurrentlyCapturing ? TEXT("continue") : TEXT("start"));
			return false;
		}

		if (!ValidateSize(FIntPoint(InRenderTarget2D->SizeX, InRenderTarget2D->SizeY), DesiredSize, CaptureOptions, bCurrentlyCapturing))
		{
			return false;
		}

		if (DesiredPixelFormat != InRenderTarget2D->GetFormat())
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target pixel format doesn't match with the requested pixel format. RenderTarget: %s MediaOutput: %s")
				, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
				, GetPixelFormatString(InRenderTarget2D->GetFormat())
				, GetPixelFormatString(DesiredPixelFormat));
			return false;
		}

		return true;
	}

	bool ValidateIsCapturing(const UMediaCapture& CaptureToBeValidated)
	{
		if (CaptureToBeValidated.GetState() != EMediaCaptureState::Capturing && CaptureToBeValidated.GetState() != EMediaCaptureState::Preparing)
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Can not update the capture. There is no capture currently.\
			Only use UpdateSceneViewport or UpdateTextureRenderTarget2D when the state is Capturing or Preparing"));
			return false;
		}

		return true;
	}

	void ShowSlateNotification()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			static double PreviousWarningTime = 0.0;
			const double TimeNow = FPlatformTime::Seconds();
			const double TimeBetweenWarningsInSeconds = 3.0f;

			if (TimeNow - PreviousWarningTime > TimeBetweenWarningsInSeconds)
			{
				FNotificationInfo NotificationInfo(LOCTEXT("MediaCaptureFailedError", "The media failed to capture. Check Output Log for details!"));
				NotificationInfo.ExpireDuration = 2.0f;
				FSlateNotificationManager::Get().AddNotification(NotificationInfo);

				PreviousWarningTime = TimeNow;
			}
		}
#endif // WITH_EDITOR
	}
}

#undef LOCTEXT_NAMESPACE
