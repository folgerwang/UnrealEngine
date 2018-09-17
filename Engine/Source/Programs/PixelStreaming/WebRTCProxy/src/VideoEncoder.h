// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "SharedQueue.h"

// Forward declarations
struct FClientSession;

class IVideoEncoderObserver
{
public:
	virtual void ForceKeyFrame() = 0;
	virtual void SetRate(uint32_t BitrateKbps, uint32_t Framerate) = 0;
};

class FVideoEncoder : public webrtc::VideoEncoder
{
public:
	explicit FVideoEncoder(IVideoEncoderObserver& Observer, FClientSession& OwnerSession);

	void SetQualityControlOwnership(bool bOwnership);
	bool HasQualityControlOwnership();

	//
	// webrtc::VideoEncoder interface
	//
	int32_t InitEncode(const webrtc::VideoCodec* CodecSetings, int32_t NumberOfCores, size_t MaxPayloadSize) override;
	int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* Callback) override;
	int32_t Release() override;
	int32_t Encode(
	    const webrtc::VideoFrame& Frame, const webrtc::CodecSpecificInfo* CodecSpecificInfo,
	    const std::vector<webrtc::FrameType>* FrameTypes) override;
	int32_t SetChannelParameters(uint32_t PacketLoss, int64_t Rtt) override;
	int32_t SetRates(uint32_t Bitrate, uint32_t Framerate) override;
	int32_t SetRateAllocation(const webrtc::BitrateAllocation& Allocation, uint32_t Framerate) override;
	ScalingSettings GetScalingSettings() const override;
	bool SupportsNativeHandle() const override;

private:
	IVideoEncoderObserver* Observer;

	// Client session that this encoder instance belongs to
	FClientSession* OwnerSession = nullptr;
	webrtc::EncodedImageCallback* Callback = nullptr;
	webrtc::EncodedImage EncodedImage;
	std::vector<uint8_t> EncodedImageBuffer;
	webrtc::H264BitstreamParser BitstreamParser;
	webrtc::CodecSpecificInfo CodecSpecific;
	webrtc::RTPFragmentationHeader FragHeader;
	bool bStartedFromSPS = false;
	size_t FrameNo = 0;

	std::atomic<bool> bOwnsQualityControl = false;
	std::atomic<bool> bForceBitrateRequest = false;
	webrtc::BitrateAllocation LastBitrate;
	uint32_t LastFramerate = 0;
};

class FVideoEncoderFactory : public webrtc::VideoEncoderFactory
{
public:
	explicit FVideoEncoderFactory(IVideoEncoderObserver& videoSource);

	/**
	 * This is used from the FClientSession::OnSucess to let the factory know
	 * what session the next created encoder should belong to.
	 * It allows us to get the right FClientSession <-> FVideoEncoder relationship
	 */
	void AddSession(FClientSession& ClientSession);

	//
	// webrtc::VideoEncoderFactory implementation
	//
	std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
	CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& Format) const override;
	std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& Format) override;

private:
	IVideoEncoderObserver* VideoSource;
	TSharedQueue<FClientSession*> PendingClientSessions;
};
