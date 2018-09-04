// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "H264FrameBuffer.h"

class FNetworkVideoCapturer : public cricket::VideoCapturer
{
public:
	FNetworkVideoCapturer();

	void ProcessPacket(PixelStreamingProtocol::EToProxyMsg PkType, const void* Data, uint32_t Size);

private:
	//////////////////////////////////////////////////////////////////////////
	// cricket::VideoCapturer interface
	cricket::CaptureState Start(const cricket::VideoFormat& Format) override
	{ return cricket::CS_RUNNING; }

	void Stop() override
	{}

	bool IsRunning() override
	{ return true; }

	bool IsScreencast() const override
	{ return false; }

	bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) override
	{
		fourccs->push_back(cricket::FOURCC_H264);
		return true;
	}
	//////////////////////////////////////////////////////////////////////////

	uint64_t FrameNo = 0;
	int32_t Width = 1920;
	int32_t Height = 1080;
	int32_t Framerate = 60;
	int64_t LastNtpTimeMs = 0;
};
