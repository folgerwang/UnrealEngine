// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NetworkVideoCapturer.h"
#include "Logging.h"
#include "UE4Connection.h"

FNetworkVideoCapturer::FNetworkVideoCapturer()
{
	set_enable_video_adapter(false);

	std::vector<cricket::VideoFormat> Formats;
	Formats.push_back(cricket::VideoFormat(1920, 1080, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_H264));
	SetSupportedFormats(Formats);
}

void FNetworkVideoCapturer::ProcessPacket(PixelStreamingProtocol::EToProxyMsg PkType, const void* Data, uint32_t Size)
{
	rtc::scoped_refptr<FH264FrameBuffer> buffer = new rtc::RefCountedObject<FH264FrameBuffer>(Width, Height);
	webrtc::VideoFrame Frame{buffer, webrtc::VideoRotation::kVideoRotation_0, 0};

	// #Andriy: WebRTC doesn't like frames with the same timestamp and will drop one of them
	// we don't like our frames to be dropped so let's cheat with setting a unique value but close to be true
	int64_t NtpTimeMs = rtc::TimeMillis();
	if (NtpTimeMs <= LastNtpTimeMs)
		NtpTimeMs = LastNtpTimeMs + 1;
	LastNtpTimeMs = NtpTimeMs;
	Frame.set_ntp_time_ms(NtpTimeMs);

	auto PkData = reinterpret_cast<const uint8_t*>(Data);

	buffer->GetBuffer().assign(PkData, PkData + Size);

	OnFrame(Frame, Width, Height);
	++FrameNo;
}
