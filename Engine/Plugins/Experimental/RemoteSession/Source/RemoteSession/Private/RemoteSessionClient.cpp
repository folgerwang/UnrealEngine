// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionClient.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "Framework/Application/SlateApplication.h"	
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "Channels/RemoteSessionARCameraChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Sockets.h"
#include "RemoteSession.h"
#include "Async/Async.h"

DECLARE_CYCLE_STAT(TEXT("RSClientTick"), STAT_RDClientTick, STATGROUP_Game);

#define RS_TIMEOUT_IS_ERROR 0

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
		if (IsConnecting == false && HasError()  == false)
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
	bool Success = Connection->WaitForConnection(0, [this](auto InConnection)
	{
		CreateOSCConnection(Connection.ToSharedRef());
		
		UE_LOG(LogRemoteSession, Log, TEXT("Connected to host at %s"), *HostAddress);

		IsConnecting = false;

		//SetReceiveInBackground(true);

		return true;
	});

	const double TimeSpentConnecting = FPlatformTime::Seconds() - TimeConnectionAttemptStarted;

	if (IsConnected() == false)
	{
		if (Success == false || TimeSpentConnecting >= ConnectionTimeout)
		{
			IsConnecting = false;
			
			FString Msg;
			
			if (TimeSpentConnecting >= ConnectionTimeout)
			{
				Msg = FString::Printf(TEXT("Timing out connection attempt after %.02f seconds"), TimeSpentConnecting);
			}
			else
			{
				Msg = TEXT("Failed to check for connection. Aborting.");
			}
			
			UE_LOG(LogRemoteSession, Log, TEXT("%s"), *Msg);
			
#if RS_TIMEOUT_IS_ERROR
			CloseWithError(*Msg);
#else
			Close();
#endif
			TimeConnectionAttemptStarted = FPlatformTime::Seconds();
		}
	}
}

void FRemoteSessionClient::OnBindEndpoints()
{
	FRemoteSessionRole::OnBindEndpoints();
}

void FRemoteSessionClient::OnChannelSelection(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	TMap<FString, ERemoteSessionChannelMode> DesiredChannels;
	
	const int NumChannels = Message.GetArgumentCount() / 2;
	
	for (int i = 0; i < NumChannels; i++)
	{
		FString ChannelName;
		int32 ChannelMode;
		
		Message.Read(ChannelName);
		Message.Read(ChannelMode);
		
		if (ChannelName.Len())
		{
			DesiredChannels.Add(ChannelName, (ERemoteSessionChannelMode)ChannelMode);
		}
		else
		{
			UE_LOG(LogRemoteSession, Error, TEXT("Failed to read channel from ChannelSelection message!"));
		}
	}
	
	// Need to create channels on the main thread
	AsyncTask(ENamedThreads::GameThread, [this, DesiredChannels]
	{
		CreateChannels(DesiredChannels);
	});
}
