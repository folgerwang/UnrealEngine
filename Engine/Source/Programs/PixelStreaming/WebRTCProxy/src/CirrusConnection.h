// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "AsyncConnection.h"

// callback interface for `FCirrusConnection`
class ICirrusConnectionObserver
{
public:
	virtual ~ICirrusConnectionObserver()
	{}

	virtual void OnCirrusConfig(const std::string& Config) = 0;
	virtual void OnOffer(FClientId Client, const std::string& Offer) = 0;
	virtual void OnIceCandidate(FClientId Client, const std::string& IceCandidate) = 0;
	virtual void OnQualityOwnership(FClientId Client) = 0;
	virtual void OnClientDisconnected(FClientId Client) = 0;
	virtual void OnCirrusDisconnected() = 0;
};

/**
 * Communication with Cirrus.
 * Sends messages to Cirrus and calls `ICirrusConnectionObserver` on incoming messages.
 * Reconnects after losing connection
 */
class FCirrusConnection : public IAsyncConnectionObserver
{
public:
	explicit FCirrusConnection(ICirrusConnectionObserver& Observer);

	void Connect(const std::string& IP, uint16_t Port);
	void Disconnect();

	// Messages to Cirrus
	void SendAnswer(FClientId Client, const std::string& Answer);
	void SendIceCandidate(FClientId Client, const std::string& IceCandidate);
	void SendDisconnectClient(FClientId Client);

private:
	// IAsyncConnectionObserver impl
	void OnConnect() override {}
	uint32_t OnRead(const uint8_t* Data, uint32_t Size) override;
	void OnDisconnect(int Err) override;

	void SendStringMsg(PixelStreamingProtocol::EProxyToCirrusMsg MsgId, FClientId Client, const std::string& Msg);

private:
	ICirrusConnectionObserver& Observer;
	FAsyncConnection Connection;
};
