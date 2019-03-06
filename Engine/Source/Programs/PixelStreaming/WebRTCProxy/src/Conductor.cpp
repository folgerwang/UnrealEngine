// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Conductor.h"
#include "Logging.h"
#include "UE4Connection.h"
#include "NetworkAudioCapturer.h"
#include "NetworkVideoCapturer.h"
#include "ClientSession.h"
#include "SetSessionDescriptionObserver.h"

const char StreamId[] = "stream_id";
const char AudioLabel[] = "audio_label";
const char VideoLabel[] = "video_label";

using webrtc::SdpType;

//////////////////////////////////////////////////////////////////////////
// FConductor
//////////////////////////////////////////////////////////////////////////

FConductor::FConductor()
    : UE4Connection(*this)
    , CirrusConnection(*this)
    , AudioCapturer(new rtc::RefCountedObject<FNetworkAudioCapturer>())
{
	auto VideoEncoderFactoryStrong = std::make_unique<FVideoEncoderFactory>(*this);
	// #HACK: Keep a pointer to the Video encoder factory, so we can use it to figure out the
	// FClientSession <-> FakeVideoEncoder relationship later on
	check(!VideoEncoderFactory);
	VideoEncoderFactory = VideoEncoderFactoryStrong.get();

	PeerConnectionFactory = webrtc::CreatePeerConnectionFactory(
	    nullptr,
	    nullptr,
	    nullptr,
	    AudioCapturer,
	    webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>(),
	    webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>(),
	    std::move(VideoEncoderFactoryStrong),
	    std::make_unique<webrtc::InternalDecoderFactory>(),
	    nullptr,
	    nullptr);
	check(PeerConnectionFactory);

	ResetPeerConnectionConfig();

	UE4Connection.Connect("127.0.0.1", PARAM_UE4Port);
}

FConductor::~FConductor()
{
	// #REFACTOR: To destroy NetworkVideoCapturer first, TODO (andriy): reconsider/simplify dependencies
	DeleteAllClients();
}

FClientSession* FConductor::GetClientSession(FClientId ClientId)
{
	auto It = Clients.find(ClientId);
	if (It == Clients.end())
	{
		return nullptr;
	}
	else
	{
		return It->second.get();
	}
}

void FConductor::DeleteAllClients()
{
	while (Clients.size())
	{
		DeleteClient(Clients.begin()->first);
	}
}

void FConductor::CreateClient(FClientId ClientId)
{
	check(PeerConnectionFactory.get() != NULL);

	if (PARAM_PlanB)
	{
		verifyf(Clients.find(ClientId) == Clients.end(), TEXT("Client %u already exists"), ClientId);
	}
	else
	{
		// With unified plan, we get several calls to OnOffer, which in turn calls
		// this several times.
		// Therefore, we only try to create the client if not created already
		if (Clients.find(ClientId) != Clients.end())
		{
			return;
		}
	}

	webrtc::FakeConstraints Constraints;
	Constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");

	rtc::scoped_refptr<FClientSession> Session = new rtc::RefCountedObject<FClientSession>(*this, ClientId, Clients.empty());
	Session->PeerConnection = PeerConnectionFactory->CreatePeerConnection(PeerConnectionConfig, &Constraints, NULL, NULL, Session.get());
	check(Session->PeerConnection);
	Clients[ClientId] = std::move(Session);
}

void FConductor::DeleteClient(FClientId ClientId)
{
	Clients.erase(ClientId);
	if (Clients.size() == 0)
	{
		UE4Connection.StopStreaming();

		if (!PARAM_PlanB)
		{
			AudioTrack = nullptr;
			VideoTrack = nullptr;
		}
		Streams.clear();
	}
}

void FConductor::AddStreams(FClientId ClientId)
{
	FClientSession* Session = GetClientSession(ClientId);
	check(Session);

	if (PARAM_PlanB)
	{
		rtc::scoped_refptr<webrtc::MediaStreamInterface> Stream;

		if (Streams.find(StreamId) != Streams.end())
		{
			Stream = Streams[StreamId];
		}
		else
		{
			Stream = PeerConnectionFactory->CreateLocalMediaStream(StreamId);

			rtc::scoped_refptr<webrtc::AudioTrackInterface> LocalAudioTrack(
			    PeerConnectionFactory->CreateAudioTrack(AudioLabel, PeerConnectionFactory->CreateAudioSource(NULL)));

			Stream->AddTrack(LocalAudioTrack);

			std::unique_ptr<FNetworkVideoCapturer> VideoCapturerStrong = std::make_unique<FNetworkVideoCapturer>();
			VideoCapturer = VideoCapturerStrong.get();
			rtc::scoped_refptr<webrtc::VideoTrackInterface> LocalVideoTrack(PeerConnectionFactory->CreateVideoTrack(
			    VideoLabel, PeerConnectionFactory->CreateVideoSource(std::move(VideoCapturerStrong))));

			Stream->AddTrack(LocalVideoTrack);

			typedef std::pair<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> > MediaStreamPair;
			Streams.insert(MediaStreamPair(Stream->id(), Stream));
		}

		verifyf(Session->PeerConnection->AddStream(Stream), TEXT("Failed to add stream for client %u"), ClientId);
	}
	else
	{
		if (!Session->PeerConnection->GetSenders().empty())
		{
			return;  // Already added tracks
		}

		if (!AudioTrack)
		{
			AudioTrack =
			    PeerConnectionFactory->CreateAudioTrack(AudioLabel, PeerConnectionFactory->CreateAudioSource(NULL));
		}

		if (!VideoTrack)
		{
			std::unique_ptr<FNetworkVideoCapturer> VideoCapturerStrong = std::make_unique<FNetworkVideoCapturer>();
			VideoCapturer = VideoCapturerStrong.get();
			VideoTrack = PeerConnectionFactory->CreateVideoTrack(
			    VideoLabel, PeerConnectionFactory->CreateVideoSource(std::move(VideoCapturerStrong)));
		}

		auto ResultOrError = Session->PeerConnection->AddTrack(AudioTrack, {StreamId});
		if (!ResultOrError.ok())
		{
			EG_LOG(
			    LogDefault,
			    Error,
			    "Failed to add AudioTrack to PeerConnection of client %u. Msg=%s",
			    Session->ClientId,
			    ResultOrError.error().message());
		}

		ResultOrError = Session->PeerConnection->AddTrack(VideoTrack, {StreamId});
		if (!ResultOrError.ok())
		{
			EG_LOG(
			    LogDefault,
			    Error,
			    "Failed to add VideoTrack to PeerConnection of client %u. Msg=%s",
			    Session->ClientId,
			    ResultOrError.error().message());
		}
	}
}

void FConductor::OnQualityOwnership(FClientId ClientId)
{
	// First disable ownership for all
	for (auto&& Client : Clients)
	{
		if (Client.second->VideoEncoder && Client.second->DataChannel)
		{
			Client.second->VideoEncoder.load()->SetQualityControlOwnership(false);
			rtc::CopyOnWriteBuffer Buf(2);
			Buf[0] = static_cast<uint8_t>(PixelStreamingProtocol::EToClientMsg::QualityControlOwnership);
			Buf[1] = 0;  // false
			Client.second->DataChannel->Send(webrtc::DataBuffer(Buf, true));
		}
	}

	FClientSession* Session = GetClientSession(ClientId);
	if (Session->VideoEncoder && Session->DataChannel)
	{
		// Then enable this instance. This avoids any potential competition
		Session->VideoEncoder.load()->SetQualityControlOwnership(true);
		rtc::CopyOnWriteBuffer Buf(2);
		Buf[0] = static_cast<uint8_t>(PixelStreamingProtocol::EToClientMsg::QualityControlOwnership);
		Buf[1] = 1;  // true
		Session->DataChannel->Send(webrtc::DataBuffer(Buf, true));
	}
}

// IUE4ConnectionObserver implementation

void FConductor::OnUE4Connected()
{
	CirrusConnection.Connect(PARAM_Cirrus.first, PARAM_Cirrus.second);
}

void FConductor::OnUE4Disconnected()
{
	DeleteAllClients();
	CirrusConnection.Disconnect();
}

void FConductor::OnUE4Packet(PixelStreamingProtocol::EToProxyMsg PktType, const void* Pkt, uint32_t Size)
{
	// Forward to the audio component if it's audio
	if (PktType == PixelStreamingProtocol::EToProxyMsg::AudioPCM)
	{
		check(AudioCapturer);
		AudioCapturer->ProcessPacket(PktType, Pkt, Size);
	}
	else if (PktType == PixelStreamingProtocol::EToProxyMsg::Response)
	{
		// Currently broadcast the response to all clients.
		for (auto&& Client : Clients)
		{
			if (Client.second->DataChannel)
			{
				rtc::CopyOnWriteBuffer Buffer(Size + 1);
				Buffer[0] = static_cast<uint8_t>(PixelStreamingProtocol::EToClientMsg::Response);
				std::memcpy(&Buffer[1], reinterpret_cast<const uint8_t*>(Pkt), Size);
				Client.second->DataChannel->Send(webrtc::DataBuffer(Buffer, true));
			}
		}
	}
	else
	{
		check(VideoCapturer);
		VideoCapturer->ProcessPacket(PktType, Pkt, Size);
	}
}

//
// ICirrusConnectionObserver implementation.
//

void FConductor::OnCirrusConfig(const std::string& Config)
// gets configuration from Cirrus so we have a single point to provide configuration shared by Proxy and clients
// parses from JSON and stores in `webrtc::RTCConfiguration` that will be used for all clients peer connections
{
	Json::Reader Reader;
	Json::Value ConfigJson;
	bool res = Reader.parse(Config, ConfigJson);
	checkf(res, TEXT("Received invalid JSON config from Cirrus: %s"), Config.c_str());

	EG_LOG(LogDefault, Log, "Cirrus config : %s", ConfigJson.toStyledString().c_str());

	checkf(!ConfigJson[kPeerConnectionConfigName].isNull(), TEXT("No \"%s\" key in Cirrus config: %s"), kPeerConnectionConfigName, ConfigJson.toStyledString().c_str());

	Json::Value PcCfgJson = ConfigJson[kPeerConnectionConfigName];
	Json::Value IceServersListJson = PcCfgJson[kIceServersName];
	if (!IceServersListJson)
		return;

	for (auto IceServerJson : IceServersListJson)
	{
		PeerConnectionConfig.servers.emplace_back();
		auto& IceServer = PeerConnectionConfig.servers.back();

		for (auto Url : IceServerJson[kUrlsName])
		{
			IceServer.urls.push_back(Url.asString());
		}

		Json::Value UsernameJson = IceServerJson[kUsernameName];
		if (!UsernameJson.isNull())
		{
			IceServer.username = UsernameJson.asString();
		}

		Json::Value CredentialJson = IceServerJson[kCredentialName];
		if (!CredentialJson.isNull())
		{
			IceServer.password = CredentialJson.asString();
		}
	}
}

void FConductor::ResetPeerConnectionConfig()
{
	PeerConnectionConfig = webrtc::PeerConnectionInterface::RTCConfiguration{};
	PeerConnectionConfig.sdp_semantics =
	    PARAM_PlanB ? webrtc::SdpSemantics::kPlanB : webrtc::SdpSemantics::kUnifiedPlan;
}

void FConductor::OnOffer(FClientId ClientId, const std::string& Offer)
{
	CreateClient(ClientId);
	AddStreams(ClientId);

	FClientSession* Session = GetClientSession(ClientId);
	checkf(Session, TEXT("Client %u not found"), ClientId);

	Json::Reader Reader;
	Json::Value Jmessage;
	std::string Sdp;
	if (!Reader.parse(Offer, Jmessage) || Jmessage.get(kSessionDescriptionTypeName, "") != "offer" ||
	    (Sdp = Jmessage.get(kSessionDescriptionSdpName, "").asString()) == "")
	{
		EG_LOG(LogDefault, Warning, "Received invalid JSON for Offer from Client %u : %s", ClientId, Offer.c_str());
		Session->DisconnectClient();
		return;
	}

	EG_LOG(LogDefault, Log, "Received offer from client %u : %s", ClientId, Sdp.c_str());

	webrtc::SdpParseError Error;
	std::unique_ptr<webrtc::SessionDescriptionInterface> SessionDesc =
	    webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, Sdp, &Error);
	if (!SessionDesc)
	{
		// offer comes from the client and can be malformed/unsupported
		// don't crash here but tell Cirrus to disconnect the client
		EG_LOG(
		    LogDefault,
		    Warning,
		    "Can't parse offer from client %u. SdpParseError was '%s'. Disconnecting client.",
		    ClientId,
		    Error.description.c_str());
		Session->DisconnectClient();
		return;
	}

	// this can fail if client is incompatible, so proceed only on success
	Session->PeerConnection->SetRemoteDescription(
	    FSetSessionDescriptionObserver::Create(
	        [Session]() { Session->PeerConnection->CreateAnswer(Session, nullptr); },
	        [Session](const std::string& error) {
		        EG_LOG(LogDefault, Error, error.c_str());
		        Session->DisconnectClient();
	        }),
	    SessionDesc.release());
}

void FConductor::OnIceCandidate(FClientId ClientId, const std::string& IceCandidate)
{
	EG_LOG(LogDefault, Log, "Received ICE candidate from Client %u : %s", ClientId, IceCandidate.c_str());

	FClientSession* Session = GetClientSession(ClientId);
	checkf(Session, TEXT("Client %u not found"), ClientId);

	Json::Reader Reader;
	Json::Value Jmessage;
	if (!Reader.parse(IceCandidate, Jmessage))
	{
		EG_LOG(
		    LogDefault,
		    Warning,
		    "Received invalid JSON for ICE Candidate from Client %u : %s",
		    ClientId,
		    IceCandidate.c_str());
		Session->DisconnectClient();
		return;
	}

	std::string Sdp_mid;
	int Sdp_mlineindex = 0;
	std::string Sdp;
	if (!rtc::GetStringFromJsonObject(Jmessage, kCandidateSdpMidName, &Sdp_mid) ||
	    !rtc::GetIntFromJsonObject(Jmessage, kCandidateSdpMlineIndexName, &Sdp_mlineindex) ||
	    !rtc::GetStringFromJsonObject(Jmessage, kCandidateSdpName, &Sdp))
	{
		EG_LOG(
		    LogDefault,
		    Warning,
		    "Cannot parse ICE Candidate fields from Client %u : %s",
		    ClientId,
		    IceCandidate.c_str());
		Session->DisconnectClient();
		return;
	}

	webrtc::SdpParseError Error;
	std::unique_ptr<webrtc::IceCandidateInterface> Candidate(
	    webrtc::CreateIceCandidate(Sdp_mid, Sdp_mlineindex, Sdp, &Error));
	if (!Candidate.get())
	{
		EG_LOG(LogDefault, Warning, "Cannot parse ICE Candidate from Client %u : %s ", ClientId, IceCandidate.c_str());
		Session->DisconnectClient();
		return;
	}

	if (!Session->PeerConnection->AddIceCandidate(Candidate.get()))
	{
		EG_LOG(
		    LogDefault, Warning, "Failed to apply ICE Candidate from Client %u : %s ", ClientId, IceCandidate.c_str());
		Session->DisconnectClient();
		return;
	}
}

void FConductor::OnClientDisconnected(FClientId ClientId)
{
	EG_LOG(LogDefault, Log, "Client %u disconnected", ClientId);
	DeleteClient(ClientId);
}

void FConductor::OnCirrusDisconnected()
{
	EG_LOG(LogDefault, Log, "Cirrus disconnected. Removing all clients");
	DeleteAllClients();
	ResetPeerConnectionConfig();
}

// IVideoEncoderObserver impl

void FConductor::ForceKeyFrame()
{
	UE4Connection.ForceKeyFrame();
}

void FConductor::SetRate(uint32_t BitrateKbps, uint32_t Framerate)
{
	UE4Connection.SetRate(BitrateKbps, Framerate);
}
