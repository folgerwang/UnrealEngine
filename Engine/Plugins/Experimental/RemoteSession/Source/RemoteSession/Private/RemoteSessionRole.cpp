// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "RemoteSessionRole.h"
#include "RemoteSession.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "Channels/RemoteSessionARCameraChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "Async/Async.h"
#include "GeneralProjectSettings.h"
#include "ARBlueprintLibrary.h"

DEFINE_LOG_CATEGORY(LogRemoteSession);

FRemoteSessionRole::~FRemoteSessionRole()
{
	Close();
}

void FRemoteSessionRole::Close()
{
	// order is specific since OSC uses the connection, and
	// dispatches to channels
	StopBackgroundThread();
	OSCConnection = nullptr;
	Connection = nullptr;
	ClearChannels();
}

void FRemoteSessionRole::CloseWithError(const FString& Message)
{
	ErrorMessage = Message;
	Close();
}

void FRemoteSessionRole::Tick(float DeltaTime)
{
	if (OSCConnection.IsValid())
	{
		if (OSCConnection->IsConnected())
		{
			if (ThreadRunning == false && OSCConnection->IsThreaded() == false)
			{
				OSCConnection->ReceivePackets();
			}

			for (auto& Channel : Channels)
			{
				Channel->Tick(DeltaTime);
			}
		}
		else
		{
			UE_LOG(LogRemoteSession, Warning, TEXT("Connection %s has disconnected."), *OSCConnection->GetDescription());
			OSCConnection = nullptr;
		}
	}
}

void FRemoteSessionRole::SetReceiveInBackground(bool bValue)
{
	if (bValue && !ThreadRunning)
	{
		StartBackgroundThread();
	}
	else if (!bValue && ThreadRunning)
	{
		StopBackgroundThread();
	}
}

void FRemoteSessionRole::StartBackgroundThread()
{
	check(ThreadRunning == false);
	ThreadExitRequested = false;
	ThreadRunning = true;

	FRunnableThread* Thread = FRunnableThread::Create(this, TEXT("RemoteSessionClientThread"), 
		1024 * 1024, 
		TPri_AboveNormal);
}

bool FRemoteSessionRole::IsConnected() const
{
    // just check this is valid, when it's actually disconnected we do some error
    // handling and clean this up
    return OSCConnection.IsValid();
}

uint32 FRemoteSessionRole::Run()
{
	/* Not used and likely to be removed! */
	double LastTick = FPlatformTime::Seconds();

	while (ThreadExitRequested == false)
	{
		const double DeltaTime = FPlatformTime::Seconds() - LastTick;

		if (OSCConnection.IsValid() == false || OSCConnection->IsConnected() == false)
		{
			FPlatformProcess::SleepNoStats(0);
			continue;
		}

		OSCConnection->ReceivePackets(1);
		LastTick = FPlatformTime::Seconds();
	}

	ThreadRunning = false;
	return 0;
}

void FRemoteSessionRole::StopBackgroundThread()
{
	if (ThreadRunning == false)
	{
		return;
	}

	ThreadExitRequested = true;

	while (ThreadRunning)
	{
		FPlatformProcess::SleepNoStats(0);
	}
}

void FRemoteSessionRole::CreateOSCConnection(TSharedRef<IBackChannelConnection> InConnection)
{
	OSCConnection = MakeShareable(new FBackChannelOSCConnection(InConnection));
	
	auto Delegate = FBackChannelDispatchDelegate::FDelegate::CreateRaw(this, &FRemoteSessionRole::OnVersionCheck);
	OSCConnection->AddMessageHandler(TEXT("/Version"),Delegate);
	
	Delegate = FBackChannelDispatchDelegate::FDelegate::CreateRaw(this, &FRemoteSessionRole::OnCreateChannels);
	OSCConnection->AddMessageHandler(*GetChannelSelectionEndPoint(), Delegate);

	OSCConnection->StartReceiveThread();
	
	SendVersion();
}

FString FRemoteSessionRole::GetVersion() const
{
	return REMOTE_SESSION_VERSION_STRING;
	//return FString::Printf(TEXT("%d"), FMath::RandHelper(1000));
}

void FRemoteSessionRole::SendVersion()
{
	// now ask the client to start these channels
	FBackChannelOSCMessage Msg(TEXT("/Version"));
	Msg.Write(GetVersion());
	OSCConnection->SendPacket(Msg);
}

void FRemoteSessionRole::OnVersionCheck(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	FString VersionString;
	
	Message.Read(VersionString);
	
	FString VersionErrorMessage;
	
	if (VersionString.Len() == 0)
	{
		VersionErrorMessage = TEXT("FRemoteSessionRole: Failed to read version string");
	}
	
	if (VersionString != GetVersion())
	{
		VersionErrorMessage = FString::Printf(TEXT("FRemoteSessionRole: Version mismatch. Local=%s, Remote=%s"), *GetVersion(), *VersionString);
	}
	
	if (VersionErrorMessage.Len() > 0)
	{
		UE_LOG(LogRemoteSession, Error, TEXT("%s"), *VersionErrorMessage);
		UE_LOG(LogRemoteSession, Log, TEXT("FRemoteSessionRole: Closing connection due to version mismatch"));
		CloseWithError(VersionErrorMessage);
	}
	else
	{
		// Run on the main thread incase our derived states touch anything there (Create Channels does)
		AsyncTask(ENamedThreads::GameThread, [this]
		{
			UE_LOG(LogRemoteSession, Log, TEXT("FRemoteSessionRole: Binding endpoints and creating channels"));
			OnBindEndpoints();
			OnCreateChannels();
		});
	}
}

void FRemoteSessionRole::OnCreateChannels(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	OnChannelSelection(Message, Dispatch);
}

void FRemoteSessionRole::OnChannelSelection(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
}

void FRemoteSessionRole::OnBindEndpoints()
{
}

void FRemoteSessionRole::OnCreateChannels()
{
}

void FRemoteSessionRole::CreateChannel(const FString& InChannelName, ERemoteSessionChannelMode InMode)
{
	TSharedPtr<IRemoteSessionChannel> NewChannel;
	
	if (InChannelName == FRemoteSessionInputChannel::StaticType())
	{
		NewChannel = MakeShareable(new FRemoteSessionInputChannel(InMode, OSCConnection));
	}
	else if (InChannelName == FRemoteSessionFrameBufferChannel::StaticType())
	{
		NewChannel = MakeShareable(new FRemoteSessionFrameBufferChannel(InMode, OSCConnection));
	}
	else if (InChannelName == FRemoteSessionXRTrackingChannel::StaticType())
	{
		bool IsSupported = (InMode == ERemoteSessionChannelMode::Read) || UARBlueprintLibrary::IsSessionTypeSupported(EARSessionType::World);
		if (IsSupported)
		{
			NewChannel = MakeShareable(new FRemoteSessionXRTrackingChannel(InMode, OSCConnection));
		}
		else
		{
			UE_LOG(LogRemoteSession, Warning, TEXT("FRemoteSessionXRTrackingChannel does not support sending on this platform/device"));
		}
	}
	else if (InChannelName == FRemoteSessionARCameraChannel::StaticType())
	{
		// Client side sending only works on iOS with Android coming in the future
		bool bSessionTypeSupported = UARBlueprintLibrary::IsSessionTypeSupported(EARSessionType::World);
		bool IsSupported = (InMode == ERemoteSessionChannelMode::Read) || (PLATFORM_IOS && bSessionTypeSupported);
		if (IsSupported)
		{
			NewChannel = MakeShareable(new FRemoteSessionARCameraChannel(InMode, OSCConnection));
		}
		else
		{
			UE_LOG(LogRemoteSession, Warning, TEXT("FRemoteSessionARCameraChannel does not support sending on this platform/device"));
		}
	}
	
	if (NewChannel.IsValid())
	{
		UE_LOG(LogRemoteSession, Log, TEXT("Created Channel %s with mode %d"), *InChannelName, (int)InMode);
		Channels.Add(NewChannel);
	}
	else
	{
		UE_LOG(LogRemoteSession, Error, TEXT("Requested Channel %s was not recognized"), *InChannelName);
	}
}

void FRemoteSessionRole::CreateChannels(const TMap<FString, ERemoteSessionChannelMode>& ChannelMap)
{
	ClearChannels();
	
	for (const auto& KP : ChannelMap)
	{
		TSharedPtr<IRemoteSessionChannel> NewChannel;
		
		CreateChannel(KP.Key, KP.Value);
	}
}

void FRemoteSessionRole::AddChannel(const TSharedPtr<IRemoteSessionChannel>& InChannel)
{
	Channels.Add(InChannel);
}

void FRemoteSessionRole::ClearChannels()
{
	Channels.Empty();
}

TSharedPtr<IRemoteSessionChannel> FRemoteSessionRole::GetChannel(const FString& InType)
{
	TSharedPtr<IRemoteSessionChannel> Channel;

	TSharedPtr<IRemoteSessionChannel>* FoundChannel = Channels.FindByPredicate([InType](auto Item) {
		return Item->GetType() == InType;
	});

	if (FoundChannel)
	{
		Channel = *FoundChannel;
	}

	return Channel;
}
