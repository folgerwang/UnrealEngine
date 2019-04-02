// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ClientSession.h"
#include "Logging.h"
#include "Conductor.h"
#include "SetSessionDescriptionObserver.h"

namespace detail
{
	const char* ToString(webrtc::PeerConnectionInterface::SignalingState Val)
	{
		if (Val == webrtc::PeerConnectionInterface::kStable)
			return "kStable";
		else if (Val == webrtc::PeerConnectionInterface::kHaveLocalOffer)
			return "kHaveLocalOffer";
		else if (Val == webrtc::PeerConnectionInterface::kHaveLocalPrAnswer)
			return "kHaveLocalPrAnswer";
		else if (Val == webrtc::PeerConnectionInterface::kHaveRemoteOffer)
			return "kHaveRemoteOffer";
		else if (Val == webrtc::PeerConnectionInterface::kHaveRemotePrAnswer)
			return "kHaveRemotePrAnswer";
		else if (Val == webrtc::PeerConnectionInterface::kClosed)
			return "kClosed";
		else
		{
			checkfSlow(false, "Unknown enum value (%u). Revise code.", (uint32_t)Val);
			return "Unknown";
		}
	};

	const char* ToString(webrtc::PeerConnectionInterface::IceConnectionState Val)
	{
		if (Val == webrtc::PeerConnectionInterface::kIceConnectionNew)
			return "kIceConnectionNew";
		else if (Val == webrtc::PeerConnectionInterface::kIceConnectionChecking)
			return "kIceConnectionChecking";
		else if (Val == webrtc::PeerConnectionInterface::kIceConnectionConnected)
			return "kIceConnectionConnected";
		else if (Val == webrtc::PeerConnectionInterface::kIceConnectionCompleted)
			return "kIceConnectionCompleted";
		else if (Val == webrtc::PeerConnectionInterface::kIceConnectionFailed)
			return "kIceConnectionFailed";
		else if (Val == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
			return "kIceConnectionDisconnected";
		else if (Val == webrtc::PeerConnectionInterface::kIceConnectionClosed)
			return "kIceConnectionClosed";
		else
		{
			checkfSlow(false, "Unknown enum value (%u). Revise code.", (uint32_t)Val);
			return "Unknown";
		}
	};

	const char* ToString(webrtc::PeerConnectionInterface::IceGatheringState Val)
	{
		if (Val == webrtc::PeerConnectionInterface::kIceGatheringNew)
			return "kIceGatheringNew";
		else if (Val == webrtc::PeerConnectionInterface::kIceGatheringGathering)
			return "kIceGatheringGathering";
		else if (Val == webrtc::PeerConnectionInterface::kIceGatheringComplete)
			return "kIceGatheringComplete";
		else
		{
			checkfSlow(false, "Unknown enum value (%u). Revise code.", (uint32_t)Val);
			return "Unknown";
		}
	};

	const char* ToString(bool Val)
	{
		return Val ? "True" : "False";
	}
}

FClientSession::FClientSession(FConductor& Outer, FClientId ClientId, bool bOriginalQualityController)
    : Outer(Outer)
    , ClientId(ClientId)
	, bOriginalQualityController(bOriginalQualityController)
{
	EG_LOG(LogDefault, Log, "%s: ClientId=%u", __FUNCTION__, ClientId);
}

FClientSession::~FClientSession()
{
	EG_LOG(LogDefault, Log, "%s: ClientId=%u", __FUNCTION__, ClientId);
	if (DataChannel)
		DataChannel->UnregisterObserver();
}

void FClientSession::DisconnectClient()
{
	if (bDisconnecting)
		return; // already notified Cirrus to disconnect this client

	bDisconnecting = true;
	Outer.CirrusConnection.SendDisconnectClient(ClientId);
}

//
// webrtc::PeerConnectionObserver implementation.
//

void FClientSession::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState NewState)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u, NewState=%s", __FUNCTION__, ClientId, detail::ToString(NewState));
}

// Called when a remote stream is added
void FClientSession::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> Stream)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u, Stream=%s", __FUNCTION__, ClientId, Stream->id().c_str());
}

void FClientSession::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> Stream)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u, Stream=%s", __FUNCTION__, ClientId, Stream->id().c_str());
}

void FClientSession::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> InDataChannel)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u", __FUNCTION__, ClientId);
	this->DataChannel = InDataChannel;
	this->DataChannel->RegisterObserver(this);
}

void FClientSession::OnRenegotiationNeeded()
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u", __FUNCTION__, ClientId);
}

void FClientSession::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState NewState)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u, NewState=%s", __FUNCTION__, ClientId, detail::ToString(NewState));
}

void FClientSession::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState NewState)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u, NewState=%s", __FUNCTION__, ClientId, detail::ToString(NewState));
}

void FClientSession::OnIceCandidate(const webrtc::IceCandidateInterface* Candidate)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u", __FUNCTION__, ClientId);

	Json::StyledWriter Writer;
	Json::Value Jmessage;

	Jmessage[kCandidateSdpMidName] = Candidate->sdp_mid();
	Jmessage[kCandidateSdpMlineIndexName] = Candidate->sdp_mline_index();
	std::string Sdp;
	if (!Candidate->ToString(&Sdp))
	{
		EG_LOG(LogDefault, Error, "Failed to serialize candidate for client %u", ClientId);
		return;
	}

	EG_LOG(
	    LogDefault,
	    Log,
	    "Sending ICE candidate to Client %u (sdp_mline_index=%d) : %s",
	    ClientId,
	    Candidate->sdp_mline_index(),
	    Sdp.c_str());

	Jmessage[kCandidateSdpName] = Sdp;
	std::string Msg = Writer.write(Jmessage);
	Outer.CirrusConnection.SendIceCandidate(ClientId, Msg);
}

void FClientSession::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u", __FUNCTION__, ClientId);
}

void FClientSession::OnIceConnectionReceivingChange(bool Receiving)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u, Receiving=%s", __FUNCTION__, ClientId, detail::ToString(Receiving));
}

void FClientSession::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u", __FUNCTION__, ClientId);
}

void FClientSession::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
	EG_LOG(LogDefault, Log, "%s : ClientId=%u", __FUNCTION__, ClientId);
}

//
// webrtc::DataChannelObserver implementation.
//

void FClientSession::OnMessage(const webrtc::DataBuffer& Buffer)
{
	auto MsgType = static_cast<PixelStreamingProtocol::EToUE4Msg>(Buffer.data.data()[0]);
	if (MsgType == PixelStreamingProtocol::EToUE4Msg::RequestQualityControl)
	{
		check(Buffer.data.size() == 1);
		Outer.OnQualityOwnership(ClientId);
	}
	else
	{
		Outer.UE4Connection.Send(Buffer.data.data(), static_cast<uint32_t>(Buffer.data.size()));
	}
}

//
// webrtc::CreateSessionDescriptionObserver implementation.
//
void FClientSession::OnSuccess(webrtc::SessionDescriptionInterface* Desc)
{
	std::string Sdp;
	Desc->ToString(&Sdp);
	EG_LOG(LogDefault, Log, "Answer for client %u : %s", ClientId, Sdp.c_str());

	// #REFACTOR : With WebRTC branch-heads/66, the sink of video capturer will be added as a direct result
	// of `PeerConnection->SetLocalDescription()` call but video encoder will be created later on
	// the first frame pushed into the pipeline (by capturer).
	// We need to associate this `FClientSession` instance with the right instance of `FVideoEncoder` for quality
	// control, the problem is that `FVideoEncoder` is created asynchronously on demand and there's no
	// clean way to give it the right instance of `FClientSession`.
	// The plan is to assume that encoder instances are created in the same order as we call
	// `PeerConnection->SetLocalDescription()`, as these calls are done from the same thread and internally
	// WebRTC uses `std::vector` for capturer's sinks and then iterates over it to create encoder instances,
	// and there's no obvious reason why it can be replaced by an unordered container in the future.
	// So before adding a new sink to the capturer (`PeerConnection->SetLocalDescription()`) we push
	// this `FClientSession` into encoder factory queue and pop it out of the queue when encoder instance
	// is created. Unfortunately I (Andriy) don't see a way to put `check`s to verify it works correctly.
	Outer.VideoEncoderFactory->AddSession(*this);
	// we assume just created local session description shouldn't cause any issue and so proceed immediately
	// not waiting for confirmation, otherwise we hard fail
	PeerConnection->SetLocalDescription(
		FSetSessionDescriptionObserver::Create(
	        []() {},
	        [](const std::string& error) { checkf(false, TEXT("Setting local description failed: %s"), error.c_str()); }
		),
		Desc
	);

    Outer.UE4Connection.StartStreaming();

	Json::StyledWriter Writer;
	Json::Value Jmessage;
	Jmessage[kSessionDescriptionTypeName] = webrtc::SdpTypeToString(Desc->GetType());
	Jmessage[kSessionDescriptionSdpName] = Sdp;
	std::string msg = Writer.write(Jmessage);
	Outer.CirrusConnection.SendAnswer(ClientId, msg);
}

void FClientSession::OnFailure(const std::string& Error)
{
	EG_LOG(LogDefault, Error, "Failed to create answer for client %u : %s", ClientId, Error.c_str());

	// This must be the last line because it will destroy this instance
	Outer.DeleteClient(ClientId);
}
