// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Streamer.h"
#include "UnrealClient.h"
#include "HAL/Runnable.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/Async.h"

#include "NvVideoEncoder.h"
#include "PixelStreamingCommon.h"
#include "Utils.h"
#include "ProxyConnection.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("EncodingFramerate"), STAT_PixelStreaming_EncodingFramerate, STATGROUP_PixelStreaming);
DECLARE_DWORD_COUNTER_STAT(TEXT("EncodingBitrate"), STAT_PixelStreaming_EncodingBitrate, STATGROUP_PixelStreaming);

TAutoConsoleVariable<int32> CVarEncoderAverageBitRate(
	TEXT("Encoder.AverageBitRate"),
	20000000,
	TEXT("Encoder bit rate before reduction for B/W jitter"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarEncoderMaxBitrate(
	TEXT("Encoder.MaxBitrate"),
	100000000,
	TEXT("Max bitrate no matter what WebRTC says, in Mbps"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<FString> CVarEncoderTargetSize(
	TEXT("Encoder.TargetSize"),
	TEXT("1920x1080"),
	TEXT("Encoder target size in format widthxheight"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarEncoderUseBackBufferSize(
	TEXT("Encoder.UseBackBufferSize"),
	1,
	TEXT("Whether to use back buffer size or custom size"),
	ECVF_Cheat);

#if !UE_BUILD_SHIPPING
static int32 bEncoderSaveVideoToFile = 0;
static FAutoConsoleVariableRef CVarEncoderSaveVideoToFile(
	TEXT("Encoder.SaveVideoToFile"),
	bEncoderSaveVideoToFile,
	TEXT("Save encoded video into a file"),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

TAutoConsoleVariable<int32> CVarStreamerPrioritiseQuality(
	TEXT("Streamer.PrioritiseQuality"),
	0,
	TEXT("Reduces framerate automatically on bitrate reduction to trade FPS/latency for video quality"),
	ECVF_Cheat);

TAutoConsoleVariable<int32> CVarStreamerLowBitrate(
	TEXT("Streamer.LowBitrate"),
	2000,
	TEXT("Lower bound of bitrate for quality adaptation, Kbps"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarStreamerHighBitrate(
	TEXT("Streamer.HighBitrate"),
	10000,
	TEXT("Upper bound of bitrate for quality adaptation, Kbps"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarStreamerMinFPS(
	TEXT("Streamer.MinFPS"),
	10,
	TEXT("Minimal FPS for quality adaptation"),
	ECVF_Default);

TAutoConsoleVariable<float> CVarStreamerBitrateReduction(
	TEXT("Streamer.BitrateReduction"),
	50.0,
	TEXT("How much to reduce WebRTC reported bitrate to handle bitrate jitter, in per cent"),
	ECVF_RenderThreadSafe);

const int32 DefaultFPS = 60;

FStreamer::FStreamer(const TCHAR* IP, uint16 Port, const FTexture2DRHIRef& FrameBuffer)
	: bResizingWindowBackBuffer(false)
	, AudioEncoder(*this)
	, bSendSpsPps(false)
	, bStreamingStarted(false)
	, InitialMaxFPS(GEngine->GetMaxFPS())
{
	if (InitialMaxFPS == 0)
	{
		InitialMaxFPS = DefaultFPS;

		check(IsInRenderingThread());
		// we are in the rendering thread but `GEngine->SetMaxFPS()` can be called only in the main thread
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			GEngine->SetMaxFPS(InitialMaxFPS);
		});
	}

	ProxyConnection.Reset(new FProxyConnection(IP, Port, *this));

	UpdateEncoderSettings(FrameBuffer);
	CreateVideoEncoder(FrameBuffer);

	// This needs to be called last, after ProxyConnection is created
	AudioEncoder.Init();

	UE_LOG(PixelStreaming, Log, TEXT("Streamer created: %dx%d %d FPS%s"),
		VideoEncoderSettings.Width, VideoEncoderSettings.Height,
		InitialMaxFPS,
		CVarStreamerPrioritiseQuality.GetValueOnAnyThread() != 0 ? TEXT(", prioritise quality") : TEXT(""));
}

// must be in cpp file cos TUniquePtr incomplete type
// this doesn't violate The Rule of Zero: https://blog.rmf.io/cxx11/rule-of-zero cos we don't do any manual stuff
FStreamer::~FStreamer()
{
}

void FStreamer::CreateVideoEncoder(const FTexture2DRHIRef& FrameBuffer)
{
	VideoEncoder.Reset(new FNvVideoEncoder(VideoEncoderSettings, FrameBuffer, [this](uint64 Timestamp, bool KeyFrame, const uint8* Data, uint32 Size)
	{
		SubmitVideoFrame(Timestamp, KeyFrame, Data, Size);
	}));
		
	checkf(VideoEncoder->IsSupported(), TEXT("Failed to initialize NvEnc"));
	UE_LOG(PixelStreaming, Log, TEXT("NvEnc initialised"));
}

void FStreamer::SendSpsPpsHeader()
{
	const TArray<uint8>& SpsPps = VideoEncoder->GetSpsPpsHeader();
	Stream(FPlatformTime::Seconds(), PixelStreamingProtocol::EToProxyMsg::SpsPps, SpsPps.GetData(), SpsPps.Num());
}

void FStreamer::OnFrameBufferReady(const FTexture2DRHIRef& FrameBuffer)
{
	if (!bStreamingStarted)
	{
		return;
	}

	uint64 CaptureMs = NowMs();

	// VideoEncoder is reset on disconnection
	if (!VideoEncoder)
	{
		CreateVideoEncoder(FrameBuffer);
		check(VideoEncoder);
	}

	if (bResizingWindowBackBuffer)
	{
		// Re-initialize video encoder if it has been destroyed by OnPreResizeWindowBackbuffer()
		VideoEncoder->PostResizeBackBuffer();
		bResizingWindowBackBuffer = false;
	}

	UpdateEncoderSettings(FrameBuffer);
	VideoEncoder->EncodeFrame(VideoEncoderSettings, FrameBuffer, CaptureMs);
}

void FStreamer::SubmitVideoFrame(uint64 Timestamp, bool KeyFrame, const uint8* Data, uint32 Size)
{
	if (bSendSpsPps)
	{
		SendSpsPpsHeader();
		bSendSpsPps = false;
	}

	Stream(Timestamp, KeyFrame ? PixelStreamingProtocol::EToProxyMsg::VideoIDR : PixelStreamingProtocol::EToProxyMsg::Video, Data, Size);
}

void FStreamer::OnPreResizeWindowBackbuffer()
{
	// Destroy video encoder before resizing window so it releases usage of graphics device & back buffer.
	// It's recreated later on in OnFrameBufferReady().
	UE_LOG(PixelStreaming, Log, TEXT("Reset video encoder OnPreResizeWindowBackbuffer"));
	VideoEncoder->PreResizeBackBuffer();
	bResizingWindowBackBuffer = true;
}

// This is called from inside the audio encoder, when a audio packet is ready
void FStreamer::OnAudioPCMPacketReady(const uint8* Data, int Size)
{
	Stream(FPlatformTime::Seconds(), PixelStreamingProtocol::EToProxyMsg::AudioPCM, Data, Size);
}

void FStreamer::Stream(uint64 Timestamp, PixelStreamingProtocol::EToProxyMsg PktType, const uint8* Data, uint32 Size)
{
	FScopeLock Lock(&AudioVideoStreamSync);

	SaveEncodedVideoToFile(PktType, Data, Size);

	if (ProxyConnection->Send(reinterpret_cast<const uint8*>(&Timestamp), sizeof(Timestamp))
		&& ProxyConnection->Send(reinterpret_cast<const uint8*>(&PktType), 1)
		&& ProxyConnection->Send(reinterpret_cast<const uint8*>(&Size), sizeof(Size))
		&& ProxyConnection->Send(Data, Size))
	{
		static uint32 frameNo = 0;
		UE_LOG(PixelStreamingNet, Verbose, TEXT("Sent %s %d, %d bytes"), PacketTypeStr(PktType), frameNo++, Size);
	}
}

void FStreamer::SaveEncodedVideoToFile(PixelStreamingProtocol::EToProxyMsg PktType, const uint8* Data, uint32 Size)
{
#if !UE_BUILD_SHIPPING
	if (bEncoderSaveVideoToFile && !EncodedVideoFile)
	{
		// Open video file for writing
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		EncodedVideoFile.Reset(PlatformFile.OpenWrite(TEXT("EncodedVideoFile.h264")));
		check(EncodedVideoFile);
	}
	if (EncodedVideoFile && (PktType != PixelStreamingProtocol::EToProxyMsg::AudioPCM))
	{
		EncodedVideoFile->Write(Data, Size);
		EncodedVideoFile->Flush();
	}
	if (!bEncoderSaveVideoToFile && EncodedVideoFile)
	{
		// Close video file for writing
		EncodedVideoFile.Reset();
	}
#endif
}

void FStreamer::ForceIdrFrame()
{
	VideoEncoder->ForceIdrFrame();
}

void FStreamer::UpdateEncoderSettings(const FTexture2DRHIRef& FrameBuffer)
{
	float MaxBitrateMbps = CVarEncoderMaxBitrate.GetValueOnRenderThread();

	// HACK(andriy): We reduce WebRTC reported bitrate to compensate for B/W jitter. We have long pipeline
	// before passing encoded frames to WebRTC and a couple of frames are already in the pipeline when
	// WebRTC reports lower bitrate. This often causes that WebRTC Rate Limiter or network drop frames
	// because they exceed available bandwidth. While significant bandwidth drop are not expected to
	// happen often small jitter is possible and causes frequent video distortion. Reducing reported bitrate
	// by a small percentage gives us a chance to avoid frame drops on bandwidth jitter.
	// There're couple of drawbacks:
	// - minor one - we don't use all available bandwidth to achieve best possible quality
	// - major one - we don't use all available bandwidth and in case of network congestion
	// other connections can get upper hand and depress bandwidth allocated for streaming even more.
	// Proper feasible solution is unknown atm.
	//
	// do reduction here instead of e.g. `SetBitrate` because this method is called on every frame and so
	// changes to `CVarStreamerBitrateReduction` will be immediately picked up
	float BitrateReduction = CVarStreamerBitrateReduction.GetValueOnRenderThread();
	uint32 Bitrate = CVarEncoderAverageBitRate.GetValueOnRenderThread();
	uint32 ReducedBitrate = static_cast<uint32>(Bitrate / 100.0 * (100.0 - BitrateReduction));
	ReducedBitrate = FMath::Min(ReducedBitrate, static_cast<uint32>(MaxBitrateMbps * 1000 * 1000));
	VideoEncoderSettings.AverageBitRate = ReducedBitrate;
	SET_DWORD_STAT(STAT_PixelStreaming_EncodingBitrate, VideoEncoderSettings.AverageBitRate);

	VideoEncoderSettings.FrameRate = GEngine->GetMaxFPS();
	SET_DWORD_STAT(STAT_PixelStreaming_EncodingFramerate, VideoEncoderSettings.FrameRate);

	bool bUseBackBufferSize = CVarEncoderUseBackBufferSize.GetValueOnAnyThread() > 0;
	if (bUseBackBufferSize)
	{
		VideoEncoderSettings.Width = FrameBuffer->GetSizeX();
		VideoEncoderSettings.Height = FrameBuffer->GetSizeY();
	}
	else
	{
		FString EncoderTargetSize = CVarEncoderTargetSize.GetValueOnAnyThread();
		FString TargetWidth, TargetHeight;
		bool bValidSize = EncoderTargetSize.Split(TEXT("x"), &TargetWidth, &TargetHeight);
		if (bValidSize)
		{
			VideoEncoderSettings.Width = FCString::Atoi(*TargetWidth);
			VideoEncoderSettings.Height = FCString::Atoi(*TargetHeight);
		}
	}
}

void FStreamer::SetBitrate(uint16 Kbps)
{
	UE_LOG(PixelStreaming, Log, TEXT("%d Kbps"), Kbps);

	AsyncTask(ENamedThreads::GameThread, [Kbps]()
	{
		CVarEncoderAverageBitRate->Set(Kbps * 1000);
	});

	// reduce framerate proportionally to WebRTC reported bitrate to prioritise quality over FPS/latency
	// by lowering framerate we allocate more bandwidth to fewer frames, thus increasing quality
	if (CVarStreamerPrioritiseQuality.GetValueOnAnyThread())
	{
		int32 Fps;

		// bitrate lower than lower bound results always in min FPS
		// bitrate between lower and upper bounds results in FPS proportionally between min and max FPS
		// bitrate higher than upper bound results always in max FPS
		const uint16 LowerBoundKbps = CVarStreamerLowBitrate.GetValueOnAnyThread();
		const int32 MinFps = FMath::Min(CVarStreamerMinFPS.GetValueOnAnyThread(), InitialMaxFPS);
		const uint16 UpperBoundKbps = CVarStreamerHighBitrate.GetValueOnAnyThread();
		const int32 MaxFps = InitialMaxFPS;

		if (Kbps < LowerBoundKbps)
		{
			Fps = MinFps;
		}
		else if (Kbps < UpperBoundKbps)
		{
			Fps = MinFps + static_cast<uint8>(static_cast<double>(MaxFps - MinFps) / (UpperBoundKbps - LowerBoundKbps) * (Kbps - LowerBoundKbps));
		}
		else
		{
			Fps = MaxFps;
		}

		SetFramerate(Fps);
	}
}

void FStreamer::SetFramerate(int32 Fps)
{
	UE_LOG(PixelStreaming, Log, TEXT("%d FPS"), Fps);

	AsyncTask(ENamedThreads::GameThread, [Fps]()
	{
		GEngine->SetMaxFPS(Fps);
	});
}

void FStreamer::SendResponse(const FString& Descriptor)
{
	Stream(FPlatformTime::Seconds(), PixelStreamingProtocol::EToProxyMsg::Response, reinterpret_cast<const uint8*>(*Descriptor), Descriptor.Len() * sizeof(TCHAR));
}
