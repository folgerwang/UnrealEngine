// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "stdafx.h"

namespace BlackmagicDevice
{

	/*
	 * Open a port on a device for input/output (C Style interface)
	 */

	PrivateInputCallback::PrivateInputCallback(PrivatePort* InPort)
		: Port(InPort)
	{
		AddRef();
	}

	ULONG PrivateInputCallback::AddRef()
	{
		++Count;
		return Count;
	}

	ULONG PrivateInputCallback::Release()
	{
		ULONG LocalCount = --Count;
		if (!LocalCount)
		{
			delete this;
		}
		return LocalCount;
	}

	HRESULT PrivateInputCallback::QueryInterface(REFIID InRIID, void **OutObject)
	{
		return E_FAIL;
	}

	HRESULT PrivateInputCallback::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents InNotificationEvents, IDeckLinkDisplayMode *InNewDisplayMode, BMDDetectedVideoInputFormatFlags InDetectedSignalFlags)
	{
		return Port->VideoInputFormatChanged(InNotificationEvents, InNewDisplayMode, InDetectedSignalFlags);
	}

	HRESULT PrivateInputCallback::VideoInputFrameArrived(IDeckLinkVideoInputFrame* InVideoFrame, IDeckLinkAudioInputPacket* InAudioPacket)
	{
		return Port->VideoInputFrameArrived(InVideoFrame, InAudioPacket);
	}

	/*
	 * Exported to allow the frame capture to call back
	 */

	PrivateOutputCallback::PrivateOutputCallback(PrivatePort* InPort)
		: Port(InPort)
	{
		AddRef();
	}

	ULONG PrivateOutputCallback::AddRef()
	{
		++Count;
		return Count;
	}

	ULONG PrivateOutputCallback::Release()
	{
		ULONG LocalCount = --Count;
		if (!LocalCount)
		{
			delete this;
		}
		return LocalCount;
	}

	HRESULT PrivateOutputCallback::QueryInterface(REFIID InRIID, void **OutObject)
	{
		return E_FAIL;
	}

	HRESULT PrivateOutputCallback::ScheduledFrameCompleted(IDeckLinkVideoFrame* InCompletedFrame, BMDOutputFrameCompletionResult InResult)
	{
		return Port->ScheduledFrameCompleted();
	}

	HRESULT PrivateOutputCallback::ScheduledPlaybackHasStopped()
	{
		return E_FAIL;
	}

	/*
	 * Input/Output Video port (C++ Object)
	 */

	PrivatePort::PrivatePort(PrivateDevice* InDevice, FUInt InPortIndex, bool InInput)
		: Started(0)
		, bInitializationCompleted(false)
		, AudioChannels(2)
		, AudioSampleRate(48000)
		, Input(InInput)
		, Output(!Input)
		, Device(InDevice)
		, PortIndex(InPortIndex)
		, SupportedDesc(nullptr)
	{
		AddRef();
	}

	PrivatePort::~PrivatePort()
	{
		Device->ReleasePort(this);
	}

	bool PrivatePort::Share(const FPortOptions& InOptions)
	{
		Thread::FAutoLock AutoLock(Lock);
		// can't share output
		if (Output)
		{
			LOG_ERROR(TEXT("Can't share output on port %d\n"), PortIndex);
			return false;
		}
		// using timecode
		if (InOptions.bUseTimecode)
		{
			Options.bUseTimecode = true;
		}
		// using video
		if (InOptions.bUseVideo || InOptions.bUseAudio)
		{
			if (Options.bUseVideo)
			{
				LOG_ERROR(TEXT("Can't share video frames on port %d\n"), PortIndex);
				return false;
			}
			Options.bUseVideo = true;
			Options.bUseAudio = true;
			Options.bUseCallback = InOptions.bUseCallback;
		}
		bInitializationCompleted = false;
		return true;
	}

	bool PrivatePort::Unshare(const FPortOptions& InOptions)
	{
		Thread::FAutoLock AutoLock(Lock);
		// can't share output
		if (Output)
		{
			LOG_ERROR(TEXT("Shoudn't share output on port %d\n"), PortIndex);
			return false;
		}
		// using video
		if (InOptions.bUseVideo)
		{
			Options.bUseVideo = false;
		}
		return true;
	}


	bool PrivatePort::Init(const FFrameDesc& InFrameDesc, const FPortOptions& InOptions)
	{
		// Sharing a port
		if (DeckLinkInput || DeckLinkOutput)
		{
			return Share(InOptions);
		}

		Thread::FAutoLock AutoLock(Lock);
		assert(Device);
		
		// Are we running

		Options = InOptions;
		FrameDesc = InFrameDesc;
		VideoIOFrameDesc2Info(FrameDesc, FrameInfo);

		if ((SupportedDesc = GetSupportedDescription(InFrameDesc)) == nullptr)
		{
			LOG_ERROR(TEXT("Unsupported mode %s %f\n"), FrameInfo.FormatName, FrameInfo.FrameRate);
			return false;
		}

		if (Input)
		{
			ComCheck(Device->QueryInterface(DeckLinkInput));

			InputFlags = bmdVideoInputFlagDefault;

			if (InFrameDesc.FrameFormat == EFrameFormat::FF_AUTO)
			{
				InputFlags |= bmdVideoInputEnableFormatDetection;
			}
			else
			{
				// validate mode against the hardware
				BMDDisplayModeSupport bSupported = bmdDisplayModeNotSupported;
				ComCheck(DeckLinkInput->DoesSupportVideoMode(SupportedDesc->DisplayMode, SupportedDesc->PixelFormat, bmdVideoInputFlagDefault, &bSupported, nullptr));
				if (bSupported == bmdDisplayModeNotSupported)
				{
					LOG_ERROR(TEXT("Invalid Frame Desciption, open port failed\n"));
					return false;
				}
			}

			// seems ok, lets use it
			ComCheck(DeckLinkInput->EnableVideoInput(SupportedDesc->DisplayMode, SupportedDesc->PixelFormat, InputFlags));

			// if using Audio
			AudioChannels = Options.AudioChannels;
			if (!(AudioChannels == 2
				|| AudioChannels == 8))
			{
				AudioChannels = 2;
				LOG_WARNING(TEXT("ConfigureAudio: Changed number of audio channel to %d.\n"), AudioChannels);
			}

			AudioSampleRate = 48000;
			ComCheck(DeckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType32bitInteger, AudioChannels));
		}
		else
		{
			// Try and take the output after configure the mode?
			ComCheck(Device->QueryInterface(DeckLinkOutput));

			// validate mode against the hardware
			BMDDisplayModeSupport bSupported = bmdDisplayModeNotSupported;
			ComCheck(DeckLinkOutput->DoesSupportVideoMode(SupportedDesc->DisplayMode, SupportedDesc->PixelFormat, bmdVideoOutputFlagDefault, &bSupported, nullptr));
			if (bSupported == bmdDisplayModeNotSupported)
			{
				DeckLinkOutput.Reset();
				return false;
			}

			// if bOutputKey, enable the output keyer
			if (Options.bOutputKey)
			{
				ComCheck(Device->QueryInterface(DeckLinkKeyer));
				DeckLinkKeyer->Enable(true);
				DeckLinkKeyer->SetLevel(255);
			}

			BMDVideoOutputFlags VideoOutputFlags = bmdVideoOutputFlagDefault;
			if (Options.bUseTimecode)
			{
				VideoOutputFlags = bmdVideoOutputRP188;
			}
			ComCheck(DeckLinkOutput->EnableVideoOutput(SupportedDesc->DisplayMode, VideoOutputFlags));
		}

		return true;
	}

	bool PrivatePort::Deinit(const FPortOptions& InOptions)
	{
		Unshare(InOptions);
		if (!Stop())
		{
			return false;
		}
		Thread::FAutoLock AutoLock(Lock);
		if (Input)
		{
			ComCheck(DeckLinkInput->DisableVideoInput());
			ComCheck(DeckLinkInput->SetCallback(nullptr));
			while (PeekFrame())
			{
				PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(WaitFrame());
				ReleaseFrame(Frame);
			}
			DeckLinkInput.Reset();
		}
		else
		{
			ComCheck(DeckLinkOutput->DisableVideoOutput());
			ComCheck(DeckLinkOutput->SetScheduledFrameCompletionCallback(nullptr));
			DeckLinkOutput.Reset();
		}
		return true;
	}

	FUInt PrivatePort::FrameSize()
	{
		return FrameInfo.Width * FrameInfo.Height * FrameInfo.BytesPerPixel;
	}

	FUInt PrivatePort::FrameDimensions(FUInt& OutWidth, FUInt& OutHeight)
	{
		OutWidth = FrameInfo.Width;
		OutWidth = FrameInfo.Height;
		return FrameInfo.Width * FrameInfo.Height * FrameInfo.BytesPerPixel;
	}

	bool PrivatePort::Start(FUInt InFrames)
	{
		Thread::FAutoLock AutoLock(Lock);
		++Started;
		if (Started != 1)
		{
			return true;
		}
		if (Input)
		{
			if (!Frames)
			{
				Frames = std::make_unique<PrivateFrame[]>(InFrames);
				// worst case 24fps, because the mode can change.
				FUInt FrameSamples = static_cast<FUInt>(AudioSampleRate / (24.0f - 1.0f));
				AudioFrames = std::make_unique<int32_t[]>(FrameSamples * AudioChannels * InFrames);

				for (FUInt i = 0; i < InFrames; ++i)
				{
					Frames[i].AudioFrame = &AudioFrames[i * FrameSamples * AudioChannels];
					Frames[i].AudioSamples = FrameSamples;
					Frames[i].ActiveAudioSamples = 0;
					FreeFrames.Send(&Frames[i]);
				}
			}
			// set the frame complete call back
			InputHandler = new PrivateInputCallback(this);
			ComCheck(DeckLinkInput->SetCallback(InputHandler));
			ComCheck(DeckLinkInput->FlushStreams());
			ComCheck(DeckLinkInput->StartStreams());
			return true;
		}
		else
		{
			// 60fps 
			OutputTime = 0;

			if (!Frames)
			{
				Frames = std::make_unique<PrivateFrame[]>(InFrames);
				for (FUInt i = 0; i < InFrames; ++i)
				{
					// This requires the resolution
					ComCheck(DeckLinkOutput->CreateVideoFrame(FrameInfo.Width, FrameInfo.Height, FrameInfo.Width*FrameInfo.BytesPerPixel, bmdFormat8BitBGRA, bmdFrameFlagDefault, Frames[i].DeckLinkMutableVideoFrame));
					FreeFrames.Send(&Frames[i]);
				}
			}

			// connect the callback
			OutputHandler = new PrivateOutputCallback(this);
			ComCheck(DeckLinkOutput->SetScheduledFrameCompletionCallback(OutputHandler));

			// Send first frame to start the pipe running
			PrivateFrame* Frame = LIST_LISTOF(PrivateFrame, MessageList, FreeFrames.Read());
			Frame->PrivatePort = this;
			InFlightFrames.Send(Frame);
			OutputTime += FrameInfo.TimeValue;
			ComCheck(DeckLinkOutput->ScheduleVideoFrame(Frame->DeckLinkMutableVideoFrame, OutputTime, FrameInfo.TimeValue, FrameInfo.TimeScale));
			ComCheck(DeckLinkOutput->StartScheduledPlayback(OutputTime, FrameInfo.TimeScale, 1.0));
			return true;
		}
		return false;
	}

	bool PrivatePort::Stop()
	{
		Thread::FAutoLock AutoLock(Lock);
		--Started;
		if (Started)
		{
			return false;
		}
		if (Input)
		{
			ComCheck(DeckLinkInput->StopStreams());
		}
		else
		{
			ComCheck(DeckLinkOutput->StopScheduledPlayback(OutputTime + FrameInfo.TimeValue, nullptr, FrameInfo.TimeScale));
		}
		return true;
	}

	bool PrivatePort::WaitVSync()
	{
		Thread::FAutoLock Lock(VSyncLock);
		if (Input)
		{
			VSyncEvent.Wait(VSyncLock, 50);
			return true;
		}
		else
		{
			if (InFlightFrames.Peek())
			{
				VSyncEvent.Wait(VSyncLock, 50);
				return true;
			}
		}
		return false;
	}

	bool PrivatePort::PeekFrame()
	{
		if (Input)
		{
			return FullFrames.Peek();
		}
		else
		{
			return FreeFrames.Peek();
		}
	}

	FFrame PrivatePort::WaitFrame()
	{
		if (Input)
		{
			PrivateFrame* Frame = LIST_LISTOF(PrivateFrame, MessageList, FullFrames.Read());
			Frame->PrivatePort = this;
			return Frame;
		}
		else
		{
			PrivateFrame* Frame = LIST_LISTOF(PrivateFrame, MessageList, FreeFrames.Read());
			Frame->PrivatePort = this;
			return Frame;
		}
	}

	void PrivatePort::ReleaseFrame(PrivateFrame* InFrame)
	{
		// Guard, as frame might hold last reference count
		RefPointer<PrivatePort> Port(InFrame->PrivatePort);
		if (Input)
		{
			InFrame->PrivatePort.Reset();
			InFrame->DeckLinkVideoInputFrame.Reset();
			Port->FreeFrames.Send(InFrame);
		}
		else
		{
			InFrame->PrivatePort.Reset();
			InFlightFrames.Send(InFrame);
			OutputTime += FrameInfo.TimeValue;
			if (Port->Options.bUseTimecode)
			{
				BMDTimecodeFlags Flags = bmdTimecodeFlagDefault;
				Flags += InFrame->Timecode.bField ? bmdTimecodeFieldMark : 0;
				Flags += InFrame->Timecode.bIsDropFrame ? bmdTimecodeIsDropFrame : 0;
				InFrame->DeckLinkMutableVideoFrame->SetTimecodeFromComponents(bmdTimecodeRP188LTC, InFrame->Timecode.Hours, InFrame->Timecode.Minutes, InFrame->Timecode.Seconds, InFrame->Timecode.Frames, Flags);
			}
			ComCheck(DeckLinkOutput->ScheduleVideoFrame(InFrame->DeckLinkMutableVideoFrame, OutputTime, FrameInfo.TimeValue, FrameInfo.TimeScale));
		}
	}

	FUInt PrivatePort::DropCount() const
	{
		return DroppedFrames;
	}

	bool PrivatePort::IsInput() const
	{
		return Input;
	}

	bool PrivatePort::IsOutput() const
	{
		return Output;
	}

	void PrivatePort::GetAudioFormat(FUInt& OutChannels, FUInt& OutSampleRate) const
	{
		OutChannels = AudioChannels;
		OutSampleRate = AudioSampleRate;
	}

	bool PrivatePort::IsPixelFormat(EPixelFormat InFormat) const
	{
		return FrameDesc.PixelFormat == InFormat;
	}

	FUInt PrivatePort::GetPortIndex() const
	{
		return PortIndex;
	}

	PrivateDevice* PrivatePort::GetDevice()
	{
		return Device;
	}

	bool PrivatePort::GetTimecode(FTimecode& OutTimecode) const
	{
		OutTimecode = Timecode;
		return bInitializationCompleted;
	}

	bool PrivatePort::AddCallback(IPortCallback* InCallback)
	{
		Thread::FAutoLock Lock(VSyncLock);
		PortCallbacks.push_back(InCallback);
		return true;
	}

	bool PrivatePort::RemCallback(IPortCallback* InCallback)
	{
		Thread::FAutoLock AutoLock(Lock);
		auto Callback = std::find_if(PortCallbacks.begin(), PortCallbacks.end(), [InCallback](IPortCallback* Callback) { return InCallback == Callback; });
		if (Callback == PortCallbacks.end())
		{
			return false;
		}
		PortCallbacks.erase(Callback);
		return true;
	}

	bool PrivatePort::InvokeInitializationCompleted(bool bSucceed)
	{
		Thread::FAutoLock AutoLock(Lock);
		for (auto i = PortCallbacks.begin(); i != PortCallbacks.end(); ++i)
		{
			(*i)->OnInitializationCompleted(bSucceed);
		}
		return true;
	}

	// Only one callback client can receive, and keep,
	// the frame, stop at first one that reports it will
	// keep it.
	bool PrivatePort::InvokeOnFrameArrived(FFrame InFrame)
	{
		Thread::FAutoLock AutoLock(Lock);
		for (auto i = PortCallbacks.begin(); i != PortCallbacks.end(); ++i)
		{
			if ((*i)->OnFrameArrived(InFrame))
			{
				return true;
			}
		}
		return false;
	}

	// Callback from capture card to deliver video/audio frames
	HRESULT PrivatePort::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents InNotificationEvents, IDeckLinkDisplayMode *InNewDisplayMode, BMDDetectedVideoInputFormatFlags InDetectedSignalFlags)
	{
		BMDDisplayMode DisplayMode = InNewDisplayMode->GetDisplayMode();

		if ((SupportedDesc = GetSupportedDescription(DisplayMode)) != nullptr) {
			FrameDesc = SupportedDesc->FrameDesc;
			VideoIOFrameDesc2Info(FrameDesc, FrameInfo);

			if (DeckLinkInput) {
				// Restart the video/audio
				ComCheck(DeckLinkInput->PauseStreams());
				ComCheck(DeckLinkInput->FlushStreams());

				ComCheck(DeckLinkInput->EnableVideoInput(SupportedDesc->DisplayMode, SupportedDesc->PixelFormat, InputFlags));
				if (Options.bUseAudio)
				{
					ComCheck(DeckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType32bitInteger, AudioChannels));
				}

				ComCheck(DeckLinkInput->StartStreams());
			}
		}
		else
		{
			LOG_ERROR(TEXT("Unsupported video input format"));
		}

		return S_OK;
	}

	// Callback from capture card to notify that input mode has changed
	HRESULT PrivatePort::VideoInputFrameArrived(IDeckLinkVideoInputFrame* InVideoFrame, IDeckLinkAudioInputPacket* InAudioPacket)
	{
		bool bHaveFrameTimecode = false;
		uint8_t Hours, Minutes, Seconds, Frames;

		if (InVideoFrame && !bInitializationCompleted)
		{
			bInitializationCompleted = true;
			InvokeInitializationCompleted(true);
		}

		if (InVideoFrame)
		{
			RefPointer<IDeckLinkTimecode> DeckLinkTimecode;
			HRESULT Error = InVideoFrame->GetTimecode(bmdTimecodeRP188LTC, DeckLinkTimecode);
			if (DeckLinkTimecode)
			{
				ComCheck(DeckLinkTimecode->GetComponents(&Hours, &Minutes, &Seconds, &Frames));
				Timecode.Hours = Hours;
				Timecode.Minutes = Minutes;
				Timecode.Seconds = Seconds;
				Timecode.Frames = Frames;

				// get the extra timecode flags
				BMDTimecodeFlags TimecodeFlags = DeckLinkTimecode->GetFlags();
				// we don't pull color framing here, its an analogue feature
				// that I don't think is needed anymore.
				Timecode.bField = (TimecodeFlags&bmdTimecodeFieldMark) != 0;
				Timecode.bIsDropFrame = (TimecodeFlags&bmdTimecodeIsDropFrame) != 0;
				bHaveFrameTimecode = true;
			}
			if (Options.bUseVideo && FreeFrames.Peek())
			{
				PrivateFrame* Frame = LIST_LISTOF(PrivateFrame, MessageList, FreeFrames.Read());
				Frame->PrivatePort = this;
				Frame->DeckLinkVideoInputFrame = InVideoFrame;

				if (InAudioPacket)
				{
					Frame->ActiveAudioSamples = InAudioPacket->GetSampleFrameCount();
					if (Frame->ActiveAudioSamples <= Frame->AudioSamples)
					{
						void *Buffer = nullptr;
						ComCheck(InAudioPacket->GetBytes(&Buffer));
						if (Buffer)
						{
							::memcpy(Frame->AudioFrame, Buffer, Frame->ActiveAudioSamples * AudioChannels * sizeof(int32_t));
						}
					}
					else
					{
						Frame->ActiveAudioSamples = 0;
					}
				}
				else
				{
					Frame->ActiveAudioSamples = 0;
				}

				if (bHaveFrameTimecode)
				{
					Frame->Timecode.Hours = Hours;
					Frame->Timecode.Minutes = Minutes;
					Frame->Timecode.Seconds = Seconds;
					Frame->Timecode.Frames = Frames;
				}
				{
					Frame->FrameDesc = FrameDesc;
				}

				// if no-one keeps the frame, we can free it
				if (Options.bUseCallback)
				{
					if (!InvokeOnFrameArrived(Frame))
					{
						Frame->PrivatePort.Reset();
						Frame->DeckLinkVideoInputFrame.Reset();
						FreeFrames.Send(Frame);
					}
				}
				else
				{
					FullFrames.Send(Frame);
				}
			}
			else
			{
				++DroppedFrames;
			}
		}
		VSyncEvent.Signal();
		return S_OK;
	}

	HRESULT PrivatePort::ScheduledFrameCompleted()
	{
		PrivateFrame* Frame = LIST_LISTOF(PrivateFrame, MessageList, InFlightFrames.Read());
		FreeFrames.Send(Frame);
		VSyncEvent.Signal();
		return S_OK;
	}
};