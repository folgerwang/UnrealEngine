// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaCapture.h"

#include "AjaMediaOutput.h"
#include "Engine/RendererSettings.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "IAjaMediaOutputModule.h"
#include "Misc/ScopeLock.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"


/* namespace AjaMediaCaptureDevice
*****************************************************************************/
namespace AjaMediaCaptureDevice
{
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

///* FAjaOutputCallback definition
//*****************************************************************************/
struct UAjaMediaCapture::FAjaOutputCallback : public AJA::IAJAInputOutputChannelCallbackInterface
{
	virtual void OnInitializationCompleted(bool bSucceed) override;
	virtual bool OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame) override;
	virtual bool OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData) override;
	virtual void OnOutputFrameStarted() override;
	virtual void OnCompletion(bool bSucceed) override;
	UAjaMediaCapture* Owner;
};

///* UAjaMediaCapture implementation
//*****************************************************************************/
UAjaMediaCapture::UAjaMediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OutputChannel(nullptr)
	, OutputCallback(nullptr)
	, bWaitForSyncEvent(false)
	, bEncodeTimecodeInTexel(false)
	, bSavedIgnoreTextureAlpha(false)
	, bIgnoreTextureAlphaChanged(false)
	, FrameRate(30, 1)
	, WakeUpEvent(nullptr)
	, LastFrameDropCount_AjaThread(0)
{
}

bool UAjaMediaCapture::ValidateMediaOutput() const
{
	UAjaMediaOutput* AjaMediaOutput = Cast<UAjaMediaOutput>(MediaOutput);
	if (!AjaMediaOutput)
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Can not start the capture. MediaSource's class is not supported."));
		return false;
	}

	if (AjaMediaOutput->OutputType == EAjaMediaOutputType::FillAndKey)
	{
		static const auto CVarPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
		EAlphaChannelMode::Type PropagateAlpha = EAlphaChannelMode::FromInt(CVarPropagateAlpha->GetValueOnGameThread());
		if (PropagateAlpha != EAlphaChannelMode::AllowThroughTonemapper)
		{

			UE_LOG(LogAjaMediaOutput, Error, TEXT("Can not start the capture. For key, 'Enable alpha channel support in post-processing' must be set to 'Allow through tonemapper'"));
			return false;
		}
	}

	return true;
}

bool UAjaMediaCapture::CaptureSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	UAjaMediaOutput* AjaMediaSource = CastChecked<UAjaMediaOutput>(MediaOutput);
	bool bResult = InitAJA(AjaMediaSource);
	if (bResult)
	{
		ApplyViewportTextureAlpha(InSceneViewport);
	}
	return bResult;
}

bool UAjaMediaCapture::CaptureRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	UAjaMediaOutput* AjaMediaSource = CastChecked<UAjaMediaOutput>(MediaOutput);
	bool bResult = InitAJA(AjaMediaSource);
	return bResult;
}

bool UAjaMediaCapture::UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	ApplyViewportTextureAlpha(InSceneViewport);
	return true;
}

bool UAjaMediaCapture::UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	return true;
}

void UAjaMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	if (!bAllowPendingFrameToBeProcess)
	{
		{
			// Prevent the rendering thread from copying while we are stopping the capture.
			FScopeLock ScopeLock(&RenderThreadCriticalSection);

			if (OutputChannel)
			{
				// Close the aja channel in the another thread.
				OutputChannel->Uninitialize();
				delete OutputChannel;
				OutputChannel = nullptr;
				delete OutputCallback;
				OutputCallback = nullptr;
			}

			if (WakeUpEvent)
			{
				FPlatformProcess::ReturnSynchEventToPool(WakeUpEvent);
				WakeUpEvent = nullptr;
			}
		}

		RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	}
}

void UAjaMediaCapture::ApplyViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport)
{
	if (InSceneViewport.IsValid())
	{
		TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
		if (Widget.IsValid())
		{
			bSavedIgnoreTextureAlpha = Widget->GetIgnoreTextureAlpha();

			UAjaMediaOutput* AjaMediaSource = CastChecked<UAjaMediaOutput>(MediaOutput);
			if (AjaMediaSource->OutputType == EAjaMediaOutputType::FillAndKey)
			{
				if (bSavedIgnoreTextureAlpha)
				{
					bIgnoreTextureAlphaChanged = true;
					Widget->SetIgnoreTextureAlpha(false);
				}
			}
		}
	}
}

void UAjaMediaCapture::RestoreViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport)
{
	// restore the ignore texture alpha state
	if (bIgnoreTextureAlphaChanged)
	{
		if (InSceneViewport.IsValid())
		{
			TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
			if (Widget.IsValid())
			{
				Widget->SetIgnoreTextureAlpha(bSavedIgnoreTextureAlpha);
			}
		}
		bIgnoreTextureAlphaChanged = false;
	}
}

bool UAjaMediaCapture::HasFinishedProcessing() const
{
	return Super::HasFinishedProcessing() || OutputChannel == nullptr;
}

bool UAjaMediaCapture::InitAJA(UAjaMediaOutput* InAjaMediaOutput)
{
	check(InAjaMediaOutput);

	// Init general settings
	bWaitForSyncEvent = InAjaMediaOutput->bWaitForSyncEvent;
	bEncodeTimecodeInTexel = InAjaMediaOutput->bEncodeTimecodeInTexel;
	FrameRate = InAjaMediaOutput->GetMediaMode().FrameRate;
	PortName = InAjaMediaOutput->FillPort.ToString();

	// Init Device options
	AJA::AJADeviceOptions DeviceOptions(InAjaMediaOutput->FillPort.DeviceIndex);

	OutputCallback = new UAjaMediaCapture::FAjaOutputCallback();
	OutputCallback->Owner = this;

	// Init Channel options
	AJA::AJAInputOutputChannelOptions ChannelOptions(TEXT("ViewportOutput"), InAjaMediaOutput->FillPort.PortIndex);
	ChannelOptions.CallbackInterface = OutputCallback;
	ChannelOptions.bOutput = true;
	ChannelOptions.NumberOfAudioChannel = 0;
	ChannelOptions.SynchronizeChannelIndex = InAjaMediaOutput->SyncPort.PortIndex;
	ChannelOptions.OutputKeyChannelIndex = InAjaMediaOutput->KeyPort.PortIndex;
	ChannelOptions.OutputNumberOfBuffers = InAjaMediaOutput->NumberOfAJABuffers;
	ChannelOptions.VideoFormatIndex = InAjaMediaOutput->GetMediaMode().VideoFormatIndex;
	ChannelOptions.bUseAutoCirculating = InAjaMediaOutput->bOutputWithAutoCirculating;
	ChannelOptions.bOutputKey = InAjaMediaOutput->OutputType == EAjaMediaOutputType::FillAndKey;  // must be RGBA to support Fill+Key
	ChannelOptions.bUseAncillary = false;
	ChannelOptions.bUseAudio = false;
	ChannelOptions.bUseVideo = true;
	ChannelOptions.bOutputInterlacedFieldsTimecodeNeedToMatch = InAjaMediaOutput->bInterlacedFieldsTimecodeNeedToMatch;

	switch (InAjaMediaOutput->PixelFormat)
	{
	case EAjaMediaOutputPixelFormat::PF_8BIT_ARGB:
		ChannelOptions.PixelFormat = AJA::EPixelFormat::PF_8BIT_ARGB;
		EncodePixelFormat = EMediaIOCoreEncodePixelFormat::CharBGRA;
		break;
	case EAjaMediaOutputPixelFormat::PF_10BIT_RGB:
		ChannelOptions.PixelFormat = AJA::EPixelFormat::PF_10BIT_RGB;
		EncodePixelFormat = EMediaIOCoreEncodePixelFormat::A2B10G10R10;
		break;
	default:
		ChannelOptions.PixelFormat = AJA::EPixelFormat::PF_10BIT_RGB;
		EncodePixelFormat = EMediaIOCoreEncodePixelFormat::A2B10G10R10;
		break;
	}

	switch (InAjaMediaOutput->TimecodeFormat)
	{
	case EAjaMediaTimecodeFormat::None:
		ChannelOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
		break;
	case EAjaMediaTimecodeFormat::LTC:
		ChannelOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_LTC;
		break;
	case EAjaMediaTimecodeFormat::VITC:
		ChannelOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_VITC1;
		break;
	default:
		ChannelOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
		break;
	}

	switch(InAjaMediaOutput->OutputReference)
	{
	case EAjaMediaOutputReferenceType::External:
		ChannelOptions.OutputReferenceType = AJA::EAJAReferenceType::EAJA_REFERENCETYPE_EXTERNAL;
		break;
	case EAjaMediaOutputReferenceType::Input:
		ChannelOptions.OutputReferenceType = AJA::EAJAReferenceType::EAJA_REFERENCETYPE_INPUT;
		break;
	default:
		ChannelOptions.OutputReferenceType = AJA::EAJAReferenceType::EAJA_REFERENCETYPE_FREERUN;
		break;
	}

	OutputChannel = new AJA::AJAOutputChannel();
	if (!OutputChannel->Initialize(DeviceOptions, ChannelOptions))
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("The AJA output port for '%s' could not be opened."), *InAjaMediaOutput->GetName());
		delete OutputChannel;
		OutputChannel = nullptr;
		delete OutputCallback;
		OutputCallback = nullptr;
		return false;
	}

	if (bWaitForSyncEvent)
	{
		const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
		if (bLockToVsync)
		{
			UE_LOG(LogAjaMediaOutput, Warning, TEXT("The Engine use VSync and '%s' wants to wait for the sync event. This may break the \"gen-lock\"."));
		}

		const bool bIsManualReset = false;
		WakeUpEvent = FPlatformProcess::GetSynchEventFromPool(bIsManualReset);
	}

	return true;
}

void UAjaMediaCapture::OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData> InUserData, void* InBuffer, int32 Width, int32 Height)
{
	// Prevent the rendering thread from copying while we are stopping the capture.
	FScopeLock ScopeLock(&RenderThreadCriticalSection);
	if (OutputChannel)
	{
		AJA::FTimecode Timecode = AjaMediaCaptureDevice::ConvertToAJATimecode(InBaseData.SourceFrameTimecode, FrameRate.AsDecimal());

		if (bEncodeTimecodeInTexel)
		{
			FMediaIOCoreEncodeTime EncodeTime(EncodePixelFormat, InBuffer, Width, Height);
			EncodeTime.Render(0, 0, Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
		}

		AJA::AJAOutputFrameBufferData FrameBuffer;
		FrameBuffer.Timecode = Timecode;
		FrameBuffer.FrameIdentifier = InBaseData.SourceFrameNumberRenderThread;
		OutputChannel->SetVideoFrameData(FrameBuffer, reinterpret_cast<uint8_t*>(InBuffer), Width * Height * 4);

		WaitForSync_RenderingThread();
	}
	else if (MediaState != EMediaCaptureState::Stopped)
	{
		MediaState = EMediaCaptureState::Error;
	}
}

void UAjaMediaCapture::WaitForSync_RenderingThread() const
{
	if (bWaitForSyncEvent)
	{
		if (WakeUpEvent && MediaState != EMediaCaptureState::Error) // In render thread, could be shutdown in a middle of a frame
		{
			WakeUpEvent->Wait();
		}
	}
}

/* namespace IAJAInputCallbackInterface implementation
// This is called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
*****************************************************************************/
void UAjaMediaCapture::FAjaOutputCallback::OnInitializationCompleted(bool bSucceed)
{
	check(Owner);
	if (Owner->MediaState != EMediaCaptureState::Stopped)
	{
		Owner->MediaState = bSucceed ? EMediaCaptureState::Capturing : EMediaCaptureState::Error;
	}

	if (Owner->WakeUpEvent)
	{
		Owner->WakeUpEvent->Trigger();
	}
}

bool UAjaMediaCapture::FAjaOutputCallback::OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData)
{
	const uint32 FrameDropCount = InFrameData.FramesDropped;
	if (FrameDropCount > Owner->LastFrameDropCount_AjaThread)
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("Lost %d frames on Aja output %s. Frame rate may be too slow."), FrameDropCount - Owner->LastFrameDropCount_AjaThread, *Owner->PortName);
	}
	Owner->LastFrameDropCount_AjaThread = FrameDropCount;

	return true;
}

void UAjaMediaCapture::FAjaOutputCallback::OnOutputFrameStarted()
{
	if (Owner->WakeUpEvent)
	{
		Owner->WakeUpEvent->Trigger();
	}
}

void UAjaMediaCapture::FAjaOutputCallback::OnCompletion(bool bSucceed)
{
	if (!bSucceed)
	{
		Owner->MediaState = EMediaCaptureState::Error;
	}

	if (Owner->WakeUpEvent)
	{
		Owner->WakeUpEvent->Trigger();
	}
}

bool UAjaMediaCapture::FAjaOutputCallback::OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame)
{
	check(false);
	return false;
}
