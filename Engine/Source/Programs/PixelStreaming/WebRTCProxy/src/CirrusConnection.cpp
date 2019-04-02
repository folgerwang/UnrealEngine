// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CirrusConnection.h"
#include "Logging.h"

using PixelStreamingProtocol::ECirrusToProxyMsg;
using PixelStreamingProtocol::EProxyToCirrusMsg;
using FMsgSize = uint32_t;

FCirrusConnection::FCirrusConnection(ICirrusConnectionObserver& Observer)
    : Observer(Observer)
    , Connection("Cirrus", *this)
{}

void FCirrusConnection::Connect(const std::string& IP, uint16_t Port)
{
	Connection.Connect(IP, Port);
}

void FCirrusConnection::Disconnect()
{
	Connection.Disconnect();
}

void FCirrusConnection::OnDisconnect(int Err)
{
	Observer.OnCirrusDisconnected();
}

uint32_t FCirrusConnection::OnRead(const uint8_t* Data, uint32_t Size)
{
	if (Size < (sizeof(ECirrusToProxyMsg) + sizeof(FMsgSize)))
	{
		return 0;
	}

	const uint8_t* Ptr = Data;  // pointer to current read pos in the buffer

	ECirrusToProxyMsg MsgId = *reinterpret_cast<const ECirrusToProxyMsg*>(Ptr);
	Ptr += sizeof(ECirrusToProxyMsg);

	checkf(MsgId < ECirrusToProxyMsg::count,
		TEXT("Invalid message ID received from Cirrus: %u"), static_cast<uint32_t>(MsgId));

	auto GetString = [&Ptr, Data, Size](std::string& res) -> bool
	{
		if (Ptr + sizeof(FMsgSize) > Data + Size)
			return false;

		FMsgSize MsgSize = *reinterpret_cast<const FMsgSize*>(Ptr);
		Ptr += sizeof(FMsgSize);
		if ((Ptr + MsgSize) > (Data + Size))
			return false;

		res.assign(Ptr, Ptr + MsgSize);
		Ptr += MsgSize;
		return true;
	};

	auto GetClientId = [&Ptr, Data, Size](FClientId& res) -> bool
	{
		if (Ptr + sizeof(FClientId) > Data + Size)
			return false;

		res = *reinterpret_cast<const FClientId*>(Ptr);
		Ptr += sizeof(FClientId);
		return true;
	};

	switch (MsgId)
	{
	case ECirrusToProxyMsg::config:
	{
		std::string Config;
		if (!GetString(Config))
			return 0;

		Observer.OnCirrusConfig(Config);
		break;
	}
	case ECirrusToProxyMsg::offer:
	{
		FClientId ClientId;
		if (!GetClientId(ClientId))
			return 0;

		std::string Msg;
		if (!GetString(Msg))
			return 0;

		Observer.OnOffer(ClientId, Msg);
		break;
	}
	case ECirrusToProxyMsg::iceCandidate:
	{
		FClientId ClientId;
		if (!GetClientId(ClientId))
			return 0;

		std::string Msg;
		if (!GetString(Msg))
			return 0;

		Observer.OnIceCandidate(ClientId, Msg);
		break;
	}
	case ECirrusToProxyMsg::clientDisconnected:
	{
		FClientId ClientId;
		if (!GetClientId(ClientId))
			return 0;

		Observer.OnClientDisconnected(ClientId);
		break;
	}
	default:
		check(false);
	}

	return static_cast<uint32_t>(Ptr - Data);
}

void FCirrusConnection::SendAnswer(FClientId Client, const std::string& Answer)
{
	SendStringMsg(EProxyToCirrusMsg::answer, Client, Answer);
}

void FCirrusConnection::SendIceCandidate(FClientId Client, const std::string& IceCandidate)
{
	SendStringMsg(EProxyToCirrusMsg::iceCandidate, Client, IceCandidate);
}

void FCirrusConnection::SendDisconnectClient(FClientId Client)
{
	auto MsgId = EProxyToCirrusMsg::disconnectClient;
	Connection.Send(&MsgId, sizeof(MsgId));
	Connection.Send(&Client, sizeof(Client));
}

void FCirrusConnection::SendStringMsg(EProxyToCirrusMsg MsgId, FClientId Client, const std::string& Msg)
{
	Connection.Send(&MsgId, sizeof(MsgId));
	Connection.Send(&Client, sizeof(Client));
	FMsgSize MsgSize = static_cast<FMsgSize>(Msg.size());
	Connection.Send(&MsgSize, sizeof(FMsgSize));
	Connection.Send(Msg.data(), MsgSize);
}
