// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "BlackmagicMediaViewportOutputImpl.h"

#include "BlackmagicMediaOutput.h"
#include "IBlackmagicMediaOutputModule.h"

#include "MediaIOCoreWaitVSyncThread.h"
#include "MediaIOCoreEncodeTime.h"

#include "RHIResources.h"

#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Templates/Atomic.h"

#include "Widgets/SViewport.h"


/* Utility Functions
*****************************************************************************/

namespace BlackmagicMediaOutputDevice
{
	void CopyFrame(uint32 InWidth, uint32 InHeight, uint8* InDst, uint32 InDstMod, uint8* InSrc, uint32 InSrcMod);;
	BlackmagicDevice::FTimecode ConvertToTimecode(const FTimecode& InTimecode, float InFPS);
}

/* namespace FramePayload implementation
*****************************************************************************/
bool FBlackmagicFramePayload::OnFrameReady_RenderThread(FColor* ColorBuffer, FIntPoint BufferSize, FIntPoint TargetSize) const
{
	if (bUseEndFrameRenderThread)
	{
		// Lock to make sure ViewportOutputImpl won't be deleted while updating the buffer
		TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> Shared = ViewportOutputImpl.Pin();
		if (Shared.IsValid())
		{
			Shared->OnEndFrame_RenderThread(Timecode, ColorBuffer, BufferSize.X, BufferSize.Y);
		}
	}
	return !bUseEndFrameRenderThread;
}

/* MediaViewportOutputImpl implementation
*****************************************************************************/
FBlackmagicMediaViewportOutputImpl::FBlackmagicMediaViewportOutputImpl()
	: VSyncThread(nullptr)
	, VSyncRunnableThread(nullptr)
	, bClearBuffer(false)
	, bOutputTimecode(false)
	, bCopyOnRenderThread(false)
	, bWaitForOutputFrame(false)
	, bWaitForVSyncEvent(false)
	, Device(nullptr)
	, Port(nullptr)
	, LastFrameDropCount(0)
	, FrameRate(30, 1)
{
}

FBlackmagicMediaViewportOutputImpl::~FBlackmagicMediaViewportOutputImpl()
{
	if (VSyncRunnableThread.IsValid())
	{
		check(VSyncThread.IsValid());
		
		// Wait for the thread to return.
		VSyncRunnableThread->WaitForCompletion();
		VSyncRunnableThread.Reset();
		VSyncThread.Reset();
	}

	ReleaseDevice();
}

TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> FBlackmagicMediaViewportOutputImpl::CreateShared(UBlackmagicMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> SceneViewport)
{
	TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> Result = TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe>(new FBlackmagicMediaViewportOutputImpl());
	if (!Result->Initialize(MediaOutput, SceneViewport))
	{
		Result.Reset();
	}
	return Result;
}

bool FBlackmagicMediaViewportOutputImpl::Initialize(UBlackmagicMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> InSceneViewport)
{
	check(MediaOutput);
	check(InSceneViewport.IsValid());

	bClearBuffer = MediaOutput->bClearBuffer;
	ClearBufferColor = MediaOutput->ClearBufferColor;
	bOutputTimecode = MediaOutput->bOutputTimecode;
	bCopyOnRenderThread = MediaOutput->bCopyOnRenderThread;
	bWaitForOutputFrame = MediaOutput->bWaitForOutputFrame;
	bWaitForVSyncEvent = MediaOutput->bWaitForVSyncEvent;
	PortName = MediaOutput->FillPort.ToString();
	bool bVSyncEventOnAnotherThread = bWaitForVSyncEvent && MediaOutput->bVSyncEventOnAnotherThread;
	
	if (!BlackmagicDevice::VideoIOModeFrameDesc(MediaOutput->MediaMode.Mode, FrameDesc))
	{
		return false;
	}
	BlackmagicDevice::VideoIOFrameDesc2Info(FrameDesc, FrameInfo);	
	FrameRate = FFrameRate(FrameInfo.TimeValue, FrameInfo.TimeScale);

	if (!InitDevice(MediaOutput))
	{
		return false;
	}
	check(Port);

	SceneViewport = InSceneViewport;
	{
		TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
		if (Widget.IsValid())
		{
			bSavedIgnoreTextureAlpha = Widget->GetIgnoreTextureAlpha();
			if (MediaOutput->OutputType == EBlackmagicMediaOutputType::FillAndKey)
			{
				Widget->SetIgnoreTextureAlpha(false);
			}
		}
	}

	EPixelFormat PixelFormat = PF_B8G8R8A8;
	uint32 RingBufferSize = 2;
	FrameGrabber = MakeShareable(new FFrameGrabber(InSceneViewport.ToSharedRef(), InSceneViewport->GetSize(), PixelFormat, RingBufferSize));
	FrameGrabber->StartCapturingFrames();

	if (bVSyncEventOnAnotherThread)
	{
		TSharedPtr<IMediaIOCoreHardwareSync> HardwareSync(new FBlackmagicHardwareSync(Port));
		VSyncThread = MakeUnique<FMediaIOCoreWaitVSyncThread>(HardwareSync);
		VSyncRunnableThread.Reset(FRunnableThread::Create(VSyncThread.Get(), TEXT("FBlackmagicMediaWaitVSyncThread::FBlackmagicMediaWaitVSyncThread"), TPri_AboveNormal));
	}

	if (!bCopyOnRenderThread)
	{
		EndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FBlackmagicMediaViewportOutputImpl::OnEndFrame_GameThread);
	}

	// Managed dropped frames
	LastFrameDropCount = Port->FrameDropCount();

	return true;
}

void FBlackmagicMediaViewportOutputImpl::Shutdown()
{
	if (VSyncRunnableThread.IsValid())
	{
		check(VSyncThread.IsValid());
		VSyncThread->Stop(); // stop but don't wait right now, this may take some time
	}

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

void FBlackmagicMediaViewportOutputImpl::Tick(const FTimecode& InTimecode)
{
	if (FrameGrabber.IsValid() && Device && Port)
	{
		auto CurrentPayload = MakeShared<FBlackmagicFramePayload, ESPMode::ThreadSafe>();

		CurrentPayload->ViewportOutputImpl = AsShared();
		CurrentPayload->bUseEndFrameRenderThread = bCopyOnRenderThread;
		CurrentPayload->Timecode = InTimecode;

		FrameGrabber->CaptureThisFrame(CurrentPayload);
		
		VerifyFrameDropCount();
	}
	else
	{
		Shutdown();
	}
}

bool FBlackmagicMediaViewportOutputImpl::HasFinishedProcessing() const
{
	return Device == nullptr || Port == nullptr || !FrameGrabber.IsValid() || !FrameGrabber->HasOutstandingFrames();
}

bool FBlackmagicMediaViewportOutputImpl::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
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

void FBlackmagicMediaViewportOutputImpl::OnEndFrame_GameThread()
{
	if (FrameGrabber.IsValid() && Port)
	{
		bool bFrameWasCaptured = false;
		if (WaitForVSync())
		{
			if (WaitForOutputFrame())
			{
				TArray<FCapturedFrameData> Frames = FrameGrabber->GetCapturedFrames();
				bFrameWasCaptured = true;
				if (Frames.Num() > 0)
				{
					FCapturedFrameData& LastFrame = Frames.Last();

					FTimecode Timecode;
					if (LastFrame.Payload.IsValid())
					{
						Timecode = static_cast<FBlackmagicFramePayload*>(LastFrame.Payload.Get())->Timecode;
					}
					Present(Timecode, reinterpret_cast<uint8*>(LastFrame.ColorBuffer.GetData()), LastFrame.BufferSize.X, LastFrame.BufferSize.Y);
				}
				else
				{
					UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("No output frame was available."));
				}
			}
			else
			{
				UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("No frame was captured."));
			}
		}
		
		// capture the frame to prevent the buffer to grow
		if (!bFrameWasCaptured)
		{
			FrameGrabber->GetCapturedFrames();
		}
	}
}

void FBlackmagicMediaViewportOutputImpl::OnEndFrame_RenderThread(const FTimecode& FrameTimecode, FColor* ColorBuffer, int32 ColorBufferWidth, int32 ColorBufferHeight)
{
	check(ColorBuffer);

	if (Port)
	{
		if (WaitForVSync())
		{
			if (WaitForOutputFrame())
			{
				Present(FrameTimecode, reinterpret_cast<uint8*>(ColorBuffer), ColorBufferWidth, ColorBufferHeight);
			}
		}
	}
}

bool FBlackmagicMediaViewportOutputImpl::WaitForVSync() const
{
	bool bResult = true;
	if (bWaitForVSyncEvent)
	{
		if (VSyncThread)
		{
			bResult = VSyncThread->Wait_GameOrRenderThread();
		}
		else
		{
			Port->WaitVSync();
		}
	}

	return bResult;
}

bool FBlackmagicMediaViewportOutputImpl::WaitForOutputFrame() const
{
	bool bResult = bWaitForOutputFrame || Port->PeekFrame();
	if (!bResult)
	{
		UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("No output frame was available."));
	}
	return bResult;
}

void FBlackmagicMediaViewportOutputImpl::Present(const FTimecode& FrameTimecode, uint8* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight) const
{
	BlackmagicDevice::FFrame Frame = Port->WaitFrame();
	uint32 Width, Height;
	BlackmagicDevice::VideoIOFrameDimensions(Frame, Width, Height);

	uint32 Size;
	void* Memory = BlackmagicDevice::VideoIOFrameVideoBuffer(Frame, Size);

	// Clip/Center into output buffer
	uint32 ClipWidth = (static_cast<uint32>(ColorBufferWidth) > Width) ? Width : ColorBufferWidth;
	uint32 ClipHeight = (static_cast<uint32>(ColorBufferHeight) > Height) ? Height : ColorBufferHeight;
	uint32 DestOffsetX = (Width - ClipWidth) / 2;
	uint32 DestOffsetY = (Height - ClipHeight) / 2;
	uint32 SrcOffsetX = (ColorBufferWidth - ClipWidth) / 2;
	uint32 SrcOffsetY = (ColorBufferHeight - ClipHeight) / 2;

	if (bClearBuffer)
	{
		uint32 Color = ClearBufferColor.ToPackedARGB();
		uint32* MemoryAsColor = reinterpret_cast<uint32*>(Memory);
		uint32* Last = MemoryAsColor + Width*Height;
		for (; MemoryAsColor != Last; ++MemoryAsColor)
		{
			*MemoryAsColor = Color;
		}
	}

	uint8* DestBuffer = static_cast<uint8*>(Memory) + (DestOffsetX + DestOffsetY * Width) * 4;
	uint32 DestMod = Width * 4;
	uint8* SrcBuffer = static_cast<uint8*>(ColorBuffer) + (SrcOffsetX + SrcOffsetY * ColorBufferWidth) * 4;
	uint32 SrcMod = ColorBufferWidth * 4;
	BlackmagicMediaOutputDevice::CopyFrame(ClipWidth, ClipHeight, DestBuffer, DestMod, SrcBuffer, SrcMod);
	
	// pass the output timecode
	if (bOutputTimecode)
	{
		BlackmagicDevice::FTimecode Timecode = BlackmagicMediaOutputDevice::ConvertToTimecode(FrameTimecode, FrameRate.AsDecimal());
		BlackmagicDevice::VideoIOFrameTimecode(Frame, Timecode);

		if (bIsTimecodeLogEnable)
		{
			UE_LOG(LogBlackmagicMediaOutput, Log, TEXT("Blackmagic output port %s has timecode : %02d:%02d:%02d:%02d"), *PortName, Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
		}
	}
	BlackmagicDevice::VideoIOReleaseFrame(Frame);
}

void FBlackmagicMediaViewportOutputImpl::VerifyFrameDropCount()
{
	const uint32 FrameDropCount = Port->FrameDropCount();
	if (FrameDropCount > LastFrameDropCount)
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("Lost %d frames on output %s. Frame rate may be too slow."), FrameDropCount - LastFrameDropCount, *PortName);
	}
	LastFrameDropCount = FrameDropCount;
}

bool FBlackmagicMediaViewportOutputImpl::InitDevice(UBlackmagicMediaOutput* MediaOutput)
{
	check(MediaOutput);
	if (!MediaOutput->FillPort.IsValid())
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("The FillPort of '%s' is not valid."), *MediaOutput->GetName());
		return false;
	}

	Device = BlackmagicDevice::VideoIOCreateDevice(MediaOutput->FillPort.DeviceIndex);
	if (!Device)
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("The Device for '%s' could not be created."), *MediaOutput->GetName());
		return false;
	}

	uint32_t PortIndex = MediaOutput->FillPort.PortIndex;

	BlackmagicDevice::FPortOptions Options = {};
	Options.bOutput = true;
	Options.bUseTimecode = bOutputTimecode;
	Options.bOutputKey = MediaOutput->OutputType == EBlackmagicMediaOutputType::FillAndKey;

	// get the output video mode.
	BlackmagicDevice::FUInt MediaMode = MediaOutput->MediaMode.Mode;
	if (!BlackmagicDevice::VideoIOModeFrameDesc(MediaMode, FrameDesc))
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("bad mode (%d), default to default."), MediaMode);
	}

	Port = BlackmagicDevice::VideoIODeviceOpenSharedPort(Device, PortIndex, FrameDesc, Options);
	if (!Port)
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("The output port for '%s' could not be opened."), *MediaOutput->GetName());
		return false;
	}

	// Get info on the current video mode
	BlackmagicDevice::VideoIOFrameDesc2Info(FrameDesc, FrameInfo);
	return true;
}

void FBlackmagicMediaViewportOutputImpl::ReleaseDevice()
{
	if (Port)
	{
		Port->Release();
		Port = nullptr;
	}

	if (Device)
	{
		BlackmagicDevice::VideoIOReleaseDevice(Device);
		Device = nullptr;
	}
}

/* namespace implementation
*****************************************************************************/
namespace BlackmagicMediaOutputDevice
{
	void CopyFrame(uint32 InWidth, uint32 InHeight, uint8* InDst, uint32 InDstMod, uint8* InSrc, uint32 InSrcMod)
	{
		if (InWidth * 4 == InSrcMod && InSrcMod == InDstMod)
		{
			::memcpy(InDst, InSrc, InHeight * InWidth * 4);
		}
		else
		{
			while (InHeight)
			{
				::memcpy(InDst, InSrc, InWidth * 4);
				InDst += InDstMod;
				InSrc += InSrcMod;
				--InHeight;
			}
		}
	}

	BlackmagicDevice::FTimecode ConvertToTimecode(const FTimecode& InTimecode, float InFPS)
	{
		//We can't write frame numbers greater than 30
		//Get by how much we need to divide the actual count.
		const int32 Divider = FMath::CeilToInt(InFPS / 30.0f);

		BlackmagicDevice::FTimecode Timecode;
		Timecode.Hours = InTimecode.Hours;
		Timecode.Minutes = InTimecode.Minutes;
		Timecode.Seconds = InTimecode.Seconds;
		Timecode.Frames = InTimecode.Frames / Divider;
		return Timecode;
	}
}
