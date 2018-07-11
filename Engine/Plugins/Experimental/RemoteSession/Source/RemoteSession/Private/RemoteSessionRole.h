// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSession/RemoteSessionRole.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"

#include "Tickable.h"

class FBackChannelOSCConnection;
enum class ERemoteSessionChannelMode;

class FRemoteSessionRole : public IRemoteSessionRole, FRunnable
{
public:

	virtual ~FRemoteSessionRole();

	virtual void Close();
	
	virtual void CloseWithError(const FString& Message);

	virtual bool IsConnected() const;
	
	virtual bool HasError() const { return ErrorMessage.Len() > 0; }
	
	virtual FString GetErrorMessage() const { return ErrorMessage; }

	virtual void Tick( float DeltaTime );

	virtual TSharedPtr<IRemoteSessionChannel> GetChannel(const FString& Type) override;

	void			SetReceiveInBackground(bool bValue);

protected:

	void			StartBackgroundThread();
	void			StopBackgroundThread();

	uint32			Run();
	
	void			CreateOSCConnection(TSharedRef<IBackChannelConnection> InConnection);
	
	FString			GetVersion() const;
	void			SendVersion();
	void 			OnVersionCheck(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch);
	void			OnCreateChannels(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch);
	
	virtual void	OnBindEndpoints();
	virtual void	OnCreateChannels();
	virtual void	OnChannelSelection(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch);
	
	void 			CreateChannels(const TMap<FString, ERemoteSessionChannelMode>& ChannelMap);
	void 			CreateChannel(const FString& InChannelList, ERemoteSessionChannelMode InRole);
	
	
	void	AddChannel(const TSharedPtr<IRemoteSessionChannel>& InChannel);
	void	ClearChannels();
	
	FString GetChannelSelectionEndPoint() const
	{
		return TEXT("/ChannelSelection");
	}

protected:
	
	TSharedPtr<IBackChannelConnection>	Connection;

	TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> OSCConnection;

private:
	
	FString					ErrorMessage;
	
	TArray<TSharedPtr<IRemoteSessionChannel>> Channels;
	FThreadSafeBool			ThreadExitRequested;
	FThreadSafeBool			ThreadRunning;
};
