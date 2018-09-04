// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"
#include "AsyncConnection.h"

// callback interface for `FUE4Connection`
struct IUE4ConnectionObserver
{
	virtual ~IUE4ConnectionObserver() {}

	virtual void OnUE4Connected() = 0;
	virtual void OnUE4Disconnected() = 0;
	// reports incoming complete packet from UE4 preserving packet boundaries
	virtual void OnUE4Packet(PixelStreamingProtocol::EToProxyMsg PktType, const void* Pkt, uint32_t Size) = 0;
};

// TCP client connection to UE4, manages UE4 <-> Proxy protocol
// automatically reconnects on disconnection
class FUE4Connection: public IAsyncConnectionObserver
{
public:
	explicit FUE4Connection(IUE4ConnectionObserver& Observer);

	// connects until succeeded
	void Connect(const std::string& IP, uint16_t Port);

	// messages to UE4
	void StartStreaming();
	void StopStreaming();
	void ForceKeyFrame();
	void SetRate(uint32_t BitrateKbps, uint32_t Framerate);
	// generic send for passing messages received from clients
	void Send(const void* Data, uint32_t Size);

private:
	// IAsyncConnectionObserver impl
	void OnConnect() override;
	uint32_t OnRead(const uint8_t* Data, uint32_t Size) override;
	void OnDisconnect(int Err) override;

private:
	IUE4ConnectionObserver& Observer;
	FAsyncConnection Connection;
	std::atomic<bool> bStreamingStarted = false;
};
