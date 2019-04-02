// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VideoEncoder.h"
#include "Logging.h"
#include "H264FrameBuffer.h"
#include "ClientSession.h"

namespace
{
	webrtc::SdpVideoFormat CreateH264Format(webrtc::H264::Profile profile, webrtc::H264::Level level)
	{
		const rtc::Optional<std::string> profile_string =
		    webrtc::H264::ProfileLevelIdToString(webrtc::H264::ProfileLevelId(profile, level));
		check(profile_string);
		return webrtc::SdpVideoFormat(
		    cricket::kH264CodecName,
		    {{cricket::kH264FmtpProfileLevelId, *profile_string},
		     {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
		     {cricket::kH264FmtpPacketizationMode, "1"}});
	}
}

//////////////////////////////////////////////////////////////////////////
//
// FVideoEncoderFactory
//
//////////////////////////////////////////////////////////////////////////

FVideoEncoderFactory::FVideoEncoderFactory(IVideoEncoderObserver& VideoSource)
    : VideoSource(&VideoSource)
{
}

void FVideoEncoderFactory::AddSession(FClientSession& ClientSession)
{
	PendingClientSessions.Push(&ClientSession);
}

std::vector<webrtc::SdpVideoFormat> FVideoEncoderFactory::GetSupportedFormats() const
{
	// return { CreateH264Format(webrtc::H264::kProfileBaseline, webrtc::H264::kLevel3_1),
	//	CreateH264Format(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1) };
	// return { CreateH264Format(webrtc::H264::kProfileMain, webrtc::H264::kLevel3_1) };
	return {CreateH264Format(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel5_1)};
	// return { CreateH264Format(webrtc::H264::kProfileHigh, webrtc::H264::kLevel5_1) };
}

webrtc::VideoEncoderFactory::CodecInfo
FVideoEncoderFactory::QueryVideoEncoder(const webrtc::SdpVideoFormat& Format) const
{
	CodecInfo Info;
	Info.is_hardware_accelerated = true;
	Info.has_internal_source = false;
	return Info;
}

std::unique_ptr<webrtc::VideoEncoder> FVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& Format)
{
	FClientSession* Session;
	bool res = PendingClientSessions.Pop(Session, 0);
	checkf(res, TEXT("no client session associated with encoder instance"));

	auto VideoEncoder = std::make_unique<FVideoEncoder>(*VideoSource, *Session);
	Session->VideoEncoder = VideoEncoder.get();
	return VideoEncoder;
}

//
// FVideoEncoder
//

FVideoEncoder::FVideoEncoder(IVideoEncoderObserver& Observer, FClientSession& OwnerSession)
    : Observer(&Observer)
    , OwnerSession(&OwnerSession)
{
	check(this->Observer);
	check(this->OwnerSession);

	bOwnsQualityControl = OwnerSession.bOriginalQualityController;

	CodecSpecific.codecType = webrtc::kVideoCodecH264;
	// #TODO: Probably smarter setting of `packetization_mode` is required, look at `H264EncoderImpl` ctor
	// CodecSpecific.codecSpecific.H264.packetization_mode = webrtc::H264PacketizationMode::SingleNalUnit;
	CodecSpecific.codecSpecific.H264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
}

void FVideoEncoder::SetQualityControlOwnership(bool bOwnership)
{
	if (bOwnsQualityControl != bOwnership)
	{
		EG_LOG(
		    LogDefault,
		    Log,
		    "%s : ClientId=%d, Ownership=%s",
		    __FUNCTION__,
		    OwnerSession->ClientId,
		    bOwnership ? "true" : "false");
		bForceBitrateRequest = bOwnership;
		bOwnsQualityControl = bOwnership;
	}
}

bool FVideoEncoder::HasQualityControlOwnership()
{
	return bOwnsQualityControl;
}

int32_t FVideoEncoder::InitEncode(const webrtc::VideoCodec* CodecSettings, int32_t NumberOfCores, size_t MaxPayloadSize)
{
	EncodedImage._completeFrame = true;
	return 0;
}

int32_t FVideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* InCallback)
{
	this->Callback = InCallback;
	return 0;
}

int32_t FVideoEncoder::Release()
{
	Callback = nullptr;
	return 0;
}

int32_t FVideoEncoder::Encode(
    const webrtc::VideoFrame& Frame, const webrtc::CodecSpecificInfo* CodecSpecificInfo,
    const std::vector<webrtc::FrameType>* FrameTypes)
{
	// convert (copy) `frame` to `encodedFrame_`, check `webrtc::H264EncoderImpl::Encode` for reference

	FH264FrameBuffer* H264Frame = static_cast<FH264FrameBuffer*>(Frame.video_frame_buffer().get());
	std::vector<uint8_t> const& FrameBuffer = H264Frame->GetBuffer();

	EncodedImage._encodedWidth = Frame.video_frame_buffer()->width();
	EncodedImage._encodedHeight = Frame.video_frame_buffer()->height();
	EncodedImage._timeStamp = Frame.timestamp();
	EncodedImage.ntp_time_ms_ = Frame.ntp_time_ms();
	EncodedImage.capture_time_ms_ = Frame.render_time_ms();
	EncodedImage.rotation_ = Frame.rotation();
	EncodedImage.content_type_ = webrtc::VideoContentType::UNSPECIFIED;
	EncodedImage.timing_.flags = webrtc::TimingFrameFlags::kInvalid;

	//// set `encodedImage_._qp` and `encodedImage-._frameType`
	//// a trick to use `H264BitstreamParser` for retrieving QP info and check for key-frames
	//// the problem is that `H264BitstreamParser::ParseSlice()` is protected
	// struct FBitstreamParser : public webrtc::H264BitstreamParser
	//{
	//	using webrtc::H264BitstreamParser::ParseSlice;
	//};

	EncodedImage._frameType = webrtc::kVideoFrameDelta;
	std::vector<webrtc::H264::NaluIndex> NALUIndices =
	    webrtc::H264::FindNaluIndices(&FrameBuffer[0], FrameBuffer.size());
	bool bKeyFrameFound = false;
	for (const webrtc::H264::NaluIndex& Index : NALUIndices)
	{
		// static_cast<FBitstreamParser*>(&BitstreamParser)
		//    ->ParseSlice(&FrameBuffer[Index.payload_start_offset], Index.payload_size);

		webrtc::H264::NaluType NALUType = webrtc::H264::ParseNaluType(FrameBuffer[Index.payload_start_offset]);

		if (NALUType == webrtc::H264::kIdr /* ||
		    NALUType == webrtc::H264::kSps ||
		    NALUType == webrtc::H264::kPps*/
		    && !bKeyFrameFound)
		{
			EncodedImage._frameType = webrtc::kVideoFrameKey;
			// EG_LOG(LogDefault, Log, "key-frame");
			bKeyFrameFound = true;
			// break; // we need to parse all NALUs so as H264BitstreamParser maintains internal state
			break;
		}
	}

	// enforce key-frame if requested by webrtc and if we haven't received one
	// seems it's always just one FrameType provided, as reference implementation of
	// H264EncoderImpl checks only the first one
	if (EncodedImage._frameType != webrtc::kVideoFrameKey && FrameTypes && (*FrameTypes)[0] == webrtc::kVideoFrameKey)
	{
		EG_LOG(LogDefault, Log, "key-frame requested, size=%zu", FrameTypes->size());

		// #MULTICAST : Should we limit what video encoder instances ask for keyframes?
		if (bOwnsQualityControl)
			Observer->ForceKeyFrame();
	}

	// when we switch quality control to client with higher B/W WebRTC won't notify us that bitrate can
	// be increased. So force set last recorded bitrate for this client though we also could set just sufficiently
	// big number to force webRTC to report what actual B/W is
	if (bOwnsQualityControl && bForceBitrateRequest && LastBitrate.get_sum_kbps() > 0)
	{
		SetRateAllocation(LastBitrate, LastFramerate);
	}

	// BitstreamParser.GetLastSliceQp(&EncodedImage.qp_);

	// copy frame buffer
	// EncodedImageBuffer.resize(FrameBuffer.size());
	EncodedImageBuffer.assign(begin(FrameBuffer), end(FrameBuffer));
	EncodedImage._buffer = &EncodedImageBuffer[0];
	EncodedImage._length = EncodedImage._size = EncodedImageBuffer.size();

	// fill RTP fragmentation info
	FragHeader.VerifyAndAllocateFragmentationHeader(NALUIndices.size());
	FragHeader.fragmentationVectorSize = static_cast<uint16_t>(NALUIndices.size());
	for (int I = 0; I != NALUIndices.size(); ++I)
	{
		webrtc::H264::NaluIndex const& NALUIndex = NALUIndices[I];
		FragHeader.fragmentationOffset[I] = NALUIndex.payload_start_offset;
		FragHeader.fragmentationLength[I] = NALUIndex.payload_size;

		webrtc::H264::NaluType NALUType = webrtc::H264::ParseNaluType(FrameBuffer[NALUIndex.payload_start_offset]);
#if 0
		EG_LOG(
		    LogDefault,
		    Log,
		    "NALU: %d, start=%z, payload=%z",
		    static_cast<int>(NALUType),
		    NALUIndex.start_offset,
		    NALUIndex.payload_size);
#endif
	}

	// Deliver encoded image.
	Callback->OnEncodedImage(EncodedImage, &CodecSpecific, &FragHeader);

	++FrameNo;

	return 0;
}

int32_t FVideoEncoder::SetChannelParameters(uint32_t PacketLoss, int64_t Rtt)
{
	// EG_LOG(
	//    LogDefault,
	//    Log,
	//    "%s : ClientId=%d, PacketLoss=%u, Rtt=%" PRId64 "",
	//    __FUNCTION__,
	//    OwnerSession->ClientId,
	//    PacketLoss,
	//    Rtt);
	return 0;
}

int32_t FVideoEncoder::SetRates(uint32_t Bitrate, uint32_t Framerate)
{
	// EG_LOG(
	//    LogDefault,
	//    Log,
	//    "%s: ClientId=%d, BitRate=%u, Framerate=%u",
	//    __FUNCTION__,
	//    OwnerSession->ClientId,
	//    Bitrate,
	//    Framerate);
	return 0;
}

int32_t FVideoEncoder::SetRateAllocation(const webrtc::BitrateAllocation& Allocation, uint32_t Framerate)
{
	LastBitrate = Allocation;
	LastFramerate = Framerate;

	if (!bOwnsQualityControl)
	{
		return 0;
	}

	// it seems webrtc just reports the current framerate w/o much effort to probe what's
	// max framerate it can achieve
	// let's lift it a bit every time so we can keep it as high as possible
	uint32_t LiftedFramerate = Framerate + std::min(static_cast<uint32_t>(Framerate * 0.9), 1u);
	EG_LOG(
	    LogDefault,
	    Log,
	    "%s : ClientId=%d, Bitrate=%u kbps, framerate=%u, lifted framerate=%u",
	    __FUNCTION__,
	    OwnerSession->ClientId,
	    Allocation.get_sum_kbps(),
	    Framerate,
	    LiftedFramerate);

	Observer->SetRate(Allocation.get_sum_kbps(), LiftedFramerate);

	bForceBitrateRequest = false;

	return 0;
}

webrtc::VideoEncoder::ScalingSettings FVideoEncoder::GetScalingSettings() const
{
	// verifySlow(false);
	// return ScalingSettings{ ScalingSettings::kOff };
	return ScalingSettings{0, 1024 * 1024};
}

bool FVideoEncoder::SupportsNativeHandle() const
{
	return true;
}
