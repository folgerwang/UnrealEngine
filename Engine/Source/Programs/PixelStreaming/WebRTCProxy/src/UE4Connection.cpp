// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UE4Connection.h"
#include "Logging.h"

using PixelStreamingProtocol::EToUE4Msg;
using PixelStreamingProtocol::EToProxyMsg;

FUE4Connection::FUE4Connection(IUE4ConnectionObserver& Observer):
	Observer(Observer),
	Connection("UE4", *this)
{}

void FUE4Connection::Connect(const std::string& IP, uint16_t Port)
{
	Connection.Connect(IP, Port);
}

void FUE4Connection::OnConnect()
{
	Observer.OnUE4Connected();
}

void FUE4Connection::OnDisconnect(int Err)
{
	Observer.OnUE4Disconnected();
}

void FUE4Connection::StartStreaming()
{
	bStreamingStarted = true;
	const auto msg = EToUE4Msg::StartStreaming;
	Connection.Send(&msg, sizeof(msg));
}

void FUE4Connection::StopStreaming()
{
	const auto msg = EToUE4Msg::StopStreaming;
	Connection.Send(&msg, sizeof(msg));
	bStreamingStarted = false;
}

void FUE4Connection::ForceKeyFrame()
{
	const auto msg = EToUE4Msg::IFrameRequest;
	Connection.Send(&msg, sizeof(msg));
}

void FUE4Connection::SetRate(uint32_t BitrateKbps, uint32_t Framerate)
{
	{
		uint8_t Buf[1 + sizeof(uint16_t)] = {
			static_cast<uint8_t>(EToUE4Msg::AverageBitrateRequest) };
		if (BitrateKbps > std::numeric_limits<uint16_t>::max())
		{
			EG_LOG(LogDefault, Log, "%s : BitrateKbps is %u . Clamping to 65535.", __FUNCTION__, BitrateKbps);
			BitrateKbps = std::numeric_limits<uint16_t>::max();
		}

		*reinterpret_cast<uint16_t*>(&Buf[1]) = static_cast<uint16_t>(BitrateKbps);
		Connection.Send(Buf, sizeof(Buf));
	}

	{
		uint8_t Buf[1 + sizeof(uint16_t)] = { static_cast<uint8_t>(EToUE4Msg::MaxFpsRequest) };
		*reinterpret_cast<uint16_t*>(&Buf[1]) = static_cast<uint16_t>(Framerate);
		Connection.Send(Buf, sizeof(Buf));
	}
}

void FUE4Connection::Send(const void* Data, uint32_t Size)
{
	Connection.Send(Data, Size);
}

uint32_t FUE4Connection::OnRead(const uint8_t* Data, uint32_t Size)
{
	if (!bStreamingStarted)
		return Size; // drop data as there's no clients to receive it

	using FTimestamp = uint64_t;
	using FPayloadSize = uint32_t;

	if (Size < sizeof(FTimestamp) + sizeof(EToProxyMsg) + sizeof(FPayloadSize))
		return 0;

	const uint8_t* Ptr = Data;  // pointer to current read pos in the buffer

	auto CaptureTimeMs = *reinterpret_cast<const FTimestamp*>(Ptr);
	Ptr += sizeof(CaptureTimeMs);

	auto PktType = *reinterpret_cast<const EToProxyMsg*>(Ptr);
	Ptr += sizeof(PktType);

	auto PayloadSize = *reinterpret_cast<const FPayloadSize*>(Ptr);
	Ptr += sizeof(PayloadSize);

	if (Ptr + PayloadSize > Data + Size)
		return 0;

	Observer.OnUE4Packet(PktType, Ptr, PayloadSize);

	Ptr += PayloadSize;

	return static_cast<uint32_t>(Ptr - Data);
}
