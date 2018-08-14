// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaPlayer.h"
#include "AjaMediaPrivate.h"

#include "AJA.h"
#include "MediaIOCoreSamples.h"
#include "MediaIOCoreEncodeTime.h"


#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformProcess.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "Misc/ScopeLock.h"

#include "AjaMediaAudioSample.h"
#include "AjaMediaBinarySample.h"
#include "AjaMediaSettings.h"
#include "AjaMediaTextureSample.h"

#include "AjaMediaAllowPlatformTypes.h"

#define LOCTEXT_NAMESPACE "FAjaMediaPlayer"

namespace AjaMediaPlayerConst
{
	static const uint32 ModeNameBufferSize = 64;
}

/* FAjaVideoPlayer structors
 *****************************************************************************/

FAjaMediaPlayer::FAjaMediaPlayer(IMediaEventSink& InEventSink)
	: Super(InEventSink)
	, AudioSamplePool(new FAjaMediaAudioSamplePool)
	, MetadataSamplePool(new FAjaMediaBinarySamplePool)
	, TextureSamplePool(new FAjaMediaTextureSamplePool)
	, MaxNumAudioFrameBuffer(8)
	, MaxNumMetadataFrameBuffer(8)
	, MaxNumVideoFrameBuffer(8)
	, AjaThreadNewState(EMediaState::Closed)
	, AjaThreadCurrentTime(FTimespan::Zero())
	, EventSink(InEventSink)
	, AjaThreadAudioChannels(0)
	, AjaThreadAudioSampleRate(0)
	, AjaLastVideoDim(FIntPoint::ZeroValue)
	, VideoFrameRate(30, 1)
	, AjaThreadFrameDropCount(0)
	, AjaThreadAutoCirculateAudioFrameDropCount(0)
	, AjaThreadAutoCirculateMetadataFrameDropCount(0)
	, AjaThreadAutoCirculateVideoFrameDropCount(0)
	, bEncodeTimecodeInTexel(false)
	, bUseAncillary(false)
	, bUseAudio(false)
	, bUseVideo(false)
	, bVerifyFrameDropCount(true)
	, VideoSampleFormat(EMediaTextureSampleFormat::CharBGRA)
	, InputChannel(nullptr)
	, AjaThreadPreviousFrameTimecode()
	, AjaThreadPreviousFrameTimespan(FTimespan::Zero())
{ }

FAjaMediaPlayer::~FAjaMediaPlayer()
{
	Close();
	delete AudioSamplePool;
	delete MetadataSamplePool;
	delete TextureSamplePool;
}


/* IMediaPlayer interface
 *****************************************************************************/
bool FAjaMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	if (!DeviceSource.FromUrl(Url, false))
	{
		return false;
	}

	if (!ReadMediaOptions(Options))
	{
		return false;
	}

	AJA::AJADeviceOptions DeviceOptions(DeviceSource.DeviceIndex);

	// Read options
	AJA::AJAInputOutputChannelOptions AjaOptions(TEXT("MediaPlayer"), DeviceSource.PortIndex);
	AjaOptions.CallbackInterface = this;
	AjaOptions.bOutput = false;
	{
		int32 Numerator, Denominator;
		Numerator = Options->GetMediaOption(AjaMediaOption::FrameRateNumerator, (int64)30);
		Denominator = Options->GetMediaOption(AjaMediaOption::FrameRateDenominator, (int64)1);
		VideoFrameRate = FFrameRate(Numerator, Denominator);
	}
	{
		EAjaMediaTimecodeFormat Timecode = (EAjaMediaTimecodeFormat)(Options->GetMediaOption(AjaMediaOption::TimecodeFormat, (int64)EAjaMediaTimecodeFormat::None));
		bUseFrameTimecode = Timecode != EAjaMediaTimecodeFormat::None;
		AjaOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
		switch (Timecode)
		{
		case EAjaMediaTimecodeFormat::None:
			AjaOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
			break;
		case EAjaMediaTimecodeFormat::LTC:
			AjaOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_LTC;
			break;
		case EAjaMediaTimecodeFormat::VITC:
			AjaOptions.TimecodeFormat = AJA::ETimecodeFormat::TCF_VITC1;
			break;
		default:
			break;
		}
		bEncodeTimecodeInTexel = bUseFrameTimecode && Options->GetMediaOption(AjaMediaOption::EncodeTimecodeInTexel, false);
	}
	{
		EAjaMediaAudioChannel AudioChannelOption = (EAjaMediaAudioChannel)(Options->GetMediaOption(AjaMediaOption::AudioChannel, (int64)EAjaMediaAudioChannel::Channel8));
		AjaOptions.NumberOfAudioChannel = (AudioChannelOption == EAjaMediaAudioChannel::Channel8) ? 8 : 6;
	}
	{
		AjaOptions.VideoFormatIndex = Options->GetMediaOption(AjaMediaOption::AjaVideoFormat, (int64)0);
		LastVideoFormatIndex = AjaOptions.VideoFormatIndex;
	}
	{
		EAjaMediaSourceColorFormat ColorFormat = (EAjaMediaSourceColorFormat)(Options->GetMediaOption(AjaMediaOption::ColorFormat, (int64)EMediaTextureSampleFormat::CharBGRA));
		switch(ColorFormat)
		{
		case EAjaMediaSourceColorFormat::UYVY:
			VideoSampleFormat = EMediaTextureSampleFormat::CharUYVY;
			AjaOptions.PixelFormat = AJA::EPixelFormat::PF_8BIT_YCBCR;
			break;
		case EAjaMediaSourceColorFormat::BGR10:
			VideoSampleFormat = EMediaTextureSampleFormat::CharBGR10A2;
			AjaOptions.PixelFormat = AJA::EPixelFormat::PF_10BIT_RGB;
			break;
		case EAjaMediaSourceColorFormat::BGRA:
		default:
			VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;
			AjaOptions.PixelFormat = AJA::EPixelFormat::PF_8BIT_ARGB;
			break;
		}
	}
	{
		AjaOptions.bUseAncillary = bUseAncillary = Options->GetMediaOption(AjaMediaOption::CaptureAncillary, false);
		AjaOptions.bUseAudio = bUseAudio = Options->GetMediaOption(AjaMediaOption::CaptureAudio, false);
		AjaOptions.bUseVideo = bUseVideo = Options->GetMediaOption(AjaMediaOption::CaptureVideo, true);
		AjaOptions.bUseAutoCirculating = Options->GetMediaOption(AjaMediaOption::CaptureWithAutoCirculating, true);
	}

	bVerifyFrameDropCount = Options->GetMediaOption(AjaMediaOption::LogDropFrame, true);
	MaxNumAudioFrameBuffer = Options->GetMediaOption(AjaMediaOption::MaxAudioFrameBuffer, (int64)8);
	MaxNumMetadataFrameBuffer = Options->GetMediaOption(AjaMediaOption::MaxAncillaryFrameBuffer, (int64)8);
	MaxNumVideoFrameBuffer = Options->GetMediaOption(AjaMediaOption::MaxVideoFrameBuffer, (int64)8);

	check(InputChannel == nullptr);
	InputChannel = new AJA::AJAInputChannel();
	if (!InputChannel->Initialize(DeviceOptions, AjaOptions))
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The Aja port couldn't be opened."));
		CurrentState = EMediaState::Error;
		AjaThreadNewState = EMediaState::Error;
		delete InputChannel;
		InputChannel = nullptr;
	}

	// configure format information for baseclass
	AudioTrackFormat.BitsPerSample = 32;
	AudioTrackFormat.NumChannels = 0;
	AudioTrackFormat.SampleRate = 48000;
	AudioTrackFormat.TypeName = FString(TEXT("PCM"));

	AudioTrackFormat.NumChannels = LastAudioChannels;
	AudioTrackFormat.SampleRate = LastAudioSampleRate;

	AJA::AJAVideoFormats::VideoFormatDescriptor FrameDescriptor = AJA::AJAVideoFormats::GetVideoFormat(LastVideoFormatIndex);
	VideoTrackFormat.Dim = FIntPoint(FrameDescriptor.Width, FrameDescriptor.Height);
	VideoTrackFormat.FrameRate = VideoFrameRate.AsDecimal();
	VideoTrackFormat.FrameRates = TRange<float>(VideoFrameRate.AsDecimal());
	VideoTrackFormat.TypeName = FrameDescriptor.FormatedText;

	// finalize
	CurrentState = EMediaState::Preparing;
	AjaThreadNewState = EMediaState::Preparing;
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaConnecting);

	return true;
}

void FAjaMediaPlayer::Close()
{
	CurrentState = EMediaState::Closed;
	AjaThreadNewState = EMediaState::Closed;

	if (InputChannel)
	{
		InputChannel->Uninitialize(); // this may block, until the completion of a callback from IAJAChannelCallbackInterface
		delete InputChannel;
		InputChannel = nullptr;
	}

	AudioSamplePool->Reset();
	MetadataSamplePool->Reset();
	TextureSamplePool->Reset();
	Samples->FlushSamples();

	CurrentTime = FTimespan::Zero();
	AjaThreadCurrentTime = FTimespan::Zero();

	DeviceSource = FAjaMediaPort();
	LastVideoDim = FIntPoint::ZeroValue;
	LastAudioChannels = 0;
	LastAudioSampleRate = 0;

	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}


FName FAjaMediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("AJAMedia"));
	return PlayerName;
}


FString FAjaMediaPlayer::GetStats() const
{
	FString Stats;

	Stats += TEXT("Aja settings\n");
	Stats += FString::Printf(TEXT("		Input port: %s\n"), *DeviceSource.ToString());
	Stats += FString::Printf(TEXT("		Frame rate: %s\n"), *VideoFrameRate.ToPrettyText().ToString());
	Stats += FString::Printf(TEXT("		  Aja Mode: %s\n"), *VideoTrackFormat.TypeName);

	Stats += TEXT("\n\n");
	Stats += TEXT("Status\n");
	
	if (bUseFrameTimecode)
	{
		//TODO This is not thread safe.
		Stats += FString::Printf(TEXT("		Newest Timecode: %02d:%02d:%02d:%02d\n"), AjaThreadPreviousFrameTimecode.Hours, AjaThreadPreviousFrameTimecode.Minutes, AjaThreadPreviousFrameTimecode.Seconds, AjaThreadPreviousFrameTimecode.Frames);
	}
	else
	{
		Stats += FString::Printf(TEXT("		Timecode: Not Enabled\n"));
	}

	if (bUseVideo)
	{
		Stats += FString::Printf(TEXT("		Buffered video frames: %d\n"), Samples->NumVideoSamples());
	}
	else
	{
		Stats += FString::Printf(TEXT("		Buffered video frames: Not enabled\n"));
	}
	
	if (bUseAudio)
	{
		Stats += FString::Printf(TEXT("		Buffered audio frames: %d\n"), Samples->NumAudioSamples());
	}
	else
	{
		Stats += FString::Printf(TEXT("		Buffered audio frames: Not enabled\n"));
	}
	
	Stats += FString::Printf(TEXT("		Frames dropped: %d"), LastFrameDropCount);

	return Stats;
}


FString FAjaMediaPlayer::GetUrl() const
{
	return DeviceSource.ToUrl();
}


void FAjaMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	if (InputChannel && CurrentState == EMediaState::Playing)
	{
		ProcessFrame();
		VerifyFrameDropCount();
	}
}


void FAjaMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	// update player state
	EMediaState NewState = AjaThreadNewState;
	
	if (NewState != CurrentState)
	{
		CurrentState = NewState;
		if (CurrentState == EMediaState::Playing)
		{
			EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
		}
		else if (NewState == EMediaState::Error)
		{
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			Close();
		}
	}

	if (CurrentState != EMediaState::Playing)
	{
		return;
	}

	if (TickTimeManagement() && !bUseFrameTimecode)
{
		// As default, use the App time
		CurrentTime = FTimespan::FromSeconds(FApp::GetCurrentTime());
}
}


/* FAjaMediaPlayer implementation
 *****************************************************************************/
void FAjaMediaPlayer::ProcessFrame()
{
	if (CurrentState == EMediaState::Playing)
	{
		//If Asset is setup to use time synchronization, use it only if it can provide a valid time.
		if (bUseFrameTimecode && !bUseTimeSynchronization)
		{
			// We want to atomically read the FTimespan.GetTick()
			static_assert(sizeof(AjaThreadCurrentTime) == sizeof(int64), "The size of a FTimespan is not a int64");
			static_assert(sizeof(AjaThreadCurrentTime.GetTicks()) == sizeof(int64), "The size of a FTimespan is not a int64");
			static_assert(TIsSame<decltype(CurrentTime), decltype(AjaThreadCurrentTime)>::Value, "The type of CurrentTime is not the same as AjaThreadCurrentTime");

			// Take the latest input as the CurrentTime
			FPlatformAtomics::InterlockedExchange(reinterpret_cast<int64*>(&CurrentTime), *reinterpret_cast<int64*>(&AjaThreadCurrentTime));
		}

		// No need to lock here. That info is only used for debug information.
		LastAudioChannels = AjaThreadAudioChannels;
		LastAudioSampleRate = AjaThreadAudioSampleRate;
	}
}

void FAjaMediaPlayer::VerifyFrameDropCount()
{
	if (bVerifyFrameDropCount)
	{
	uint32 FrameDropCount = AjaThreadFrameDropCount;
	if (FrameDropCount > LastFrameDropCount)
	{
			UE_LOG(LogAjaMedia, Warning, TEXT("Lost %d frames on Aja input %s. UE4 frame rate is too slow and the capture card was not able to send the frame(s) to UE4."), FrameDropCount - LastFrameDropCount, *DeviceSource.ToString());
	}
	LastFrameDropCount = FrameDropCount;

	FrameDropCount = FPlatformAtomics::InterlockedExchange(&AjaThreadAutoCirculateAudioFrameDropCount, 0);
	if (FrameDropCount > 0)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("Lost %d audio frames on Aja input %s. Frame rate is either too slow or buffering capacity is too small."), FrameDropCount, *DeviceSource.ToString());
	}

	FrameDropCount = FPlatformAtomics::InterlockedExchange(&AjaThreadAutoCirculateMetadataFrameDropCount, 0);
	if (FrameDropCount > 0)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("Lost %d metadata frames on Aja input %s. Frame rate is either too slow or buffering capacity is too small."), FrameDropCount, *DeviceSource.ToString());
	}

	FrameDropCount = FPlatformAtomics::InterlockedExchange(&AjaThreadAutoCirculateVideoFrameDropCount, 0);
	if (FrameDropCount > 0)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("Lost %d video frames on Aja input %s. Frame rate is either too slow or buffering capacity is too small."), FrameDropCount, *DeviceSource.ToString());
	}
}
}


/* IAJAInputOutputCallbackInterface implementation
// This is called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
*****************************************************************************/
void FAjaMediaPlayer::OnInitializationCompleted(bool bSucceed)
{
	if (bSucceed)
	{
		LastFrameDropCount = InputChannel->GetFrameDropCount();
	}
	AjaThreadNewState = bSucceed ? EMediaState::Playing : EMediaState::Error;
}


void FAjaMediaPlayer::OnCompletion(bool bSucceed)
{
	AjaThreadNewState = bSucceed ? EMediaState::Closed : EMediaState::Error;
}


bool FAjaMediaPlayer::OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& InAudioFrame, const AJA::AJAVideoFrameData& InVideoFrame)
{
	if (AjaThreadNewState != EMediaState::Playing && CurrentState != EMediaState::Playing)
	{
		return false;
	}

	AjaThreadFrameDropCount = InInputFrame.FramesDropped;

	FTimespan DecodedTime = FTimespan::FromSeconds(FApp::GetCurrentTime());
	
	if (bUseFrameTimecode)
	{
		DecodedTime = FAja::ConvertAJATimecode2Timespan(InInputFrame.Timecode, AjaThreadPreviousFrameTimecode, AjaThreadPreviousFrameTimespan, VideoFrameRate);

		//Previous frame Timecode and Timespan are used to cover the facts that AJAFrameTimecode FrameNumber is capped at 30 even for higher FPS.
		AjaThreadPreviousFrameTimecode = InInputFrame.Timecode;
		AjaThreadPreviousFrameTimespan = DecodedTime;

		if (bIsTimecodeLogEnable)
		{
			UE_LOG(LogAjaMedia, Log, TEXT("Aja input port %s has timecode : %02d:%02d:%02d:%02d"), *DeviceSource.ToString(), AjaThreadPreviousFrameTimecode.Hours, AjaThreadPreviousFrameTimecode.Minutes, AjaThreadPreviousFrameTimecode.Seconds, AjaThreadPreviousFrameTimecode.Frames);
		}
	}

	// We want to atomically set AjaThreadCurrentTime
	static_assert(sizeof(AjaThreadCurrentTime) == sizeof(int64), "The size of a FTimespan is not a int64");
	static_assert(sizeof(AjaThreadCurrentTime.GetTicks()) == sizeof(int64), "The size of a FTimespan is not a int64");
	static_assert(TIsSame<decltype(DecodedTime), decltype(AjaThreadCurrentTime)>::Value, "The type of DecodedTime is not the same as AjaThreadCurrentTime");

	// Take the latest input as the CurrentTime
	FPlatformAtomics::InterlockedExchange(reinterpret_cast<int64*>(&AjaThreadCurrentTime), *reinterpret_cast<int64*>(&DecodedTime));

	if (AjaThreadNewState == EMediaState::Playing)
	{
		if (bUseAncillary && InAncillaryFrame.AncBuffer)
		{
			const bool bHaveField2 = (InAncillaryFrame.AncF2Buffer && !InVideoFrame.bIsProgressivePicture);

			const int32 NumMetadataSamples = Samples->NumMetadataSamples() + (bHaveField2 ? 1 : 0);
			if (NumMetadataSamples >= MaxNumMetadataFrameBuffer)
			{
				FPlatformAtomics::InterlockedIncrement(&AjaThreadAutoCirculateMetadataFrameDropCount);
				Samples->PopMetadata();
				if (!InVideoFrame.bIsProgressivePicture)
				{
					Samples->PopMetadata();
				}
			}

			{
				auto MetaDataSample = MetadataSamplePool->AcquireShared();
				if (MetaDataSample->Initialize(InAncillaryFrame.AncBuffer, InAncillaryFrame.AncBufferSize, AjaThreadCurrentTime))
				{
					Samples->AddMetadata(MetaDataSample);
				}
			}

			if (bHaveField2)
			{
				FTimespan CurrentOddTime = AjaThreadCurrentTime + FTimespan::FromSeconds(VideoFrameRate.AsInterval() / 2.0);
				auto MetaDataSample = MetadataSamplePool->AcquireShared();
				if (MetaDataSample->Initialize(InAncillaryFrame.AncF2Buffer, InAncillaryFrame.AncF2BufferSize, AjaThreadCurrentTime))
				{
					Samples->AddMetadata(MetaDataSample);
				}
			}
		}

		if (bUseAudio && InAudioFrame.AudioBuffer)
		{
			if (Samples->NumAudioSamples() >= MaxNumAudioFrameBuffer)
			{
				FPlatformAtomics::InterlockedIncrement(&AjaThreadAutoCirculateAudioFrameDropCount);
				Samples->PopAudio();
			}

			auto AudioSample = AudioSamplePool->AcquireShared();
			if (AudioSample->Initialize(InAudioFrame, AjaThreadCurrentTime))
			{
				Samples->AddAudio(AudioSample);

				AjaThreadAudioChannels = AudioSample->GetChannels();
				AjaThreadAudioSampleRate = AudioSample->GetSampleRate();
			}
		}

		if (bUseVideo && InVideoFrame.VideoBuffer)
		{
			const int32 NumVideoSamples = Samples->NumVideoSamples() + (!InVideoFrame.bIsProgressivePicture ? 1 : 0);
			if (NumVideoSamples >= MaxNumVideoFrameBuffer)
			{
				FPlatformAtomics::InterlockedIncrement(&AjaThreadAutoCirculateVideoFrameDropCount);
				Samples->PopVideo();
				if (!InVideoFrame.bIsProgressivePicture)
				{
					Samples->PopVideo();
				}
			}

			auto TextureSample = TextureSamplePool->AcquireShared();
			bool bWasAdded = false;
			if (InVideoFrame.bIsProgressivePicture)
			{
				if (bEncodeTimecodeInTexel)
				{
					EMediaIOCoreEncodePixelFormat EncodePixelFormat = (VideoSampleFormat == EMediaTextureSampleFormat::CharBGRA) ? EMediaIOCoreEncodePixelFormat::CharBGRA : EMediaIOCoreEncodePixelFormat::CharUYVY;
					FMediaIOCoreEncodeTime EncodeTime(EncodePixelFormat, InVideoFrame.VideoBuffer, InVideoFrame.Width, InVideoFrame.Height);
					EncodeTime.Render(0, 0, AjaThreadPreviousFrameTimecode.Hours, AjaThreadPreviousFrameTimecode.Minutes, AjaThreadPreviousFrameTimecode.Seconds, AjaThreadPreviousFrameTimecode.Frames);
				}

				if (TextureSample->InitializeProgressive(InVideoFrame, VideoSampleFormat, AjaThreadCurrentTime))
				{
					Samples->AddVideo(TextureSample);
					bWasAdded = true;
				}
			}
			else
			{
				bool bEven = true;
				if (TextureSample->InitializeInterlaced_Halfed(InVideoFrame, VideoSampleFormat, AjaThreadCurrentTime, bEven))
				{
					Samples->AddVideo(TextureSample);
					bWasAdded = true;
				}

				auto TextureSampleOdd = TextureSamplePool->AcquireShared();
				FTimespan CurrentOddTime = AjaThreadCurrentTime + FTimespan::FromSeconds(VideoFrameRate.AsInterval() / 2.0);
				bEven = false;
				if (TextureSampleOdd->InitializeInterlaced_Halfed(InVideoFrame, VideoSampleFormat, CurrentOddTime, bEven))
				{
					Samples->AddVideo(TextureSampleOdd);
				}
			}

			if (bWasAdded)
			{
				AjaLastVideoDim = TextureSample->GetDim();
			}
		}
	}

	return true;
}


bool FAjaMediaPlayer::OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData)
{
	// this is not supported
	check(false);
	return false;
}

bool FAjaMediaPlayer::IsHardwareReady() const
{
	return CurrentState == EMediaState::Playing ? true : false;
}

#undef LOCTEXT_NAMESPACE

#include "AjaMediaHidePlatformTypes.h"
