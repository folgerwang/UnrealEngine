// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaPlayer.h"
#include "BlackmagicMediaPrivate.h"

#include "HAL/PlatformProcess.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"

#include "MediaIOCoreSamples.h"
#include "MediaIOCoreEncodeTime.h"

#include "Misc/ScopeLock.h"

#include "Engine/GameEngine.h"
#include "Slate/SceneViewport.h"

#include "BlackmagicMediaAudioSample.h"
#include "BlackmagicMediaSettings.h"
#include "BlackmagicMediaSource.h"
#include "BlackmagicMediaTextureSample.h"

#include "BlackmagicMediaAllowPlatformTypes.h"

#define LOCTEXT_NAMESPACE "BlackmagicMediaPlayer"

namespace BlackMagicMediaPlayerHelpers
{
	FTimespan ConvertTimecode2Timespan(const BlackmagicDevice::FTimecode& InTimecode, const BlackmagicDevice::FTimecode& PreviousTimecode, const FTimespan& PreviousTimespan, const FFrameRate& InFPS)
	{
		check(InFPS.IsValid());

		//With FrameRate faster than 30FPS, max frame number will still be small than 30
		//Get by how much we need to divide the actual count.
		const float FrameRate = InFPS.AsDecimal();
		const float DividedFrameRate = FrameRate > 30.0f ? (FrameRate * 30.0f) / FrameRate : FrameRate;

		FTimespan NewTimespan;
		if (PreviousTimecode == InTimecode)
		{
			NewTimespan = PreviousTimespan + FTimespan::FromSeconds(InFPS.AsInterval());
		}
		else
		{
			NewTimespan = FTimespan(0, InTimecode.Hours, InTimecode.Minutes, InTimecode.Seconds, static_cast<int32>((ETimespan::TicksPerSecond * InTimecode.Frames) / DividedFrameRate) * ETimespan::NanosecondsPerTick);
		}
		return NewTimespan;
	}
}

//~ BlackMagicDevice::IPortCallbackInterface implementation
//--------------------------------------------------------------------
// Those are called from the Blackmagic thread. There's a lock inside the Blackmagic layer
// to prevent this object from dying while in this thread.

struct FBlackmagicMediaPlayer::FCallbackHandler : public BlackmagicDevice::IPortCallback
{
	FCallbackHandler(FBlackmagicMediaPlayer* InOwner)
		: Owner(InOwner)
	{
	}

	//~ BlackmagicDevice::IPortCallback interface
	virtual void OnInitializationCompleted(bool bSucceed) override
	{
	}
	virtual bool OnFrameArrived(BlackmagicDevice::FFrame InFrame)
	{
		return Owner->OnFrameArrived(InFrame);
	}

protected:
	FBlackmagicMediaPlayer* Owner;
};

/* FBlackmagicVideoPlayer structors
*****************************************************************************/

FBlackmagicMediaPlayer::FBlackmagicMediaPlayer(IMediaEventSink& InEventSink)
	: Super(InEventSink)
	, bUseFrameTimecode(false)
	, bEncodeTimecodeInTexel(false)
	, bIsOpen(false)
	, AudioSamplePool(new FBlackmagicMediaAudioSamplePool)
	, CaptureStyle(EBlackmagicMediaCaptureStyle::AudioVideo)
	, BmThread_AudioChannels(0)
	, BmThread_AudioSampleRate(0)
	, Device(nullptr)
	, Port(nullptr)
{
}

FBlackmagicMediaPlayer::~FBlackmagicMediaPlayer()
{
	Close();
	delete AudioSamplePool;
	AudioSamplePool = nullptr;
}

/* IMediaPlayer interface
*****************************************************************************/

void FBlackmagicMediaPlayer::Close()
{
	bIsOpen = false;

	if (Port && Device)
	{
		FScopeLock Lock(&CriticalSection);

		if (Port)
		{
			if (CallbackHandler)
			{
				Port->SetCallback(nullptr);
				delete CallbackHandler;
				CallbackHandler = nullptr;
			}

			Port->Release();
			Port = nullptr;
		}

		if (Device != nullptr)
		{
			BlackmagicDevice::VideoIOReleaseDevice(Device);
			Device = nullptr;
		}
	}

	AudioSamplePool->Reset();
	DeviceSource = FBlackmagicMediaPort();

	Super::Close();
}

FName FBlackmagicMediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("BlackmagicMedia"));
	return PlayerName;
}


FString FBlackmagicMediaPlayer::GetUrl() const
{
	return DeviceSource.ToUrl();
}


bool FBlackmagicMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	if (!Super::Open(Url, Options))
	{
		return false;
	}

	if (!DeviceSource.FromUrl(Url, false))
	{
		return false;
	}

	Device = BlackmagicDevice::VideoIOCreateDevice(DeviceSource.DeviceIndex);

	if (Device == nullptr)
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("Can't aquire the Blackmagic device."));
		return false;
	}

	// Read options
	bUseFrameTimecode = Options->GetMediaOption(BlackmagicMedia::UseTimecodeOption, false);
	bEncodeTimecodeInTexel = bUseFrameTimecode && Options->GetMediaOption(BlackmagicMedia::EncodeTimecodeInTexel, false);

	CaptureStyle = EBlackmagicMediaCaptureStyle(Options->GetMediaOption(BlackmagicMedia::CaptureStyleOption, (int64)EBlackmagicMediaCaptureStyle::AudioVideo));

	BlackmagicDevice::FPortOptions PortOptions;
	FMemory::Memset(&PortOptions, 0, sizeof(PortOptions));

	if (bUseFrameTimecode)
	{
		PortOptions.bUseTimecode = true;
	}

	if (CaptureStyle == EBlackmagicMediaCaptureStyle::AudioVideo)
	{
		PortOptions.bUseAudio = true;
		PortOptions.AudioChannels = Options->GetMediaOption(BlackmagicMedia::AudioChannelOption, (int64)2);
	}
	PortOptions.bUseVideo = true;
	PortOptions.bUseCallback = !Options->GetMediaOption(BlackmagicMedia::UseStreamBufferOption, false);


	int32 NumFrameBufferOptions = Options->GetMediaOption(BlackmagicMedia::NumFrameBufferOption, (int64)8);
	NumFrameBufferOptions = FMath::Clamp(NumFrameBufferOptions, 2, 16);
	PortOptions.FrameBuffers = NumFrameBufferOptions;

	// Open Device port
	int32 PortIndex = DeviceSource.PortIndex;

	BlackmagicDevice::FUInt MediaMode = Options->GetMediaOption(BlackmagicMedia::MediaModeOption, (int64)0);

	if (!BlackmagicDevice::VideoIOModeFrameDesc(MediaMode, FrameDesc))
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("bad mode (%d), default to default."), MediaMode);
	}

	VideoSampleFormat = (FrameDesc.PixelFormat == BlackmagicDevice::EPixelFormat::PF_ARGB) ? EMediaTextureSampleFormat::CharBGRA : EMediaTextureSampleFormat::CharUYVY;

	Port = BlackmagicDevice::VideoIODeviceOpenSharedPort(Device, PortIndex, FrameDesc, PortOptions);
	
	// match, so we will update when the actual mode arrives
	LastFrameDesc = FrameDesc;

	// Configure the audio supported
	AudioTrackFormat.BitsPerSample = 32;
	AudioTrackFormat.NumChannels = 0;
	AudioTrackFormat.SampleRate = 48000;
	AudioTrackFormat.TypeName = TEXT("PCM");

	// Configure the video supported
	VideoTrackFormat.Dim = FIntPoint(FrameInfo.Width, FrameInfo.Height);
	VideoTrackFormat.FrameRate = VideoFrameRate.AsDecimal();
	VideoTrackFormat.FrameRates = TRange<float>(VideoFrameRate.AsDecimal());
	VideoTrackFormat.TypeName = FString();
	
	if (Port)
	{
		BlackmagicDevice::VideoIOFrameDesc2Info(FrameDesc, FrameInfo);
		VideoFrameRate = FFrameRate(FrameInfo.TimeScale, FrameInfo.TimeValue);
	}
	else
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The port couldn't be opened."));
		return false;
	}

	LastFrameDropCount = Port->FrameDropCount();
	
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);

	CallbackHandler = new FCallbackHandler(this);
	Port->SetCallback(CallbackHandler);

	bIsOpen = true;
	return true;
}

void FBlackmagicMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	// update player state
	const EMediaState State = IsHardwareReady() ? EMediaState::Playing : EMediaState::Preparing;

	if (State != CurrentState)
	{
		CurrentState = State;
		EventSink.ReceiveMediaEvent(State == EMediaState::Playing ? EMediaEvent::PlaybackResumed : EMediaEvent::PlaybackSuspended);
	}

	if (CurrentState != EMediaState::Playing)
	{
		return;
	}

	// Don't update unless changed
	// (operator != is not defined for Blackmagic::FrameDesc)
	if (!(FrameDesc == LastFrameDesc))
	{
		FrameDesc = LastFrameDesc;
		// update the capture format
		BlackmagicDevice::VideoIOFrameDesc2Info(FrameDesc, FrameInfo);
		VideoFrameRate = FFrameRate(FrameInfo.TimeScale, FrameInfo.TimeValue);
		VideoTrackFormat.Dim = FIntPoint(FrameInfo.Width, FrameInfo.Height);
		VideoTrackFormat.FrameRate = VideoFrameRate.AsDecimal();
		VideoTrackFormat.FrameRates = TRange<float>(VideoFrameRate.AsDecimal());

		static const int ModeNameLength = 64;
		TCHAR ModeName[ModeNameLength];
		BlackmagicDevice::VideoIOFrameDesc2Name(FrameDesc, ModeName, ModeNameLength);
		VideoTrackFormat.TypeName = FString(ModeName);
	}

	AudioTrackFormat.NumChannels = BmThread_AudioChannels;
	AudioTrackFormat.SampleRate = BmThread_AudioSampleRate;

	TickTimeManagement();
}

void FBlackmagicMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	if (IsHardwareReady())
	{
		ProcessFrame();
		VerifyFrameDropCount();
	}
}

/* FBlackmagicMediaPlayer implementation
*****************************************************************************/

bool FBlackmagicMediaPlayer::DeliverFrame(BlackmagicDevice::FFrame InFrame)
{
	bool bReturn = false;

	if (InFrame)
	{
		if (CurrentState == EMediaState::Playing)
		{
			FTimespan DecodedTime = FTimespan::FromSeconds(FPlatformTime::Seconds());
			if (bUseFrameTimecode)
			{
				BlackmagicDevice::FTimecode Timecode;
				BlackmagicDevice::VideoIOFrameTimecode(InFrame, Timecode);
				DecodedTime = BlackMagicMediaPlayerHelpers::ConvertTimecode2Timespan(Timecode, PreviousFrameTimecode, PreviousFrameTimespan, VideoFrameRate);

				//Previous frame Timecode and Timespan are used to cover the facts that FrameTimecode FrameNumber is capped at 30 even for higher FPS.
				PreviousFrameTimecode = Timecode;
				PreviousFrameTimespan = DecodedTime;
			}
			{
				BlackmagicDevice::VideoIOFrameDesc(InFrame, LastFrameDesc);
			}

			if (bUseFrameTimecode && !bUseTimeSynchronization)
			{
				CurrentTime = DecodedTime;
			}

			if (CaptureStyle == EBlackmagicMediaCaptureStyle::AudioVideo)
			{
				auto AudioSample = AudioSamplePool->AcquireShared();
				if (AudioSample->Initialize(InFrame, DecodedTime))
				{
					BmThread_AudioChannels = AudioSample->GetChannels();
					BmThread_AudioSampleRate = AudioSample->GetSampleRate();
					Samples->AddAudio(AudioSample);
				}
			}

			auto TextureSample = MakeShared<FBlackmagicMediaTextureSample, ESPMode::ThreadSafe>();

			if (TextureSample->Initialize(InFrame, VideoSampleFormat, DecodedTime))
			{
				LastVideoDim = TextureSample->GetDim();

				if (bEncodeTimecodeInTexel && bUseFrameTimecode)
				{
					void* PixelBuffer = const_cast<void*>(TextureSample->GetBuffer());
					EMediaIOCoreEncodePixelFormat EncodePixelFormat = (VideoSampleFormat == EMediaTextureSampleFormat::CharBGRA) ? EMediaIOCoreEncodePixelFormat::CharBGRA : EMediaIOCoreEncodePixelFormat::CharUYVY;
					FMediaIOCoreEncodeTime EncodeTime(EncodePixelFormat, PixelBuffer, LastVideoDim.X, LastVideoDim.Y);
					EncodeTime.Render(0, 0, PreviousFrameTimecode.Hours, PreviousFrameTimecode.Minutes, PreviousFrameTimecode.Seconds, PreviousFrameTimecode.Frames);
				}

				Samples->AddVideo(TextureSample);
				bReturn = true;
			}
		}
	}
	return bReturn;
}

void FBlackmagicMediaPlayer::ProcessFrame()
{
	while (IsHardwareReady() && Port->PeekFrame())
	{
		BlackmagicDevice::FFrame Frame = Port->WaitFrame();

		if (!DeliverFrame(Frame))
		{
			BlackmagicDevice::VideoIOReleaseFrame(Frame);
		}
	}
}

bool FBlackmagicMediaPlayer::OnFrameArrived(BlackmagicDevice::FFrame InFrame)
{
	return DeliverFrame(InFrame);
}

void FBlackmagicMediaPlayer::VerifyFrameDropCount()
{
	if (IsHardwareReady())
	{
		const uint32 FrameDropCount = Port->FrameDropCount();
		if (FrameDropCount > LastFrameDropCount)
		{
			UE_LOG(LogBlackmagicMedia, Warning, TEXT("Lost %d frames on input %s. Frame rate is either too slow or buffering capacity is too small."), FrameDropCount - LastFrameDropCount, *DeviceSource.ToString());
		}
		LastFrameDropCount = FrameDropCount;
	}
}

bool FBlackmagicMediaPlayer::IsHardwareReady() const
{
	return (Device && Port && bIsOpen);
}

#undef LOCTEXT_NAMESPACE

#include "BlackmagicMediaHidePlatformTypes.h"
