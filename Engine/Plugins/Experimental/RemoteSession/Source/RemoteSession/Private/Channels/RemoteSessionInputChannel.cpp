// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/RemoteSessionInputChannel.h"
#include "Framework/Application/SlateApplication.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "MessageHandler/RecordingMessageHandler.h"



FRemoteSessionInputChannel::FRemoteSessionInputChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection)
	: IRemoteSessionChannel(InRole, InConnection)
{

	Connection = InConnection;
	Role = InRole;

	// if sending input replace the default message handler with a recording version, and set us as the
	// handler for that data 
	if (Role == ERemoteSessionChannelMode::Write)
	{
		DefaultHandler = FSlateApplication::Get().GetPlatformApplication()->GetMessageHandler();

		RecordingHandler = MakeShareable(new FRecordingMessageHandler(DefaultHandler.Pin()));

		RecordingHandler->SetRecordingHandler(this);

		FSlateApplication::Get().GetPlatformApplication()->SetMessageHandler(RecordingHandler.ToSharedRef());
	}
	else
	{
		TSharedRef<FGenericApplicationMessageHandler> DestinationHandler = FSlateApplication::Get().GetPlatformApplication()->GetMessageHandler();

		PlaybackHandler = MakeShareable(new FRecordingMessageHandler(DestinationHandler));
		
		auto Delegate = FBackChannelDispatchDelegate::FDelegate::CreateRaw(this, &FRemoteSessionInputChannel::OnRemoteMessage);
		MessageCallbackHandle = Connection->AddMessageHandler(TEXT("/MessageHandler/"), Delegate);
	}
}

FRemoteSessionInputChannel::~FRemoteSessionInputChannel()
{
	if (Role == ERemoteSessionChannelMode::Read)
	{
		// Remove the callback so it doesn't call back on an invalid this
		Connection->RemoveMessageHandler(TEXT("/MessageHandler/"), MessageCallbackHandle);
		MessageCallbackHandle.Reset();
	}

	// todo - is this ok? Might other things have changed the handler like we do?
	if (DefaultHandler.IsValid())
	{
		FSlateApplication::Get().GetPlatformApplication()->SetMessageHandler(DefaultHandler.Pin().ToSharedRef());
	}

	// should restore handler? What if something else changed it...
	if (RecordingHandler.IsValid())
	{
		RecordingHandler->SetRecordingHandler(nullptr);
	}
}


void FRemoteSessionInputChannel::SetPlaybackWindow(TWeakPtr<SWindow> InWindow, TWeakPtr<FSceneViewport> InViewport)
{
	PlaybackHandler->SetPlaybackWindow(InWindow, InViewport);
}

void FRemoteSessionInputChannel::SetInputRect(const FVector2D& TopLeft, const FVector2D& Extents)
{
	if (RecordingHandler.IsValid())
	{
		RecordingHandler->SetInputRect(TopLeft, Extents);
	}
}

void FRemoteSessionInputChannel::Tick(const float InDeltaTime)
{
	// everything happens via messaging.
}

void FRemoteSessionInputChannel::RecordMessage(const TCHAR* MsgName, const TArray<uint8>& Data)
{
	if (Connection.IsValid())
	{
		// send as blobs
		FString Path = FString::Printf(TEXT("/MessageHandler/%s"), MsgName);
		FBackChannelOSCMessage Msg(*Path);

		Msg.Write(Data);

		Connection->SendPacket(Msg);
	}
}

void FRemoteSessionInputChannel::OnRemoteMessage(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	FString MessageName = Message.GetAddress();
	MessageName.RemoveFromStart(TEXT("/MessageHandler/"));

	TArray<uint8> MsgData;
	Message << MsgData;

	PlaybackHandler->PlayMessage(*MessageName, MsgData);
}
