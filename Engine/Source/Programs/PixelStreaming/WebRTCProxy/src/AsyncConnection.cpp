// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AsyncConnection.h"
#include "Logging.h"

FAsyncConnection::FAsyncConnection(const std::string& ConnectionName, IAsyncConnectionObserver& Observer) :
	Name(ConnectionName),
	Observer(Observer)
{}

void FAsyncConnection::Connect(const std::string& IP, uint16_t Port)
{
	SocketAddress.SetIP(IP);
	SocketAddress.SetPort(Port);

	verify(!Socket || Socket->GetState() == rtc::AsyncSocket::CS_CLOSED);
	Socket.reset(rtc::ThreadManager::Instance()->CurrentThread()->socketserver()->CreateAsyncSocket(SocketAddress.family(), SOCK_STREAM));

	Socket->SignalConnectEvent.connect(this, &FAsyncConnection::OnConnect);
	Socket->SignalReadEvent.connect(this, &FAsyncConnection::OnRead);
	Socket->SignalCloseEvent.connect(this, &FAsyncConnection::OnClose);

	bReconnect = true;
	
	EG_LOG(LogDefault, Log, "Connecting to %s %s:%d", Name.c_str(), IP.c_str(), Port);
	if (Socket->Connect(SocketAddress) == SOCKET_ERROR)
	{
		OnClose(Socket.get(), SOCKET_ERROR);
	}
}

void FAsyncConnection::Disconnect()
{
	bReconnect = false;
	Socket->Close();
}

void FAsyncConnection::OnConnect(rtc::AsyncSocket*)
{
	EG_LOG(LogDefault, Log, "Connected to %s", Name.c_str());
	bReportDisconnection = true;
	Observer.OnConnect();
}

void FAsyncConnection::OnClose(rtc::AsyncSocket*, int Err)
{
	if (bReportDisconnection)
	{
		EG_LOG(LogDefault, Warning, "Disconnected from %s, error %d. Reconnecting...", Name.c_str(), Err);
		bReportDisconnection = false;
		Observer.OnDisconnect(Err);
	}

	if (!bReconnect)
		return;

	while (Socket->Connect(SocketAddress) == SOCKET_ERROR)
	{
	}
}

void FAsyncConnection::OnRead(rtc::AsyncSocket*)
{
	do
	{
		int ReceivedBytes = Socket->Recv(TmpReadBuffer, sizeof(TmpReadBuffer), nullptr);
		if (ReceivedBytes <= 0)
		{
			break;
		}
		ReadBuffer.insert(ReadBuffer.end(), TmpReadBuffer, TmpReadBuffer + ReceivedBytes);
	} while (true);

	uint32_t Consumed = 0;
	while (!ReadBuffer.empty() &&
		(Consumed = Observer.OnRead(&ReadBuffer.front(), static_cast<uint32_t>(ReadBuffer.size()))) != 0)
	{
		ReadBuffer.erase(ReadBuffer.begin(), ReadBuffer.begin() + Consumed);
	}
}

void FAsyncConnection::Send(const void* Data, uint32_t Size)
{
	Socket->Send(Data, Size);
}
