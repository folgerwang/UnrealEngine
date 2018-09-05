// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

class FH264FrameBuffer : public webrtc::VideoFrameBuffer
{
public:
	FH264FrameBuffer(int Width, int Height)
	    : Width(Width)
	    , Height(Height)
	{
	}

	//
	// webrtc::VideoFrameBuffer interface
	//
	Type type() const override
	{
		return Type::kNative;
	}

	virtual int width() const override
	{
		return Width;
	}

	virtual int height() const override
	{
		return Height;
	}

	rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
	{
		check(false);
		return nullptr;
	}

	//
	// Own methods
	//
	std::vector<uint8_t>& GetBuffer()
	{
		return Buffer;
	}

private:
	int Width;
	int Height;
	std::vector<uint8_t> Buffer;
};
