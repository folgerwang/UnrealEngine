// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaViewportOutputImpl.h"


#include "AjaMediaOutput.h"
#include "IAjaMediaOutputModule.h"

#include "RHIResources.h"

#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"

#include "Widgets/SViewport.h"


/* namespace AjaMediaOutputDevice definition
*****************************************************************************/
namespace AjaMediaOutputDevice
{
	void CopyFrame(uint32 InWidth, uint32 InHeight, uint8* InDst, uint32 InDstMod, const uint8* InSrc, uint32 InSrcMod);;
	AJA::FTimecode ConvertToAJATimecode(const FTimecode& InTimecode, float InFPS);
}

/* namespace FAjaFramePayload implementation
*****************************************************************************/
bool FAjaFramePayload::OnFrameReady_RenderThread(FColor* ColorBuffer, FIntPoint BufferSize, FIntPoint TargetSize) const
{
	if (bUseEndFrameRenderThread)
	{
		// Lock to make sure ViewportOutputImpl won't be deleted while updating the buffer
		TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> Shared = ViewportOutputImpl.Pin();
		if (Shared.IsValid())
		{
			Shared->OnEndFrame_RenderThread(Timecode, ColorBuffer, BufferSize.X, BufferSize.Y);
		}
	}
	return !bUseEndFrameRenderThread;
}

/* FAjaMediaViewportOutputImpl implementation
*****************************************************************************/
FAjaMediaViewportOutputImpl::FAjaMediaViewportOutputImpl()
	: bClearBuffer(false)
	, bOutputTimecode(false)
	, bCopyOnRenderThread(false)
	, bWaitForSyncEvent(false)
	, bEncodeTimecodeInTexel(false)
	, bSavedIgnoreTextureAlpha(false)
	, bIgnoreTextureAlphaChanged(false)
	, WakeUpEvent(nullptr)
	, CurrentState(EMediaState::Closed)
	, AjaThreadNewState(EMediaState::Error)
	, OutputChannel(nullptr)
	, LastFrameDropCount(0)
	, FrameRate(30, 1)
	, bIsTimecodeLogEnable(false)
{
}

FAjaMediaViewportOutputImpl::~FAjaMediaViewportOutputImpl()
{
	Shutdown();
}

TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> FAjaMediaViewportOutputImpl::CreateShared(UAjaMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> SceneViewport)
{
	TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> Result = TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe>(new FAjaMediaViewportOutputImpl());
	if (!Result->Initialize(MediaOutput, SceneViewport))
	{
		Result.Reset();
	}
	return Result;
}

bool FAjaMediaViewportOutputImpl::Initialize(UAjaMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> InSceneViewport)
{
	check(MediaOutput);
	check(InSceneViewport.IsValid());

	CurrentState = EMediaState::Preparing;
	AjaThreadNewState = EMediaState::Preparing;

	bClearBuffer = MediaOutput->bClearBuffer;
	ClearBufferColor = MediaOutput->ClearBufferColor;
	bOutputTimecode = MediaOutput->bOutputTimecode;
	bCopyOnRenderThread = MediaOutput->bCopyVideoOnRenderThread;
	bWaitForSyncEvent = MediaOutput->bOutputWithAutoCirculating && MediaOutput->bWaitForSyncEvent; // Can only wait if using AutoCirculate
	bEncodeTimecodeInTexel = MediaOutput->bEncodeTimecodeInTexel;
	PortName = MediaOutput->FillPort.ToString();

	if (!InitAJA(MediaOutput))
	{
		CurrentState = EMediaState::Error;
		AjaThreadNewState = EMediaState::Error;
		return false;
	}
	check(OutputChannel);

	SceneViewport = InSceneViewport;
	{
		TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
		if (Widget.IsValid())
		{
			bSavedIgnoreTextureAlpha = Widget->GetIgnoreTextureAlpha();
			if (MediaOutput->OutputType == EAjaMediaOutputType::FillAndKey)
			{
				if (bSavedIgnoreTextureAlpha)
				{
					bIgnoreTextureAlphaChanged = true;
					Widget->SetIgnoreTextureAlpha(false);
				}
			}
		}
	}

	if (bWaitForSyncEvent)
	{
		const bool bIsManualReset = false;
		WakeUpEvent = FPlatformProcess::GetSynchEventFromPool(bIsManualReset);
	}

	EPixelFormat PixelFormat = PF_B8G8R8A8;
	uint32 RingBufferSize = 2;
	bool bAlwaysFlushOnDraw = false;
	FrameGrabber = MakeShareable(new FFrameGrabber(InSceneViewport.ToSharedRef(), InSceneViewport->GetSize(), PixelFormat, RingBufferSize, bAlwaysFlushOnDraw));

	if (!bCopyOnRenderThread)
	{
		EndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FAjaMediaViewportOutputImpl::OnEndFrame_GameThread);
	}

	return true;
}

void FAjaMediaViewportOutputImpl::Shutdown()
{
	{
		FScopeLock ScopeLock(&RenderThreadCriticalSection); // Prevent the rendering thread from copying while we are shutdowning..

		ReleaseAJA();
		if (WakeUpEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(WakeUpEvent);
			WakeUpEvent = nullptr;
		}
	}

	if (bIgnoreTextureAlphaChanged)
	{ // restore the ignore texture alpha state
		TSharedPtr<FSceneViewport> ViewPort(SceneViewport.Pin());
		if (ViewPort.IsValid())
		{
			TSharedPtr<SViewport> Widget(ViewPort->GetViewportWidget().Pin());
			if (Widget.IsValid())
			{
				Widget->SetIgnoreTextureAlpha(bSavedIgnoreTextureAlpha);
			}
		}
		bIgnoreTextureAlphaChanged = false;
	}

	SceneViewport.Reset();
	if(FrameGrabber.IsValid())
	{
		FrameGrabber->StopCapturingFrames();
		FrameGrabber.Reset();
	}

	if (EndFrameHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
	}
}

bool FAjaMediaViewportOutputImpl::InitAJA(UAjaMediaOutput* MediaOutput)
{
	check(MediaOutput);
	if (!MediaOutput->FillPort.IsValid())
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("The FillPort of '%s' is not valid."), *MediaOutput->GetName());
		return false;
	}

	if (MediaOutput->FillPort.DeviceIndex != MediaOutput->SyncPort.DeviceIndex || MediaOutput->FillPort.DeviceIndex != MediaOutput->KeyPort.DeviceIndex)
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("The FillPort & SyncPort & KeyPort of '%s' are not on the same device."), *MediaOutput->GetName());
		return false;
	}

	AJA::AJADeviceOptions DeviceOptions(MediaOutput->FillPort.DeviceIndex);

	AJA::AJAInputOutputChannelOptions ChannelOptions(TEXT("ViewportOutput"), MediaOutput->FillPort.PortIndex);
	ChannelOptions.CallbackInterface = this;
	ChannelOptions.bOutput = true;

	if (!AJA::Mode2FrameDesc(MediaOutput->MediaMode.Mode, AJA::EDirectionFilter::DF_OUTPUT, ChannelOptions.FrameDesc))
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("Mode not supported for output."), *MediaOutput->GetName());
		return false;
	}

	AJA::FFrameInfo FrameInfo;
	AJA::FrameDesc2Info(ChannelOptions.FrameDesc, FrameInfo);
	FrameRate = FFrameRate(FrameInfo.TimeValue, FrameInfo.TimeScale);

	ChannelOptions.NumberOfAudioChannel = 0;
	ChannelOptions.SynchronizeChannelIndex = MediaOutput->SyncPort.PortIndex;
	ChannelOptions.OutputKeyChannelIndex = MediaOutput->KeyPort.PortIndex;
	ChannelOptions.bUseAutoCirculating = MediaOutput->bOutputWithAutoCirculating;
	ChannelOptions.bOutputKey = MediaOutput->OutputType == EAjaMediaOutputType::FillAndKey;  // must be RGBA to support Fill+Key
	ChannelOptions.bUseTimecode = bOutputTimecode;
	ChannelOptions.bUseAncillary = false;
	ChannelOptions.bUseAncillaryField2 = false;
	ChannelOptions.bUseAudio = false;
	ChannelOptions.bUseVideo = true;

	OutputChannel = new AJA::AJAOutputChannel();
	if (!OutputChannel->Initialize(DeviceOptions, ChannelOptions))
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("The AJA output port for '%s' could not be opened."), *MediaOutput->GetName());
		delete OutputChannel;
		OutputChannel = nullptr;
		return false;
	}

	return true;
}

void FAjaMediaViewportOutputImpl::ReleaseAJA()
{
	if (OutputChannel)
	{
		OutputChannel->Uninitialize(); // close the channel in the another thread
		delete OutputChannel;
		OutputChannel = nullptr;
	}
}

void FAjaMediaViewportOutputImpl::Tick(const FTimecode& InTimecode)
{
	EMediaState NewState = AjaThreadNewState;
	if (NewState != CurrentState)
	{
		CurrentState = NewState;

		check(FrameGrabber.IsValid());
		if (NewState == EMediaState::Playing)
		{
			FrameGrabber->StartCapturingFrames();
		}
	}

	if (FrameGrabber.IsValid() && OutputChannel)
	{
		if (CurrentState == EMediaState::Closed || CurrentState == EMediaState::Error)
		{
			Shutdown();
		}
		else if (CurrentState == EMediaState::Playing)
		{
			auto CurrentPayload = MakeShared<FAjaFramePayload, ESPMode::ThreadSafe>();

			CurrentPayload->ViewportOutputImpl = AsShared();
			CurrentPayload->bUseEndFrameRenderThread = bCopyOnRenderThread;
			CurrentPayload->Timecode = InTimecode;

			FrameGrabber->CaptureThisFrame(CurrentPayload);
		}
	}
	else
	{
		Shutdown();
	}
}

bool FAjaMediaViewportOutputImpl::HasFinishedProcessing() const
{
	return OutputChannel == nullptr
		|| !FrameGrabber.IsValid()
		|| !FrameGrabber->HasOutstandingFrames()
		|| CurrentState == EMediaState::Closed
		|| CurrentState == EMediaState::Error;
}

bool FAjaMediaViewportOutputImpl::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if !UE_BUILD_SHIPPING
	if (FParse::Command(&Cmd, TEXT("MediaIO")))
	{
		if (FParse::Command(&Cmd, TEXT("ShowOutputTimecode")))
		{
			bIsTimecodeLogEnable = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("HideOutputTimecode")))
		{
			bIsTimecodeLogEnable = false;
			return true;
		}
	}
#endif
	return false;
}

void FAjaMediaViewportOutputImpl::OnEndFrame_GameThread()
{
	if (FrameGrabber.IsValid() && OutputChannel)
	{
		bool bFrameWasCaptured = false;

		TArray<FCapturedFrameData> Frames = FrameGrabber->GetCapturedFrames();
		bFrameWasCaptured = true;
		if (Frames.Num() > 0)
		{
			FCapturedFrameData& LastFrame = Frames.Last();

			FTimecode Timecode;
			if (LastFrame.Payload.IsValid())
			{
				Timecode = static_cast<FAjaFramePayload*>(LastFrame.Payload.Get())->Timecode;
			}
			SendToAja(Timecode, LastFrame.ColorBuffer.GetData(), LastFrame.BufferSize.X, LastFrame.BufferSize.Y);
		}
		else
		{
			UE_LOG(LogAjaMediaOutput, Error, TEXT("No output frame was available."));
		}
		
		// capture the frame to prevent the buffer to grow
		if (!bFrameWasCaptured)
		{
			FrameGrabber->GetCapturedFrames();
		}

		WaitForSync();
	}
}

void FAjaMediaViewportOutputImpl::OnEndFrame_RenderThread(const FTimecode& FrameTimecode, FColor* ColorBuffer, int32 ColorBufferWidth, int32 ColorBufferHeight)
{
	FScopeLock ScopeLock(&RenderThreadCriticalSection); // Prevent the rendering thread from copying while we are shutdowning..

	SendToAja(FrameTimecode, ColorBuffer, ColorBufferWidth, ColorBufferHeight);
	WaitForSync();
}

bool FAjaMediaViewportOutputImpl::WaitForSync() const
{
	bool bResult = true;
	if (bWaitForSyncEvent)
	{
		if (WakeUpEvent) // In render thread path, could be shutdown in a middle of a frame
		{
			WakeUpEvent->Wait();
		}
	}

	return bResult;
}

void FAjaMediaViewportOutputImpl::EncodeTimecode_Pattern(FColor* ColorBuffer, uint32 ColorBufferWidth, int32 HeightIndex, int32 Amount) const
{
	for (int32 Index = 0; Index < Amount; ++Index)
	{
		*(ColorBuffer + (ColorBufferWidth * HeightIndex) + Index) = (Index % 2) ? FColor::Red : FColor::Black;
	}
}

void FAjaMediaViewportOutputImpl::EncodeTimecode_Time(FColor* ColorBuffer, uint32 ColorBufferWidth, int32 HeightIndex, int32 Time) const
{
	int32 Tenth = (Time / 10);
	int32 Unit = (Time % 10);
	if (Tenth > 0)
	{
		*(ColorBuffer + (ColorBufferWidth * HeightIndex) + Tenth - 1) = FColor::White;
	}
	*(ColorBuffer + (ColorBufferWidth * (1 + HeightIndex)) + Unit) = FColor::White;
}

void FAjaMediaViewportOutputImpl::EncodeTimecode(const AJA::FTimecode& Timecode, FColor* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight) const
{
	if (bEncodeTimecodeInTexel)
	{
		const int32 FillWidth = 12;
		const int32 FillHeight = 6*2;

		if (ColorBufferWidth > FillWidth && ColorBufferHeight > FillHeight)
		{
			for (int32 IndexHeight = 0; IndexHeight < FillHeight; ++IndexHeight)
			{
				for (int32 IndexWidth = 0; IndexWidth < FillWidth; ++IndexWidth)
				{
					*(ColorBuffer + (ColorBufferWidth*IndexHeight) + IndexWidth) = FColor::Black;
				}
			}

			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 0, 2);	//hh
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 1, 10);
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 3, 6);	//mm
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 4, 10);
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 6, 6);	//ss
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 7, 10);
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 9, 6);	//ff
			EncodeTimecode_Pattern(ColorBuffer, ColorBufferWidth, 10, 10);

			EncodeTimecode_Time(ColorBuffer, ColorBufferWidth, 0, Timecode.Hours);
			EncodeTimecode_Time(ColorBuffer, ColorBufferWidth, 3, Timecode.Minutes);
			EncodeTimecode_Time(ColorBuffer, ColorBufferWidth, 6, Timecode.Seconds);
			EncodeTimecode_Time(ColorBuffer, ColorBufferWidth, 9, Timecode.Frames);
		}
	}
}

void FAjaMediaViewportOutputImpl::SendToAja(const FTimecode& FrameTimecode, FColor* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight)
{
	check(ColorBuffer);
	static_assert(sizeof(FColor) == 4, "The size of FColor is not compatible with AJA int32_t format.");

	if (!OutputChannel) // In render thread path, could be shutdown in a middle of a frame
	{
		return;
	}

	AJA::FTimecode Timecode = AjaMediaOutputDevice::ConvertToAJATimecode(FrameTimecode, FrameRate.AsDecimal());

	if (AjaWishResolution.X == ColorBufferWidth && AjaWishResolution.Y == ColorBufferHeight)
	{
		EncodeTimecode(Timecode, ColorBuffer, ColorBufferWidth, ColorBufferHeight);
		OutputChannel->SetVideoBuffer(Timecode, reinterpret_cast<uint8_t*>(ColorBuffer), ColorBufferWidth*ColorBufferHeight * 4);
	}
	else
	{
		const uint32 AjaWidth = AjaWishResolution.X;
		const uint32 AjaHeight = AjaWishResolution.Y;

		FrameData.ColorBuffer.Reset(AjaWidth*AjaHeight);
		FrameData.ColorBuffer.InsertUninitialized(0, AjaWidth*AjaHeight);

		// Clip/Center into output buffer
		const uint32 ClipWidth = (ColorBufferWidth > AjaWidth) ? AjaWidth : ColorBufferWidth;
		const uint32 ClipHeight = (ColorBufferHeight > AjaHeight) ? AjaHeight : ColorBufferHeight;
		const uint32 DestOffsetX = (AjaWidth - ClipWidth) / 2;
		const uint32 DestOffsetY = (AjaHeight - ClipHeight) / 2;
		const uint32 SrcOffsetX = (ColorBufferWidth - ClipWidth) / 2;
		const uint32 SrcOffsetY = (ColorBufferHeight - ClipHeight) / 2;

		if (bClearBuffer)
		{
			if (ColorBufferWidth < AjaWidth || ColorBufferHeight < AjaHeight)
			{
				for (FColor& Color : FrameData.ColorBuffer)
				{
					Color = ClearBufferColor;
				}
			}
		}

		uint8* DestBuffer = reinterpret_cast<uint8*>(FrameData.ColorBuffer.GetData()) + ((DestOffsetX + DestOffsetY * AjaWidth) * 4);
		const uint32 DestMod = AjaWidth * 4;
		const uint8* SrcBuffer = reinterpret_cast<const uint8*>(ColorBuffer) + ((SrcOffsetX + SrcOffsetY * ColorBufferWidth) * 4);
		const uint32 SrcMod = ColorBufferWidth * 4;
		AjaMediaOutputDevice::CopyFrame(ClipWidth, ClipHeight, DestBuffer, DestMod, SrcBuffer, SrcMod);

		if (bIsTimecodeLogEnable)
		{
			UE_LOG(LogAjaMediaOutput, Log, TEXT("Aja output port %s has timecode : %02d:%02d:%02d:%02d"), *PortName, Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
		}

		EncodeTimecode(Timecode, ColorBuffer, AjaWidth, AjaHeight);
		OutputChannel->SetVideoBuffer(Timecode, reinterpret_cast<uint8*>(FrameData.ColorBuffer.GetData()), AjaWidth*AjaHeight* 4);
	}
}


/* namespace IAJAInputCallbackInterface implementation
// This is called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
*****************************************************************************/
void FAjaMediaViewportOutputImpl::OnInitializationCompleted(bool bSucceed)
{
	if (bSucceed)
	{
		uint32 X, Y;
		bSucceed = OutputChannel->GetOutputDimension(X, Y);
		AjaWishResolution.X = X;
		AjaWishResolution.Y = Y;
	}
	AjaThreadNewState = bSucceed ? EMediaState::Playing : EMediaState::Error;

	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}
}

bool FAjaMediaViewportOutputImpl::OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData)
{
	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}

	const uint32 FrameDropCount = InFrameData.FramesDropped;
	if (FrameDropCount > LastFrameDropCount)
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("Lost %d frames on Aja output %s. Frame rate may be too slow."), FrameDropCount - LastFrameDropCount, *PortName);
	}
	LastFrameDropCount = FrameDropCount;

	return true;
}

void FAjaMediaViewportOutputImpl::OnCompletion(bool bSucceed)
{
	AjaThreadNewState = bSucceed ? EMediaState::Closed : EMediaState::Error;
	if (WakeUpEvent)
	{
		WakeUpEvent->Trigger();
	}
}

bool FAjaMediaViewportOutputImpl::OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame)
{
	check(false);
	return false;
}

/* namespace AjaMediaOutputDevice implementation
*****************************************************************************/
namespace AjaMediaOutputDevice
{
	void CopyFrame(uint32 InWidth, uint32 InHeight, uint8* InDst, uint32 InDstMod, const uint8* InSrc, uint32 InSrcMod)
	{
		if (InWidth * 4 == InSrcMod && InSrcMod == InDstMod)
		{
			FMemory::Memcpy(InDst, InSrc, InHeight * InWidth * 4);
		}
		else
		{
			while (InHeight)
			{
				FMemory::Memcpy(InDst, InSrc, InWidth * 4);
				InDst += InDstMod;
				InSrc += InSrcMod;
				--InHeight;
			}
		}
	}

	AJA::FTimecode ConvertToAJATimecode(const FTimecode& InTimecode, float InFPS)
	{
		//We can't write frame numbers greater than 30
		//Get by how much we need to divide the actual count.
		const int32 Divider = FMath::CeilToInt(InFPS / 30.0f);

		AJA::FTimecode Timecode;
		Timecode.Hours = InTimecode.Hours;
		Timecode.Minutes = InTimecode.Minutes;
		Timecode.Seconds = InTimecode.Seconds;
		Timecode.Frames = InTimecode.Frames / Divider;
		return Timecode;
	}
}
