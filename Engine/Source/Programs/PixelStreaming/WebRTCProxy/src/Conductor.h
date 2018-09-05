// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "UE4Connection.h"
#include "CirrusConnection.h"
#include "VideoEncoder.h"

// Forward declarations
struct FClientSession;
class FNetworkAudioCapturer;
class FNetworkVideoCapturer;

class FConductor :
	public IUE4ConnectionObserver,
	public ICirrusConnectionObserver,
	public IVideoEncoderObserver
{
public:
	FConductor();
	~FConductor();

private:
	// IUE4ConnectionObserver implementation
	void OnUE4Connected() override;
	void OnUE4Disconnected() override;
	void OnUE4Packet(PixelStreamingProtocol::EToProxyMsg PktType, const void* Data, uint32_t Size) override;

	// ICirrusConnectionObserver implementation
	void OnCirrusConfig(const std::string& Config) override;
	void OnOffer(FClientId ClientId, const std::string& Offer) override;
	void OnIceCandidate(FClientId ClientId, const std::string& IceCandidate) override;
	void OnClientDisconnected(FClientId ClientId) override;
	void OnCirrusDisconnected() override;

	// IVideoEncoderObserver
	void ForceKeyFrame() override;
	void SetRate(uint32_t BitrateKbps, uint32_t Framerate) override;

	// own methods
	void CreateClient(FClientId ClientId);
	void DeleteClient(FClientId ClientId);
	void DeleteAllClients();
	FClientSession* GetClientSession(FClientId ClientId);

	void AddStreams(FClientId ClientId);

	void OnQualityOwnership(FClientId ClientId);

	void ResetPeerConnectionConfig();

	friend FClientSession;

private:
	FUE4Connection UE4Connection;
	FCirrusConnection CirrusConnection;

	rtc::scoped_refptr<FNetworkAudioCapturer> AudioCapturer;
	// #MULTICAST : Refactor this. We are keeping the raw pointer internally,
	// since the outside code requires the ownership (std::unique_ptr). Dangerous cos it allows
	// usage after destruction
	FNetworkVideoCapturer* VideoCapturer = nullptr;
	FVideoEncoderFactory* VideoEncoderFactory = nullptr;

	std::unordered_map<FClientId, rtc::scoped_refptr<FClientSession>> Clients;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory;
	webrtc::PeerConnectionInterface::RTCConfiguration PeerConnectionConfig;

	// This is only used if using PlanB semantics
	std::unordered_map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface>> Streams;
	// These are used only if using UnifiedPlan semantics
	rtc::scoped_refptr<webrtc::AudioTrackInterface> AudioTrack;
	rtc::scoped_refptr<webrtc::VideoTrackInterface> VideoTrack;
};
