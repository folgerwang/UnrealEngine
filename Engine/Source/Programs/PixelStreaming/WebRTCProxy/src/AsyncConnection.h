// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

// callback interface for `FAsyncConnection`
struct IAsyncConnectionObserver
{
	virtual ~IAsyncConnectionObserver() {}

	// reports succeeded connection
	virtual void OnConnect() = 0;
	// reports incoming data
	virtual uint32_t OnRead(const uint8_t* Data, uint32_t Size) = 0;
	// reports disconnection
	virtual void OnDisconnect(int Err) = 0;
};

// async TCP client connection
// automatically reconnects on disconnection except disconnection was explicit by the caller
class FAsyncConnection : public sigslot::has_slots<>
{
public:
	// `ConnectionName` is used for logging
	FAsyncConnection(const std::string& ConnectionName, IAsyncConnectionObserver& Observer);

	// keeps connecting until succeeded, success is reported by `IAsyncConnectionObserver::OnConnect()`
	void Connect(const std::string& IP, uint16_t Port);
	// disconnects and calls `IAsyncConnectionObserver::OnDisconnect()`
	void Disconnect();

	// sends data asynchronously (?) but doesn't report when done
	void Send(const void* Data, uint32_t Size);

private:
	void OnConnect(rtc::AsyncSocket*);
	void OnRead(rtc::AsyncSocket*);
	void OnClose(rtc::AsyncSocket*, int Err);

private:
	std::string Name;
	IAsyncConnectionObserver& Observer;
	rtc::SocketAddress SocketAddress;
	std::unique_ptr<rtc::AsyncSocket> Socket;

	std::atomic<bool> bReconnect = false; // automatically try to reconnect on disconnection
	std::atomic<bool> bReportDisconnection = false;  // to avoid reporting disconnection on repeated connection attempts

	uint8_t TmpReadBuffer[0xFFFF];
	std::vector<uint8_t> ReadBuffer;
};
