// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

class FConductor;

struct FClientSession
    : public webrtc::CreateSessionDescriptionObserver
    , public webrtc::PeerConnectionObserver
    , public webrtc::DataChannelObserver
{
	FClientSession(FConductor& Outer, FClientId ClientId, bool bOriginalQualityController);
	~FClientSession() override;

	void DisconnectClient();

	FConductor& Outer;
	FClientId ClientId;
	bool bOriginalQualityController;
	std::atomic<class FVideoEncoder*> VideoEncoder = nullptr;
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnection;
	rtc::scoped_refptr<webrtc::DataChannelInterface> DataChannel;
	std::atomic<bool> bDisconnecting = false;

	//
	// webrtc::PeerConnectionObserver implementation.
	//
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState NewState) override;
	void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> Stream) override;
	void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> Stream) override;
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> Channel) override;
	void OnRenegotiationNeeded() override;
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState NewState) override;
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState NewState) override;
	void OnIceCandidate(const webrtc::IceCandidateInterface* Candidate) override;
	void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates) override;
	void OnIceConnectionReceivingChange(bool Receiving) override;
	void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
	void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;

	//
	// werbrtc::DataChannelObserver implementation.
	//
	void OnStateChange() override
	{}
	void OnBufferedAmountChange(uint64_t PreviousAmount) override
	{}
	void OnMessage(const webrtc::DataBuffer& Buffer) override;

	//
	// webrtc::CreateSessionDescriptionObserver implementation.
	//
	void OnSuccess(webrtc::SessionDescriptionInterface* Desc) override;
	void OnFailure(const std::string& Error) override;
};
