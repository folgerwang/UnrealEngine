// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionClient.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "Framework/Application/SlateApplication.h"	
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Sockets.h"
#include "RemoteSession.h"


DECLARE_CYCLE_STAT(TEXT("RSClientTick"), STAT_RDClientTick, STATGROUP_Game);

FRemoteSessionClient::FRemoteSessionClient(const TCHAR* InHostAddress)
{
	HostAddress = InHostAddress;
	ConnectionAttemptTimer = FLT_MAX;		// attempt a connection asap
	TimeConnectionAttemptStarted = 0;
    ConnectionTimeout = 5;

	IsConnecting = false;

	if (HostAddress.Contains(TEXT(":")) == false)
	{
		HostAddress += FString::Printf(TEXT(":%d"), (int32)IRemoteSessionModule::kDefaultPort);
	}

	UE_LOG(LogRemoteSession, Display, TEXT("Will attempt to connect to %s.."), *HostAddress);
}

FRemoteSessionClient::~FRemoteSessionClient()
{
	Close();
}

bool FRemoteSessionClient::IsConnected() const
{
	// this is to work-around the UE BSD socket implt always saying
	// things are connected for the first 5 secs...
	return FRemoteSessionRole::IsConnected() && Connection->GetPacketsReceived() > 0;
}

void FRemoteSessionClient::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_RDClientTick);

	if (IsConnected() == false)
	{
		if (IsConnecting == false)
		{
			const double TimeSinceLastAttempt = FPlatformTime::Seconds() - TimeConnectionAttemptStarted;

			if (TimeSinceLastAttempt >= 5.0)
			{
				StartConnection();
			}
		}

		if (IsConnecting)
		{
			CheckConnection();
		}
	}

	FRemoteSessionRole::Tick(DeltaTime);
}

void  FRemoteSessionClient::StartConnection()
{
	check(IsConnecting == false);

	Close();

	if (IBackChannelTransport* Transport = IBackChannelTransport::Get())
	{
		Connection = Transport->CreateConnection(IBackChannelTransport::TCP);

		if (Connection.IsValid())
		{
			if (Connection->Connect(*HostAddress))
			{
				IsConnecting = true;
				check(Connection->GetSocket());
			}
		}
	}

	TimeConnectionAttemptStarted = FPlatformTime::Seconds();
}

void FRemoteSessionClient::CheckConnection()
{
	check(IsConnected() == false && IsConnecting == true);
	check(Connection->GetSocket());

	// success indicates that our check was successful, if our connection was successful then
	// the delegate code is called
	bool Success = Connection->WaitForConnection(0, [this](auto InConnection) {
		int32 WantedSize = 4 * 1024 * 1024;
		int32 ActualSize(0);

		Connection->GetSocket()->SetReceiveBufferSize(WantedSize, ActualSize);

		OSCConnection = MakeShareable(new FBackChannelOSCConnection(Connection.ToSharedRef()));

		Channels.Add(MakeShareable(new FRemoteSessionInputChannel(ERemoteSessionChannelMode::Send, OSCConnection)));
		Channels.Add(MakeShareable(new FRemoteSessionXRTrackingChannel(ERemoteSessionChannelMode::Send, OSCConnection)));
		Channels.Add(MakeShareable(new FRemoteSessionFrameBufferChannel(ERemoteSessionChannelMode::Receive, OSCConnection)));

		UE_LOG(LogRemoteSession, Log, TEXT("Connected to host at %s (ReceiveSize=%dkb)"), *HostAddress, ActualSize / 1024);

		IsConnecting = false;

		OSCConnection->StartReceiveThread();
		//SetReceiveInBackground(true);

		return true;
	});

	const double TimeSpentConnecting = FPlatformTime::Seconds() - TimeConnectionAttemptStarted;

	if (IsConnected() == false)
	{
		if (Success == false || TimeSpentConnecting >= ConnectionTimeout)
		{
			IsConnecting = false;
			if (TimeSpentConnecting >= ConnectionTimeout)
			{
				UE_LOG(LogRemoteSession, Log, TEXT("Timing out connection attempt after %.02f seconds"), TimeSpentConnecting);	
			}
			else
			{
				UE_LOG(LogRemoteSession, Log, TEXT("Failed to check for connection. Aborting."));
			}

			Close();
			TimeConnectionAttemptStarted = FPlatformTime::Seconds();
		}
	}
}
