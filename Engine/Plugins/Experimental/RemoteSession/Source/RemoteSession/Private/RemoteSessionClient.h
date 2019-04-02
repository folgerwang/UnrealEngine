// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionRole.h"

class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;

class FRemoteSessionClient : public FRemoteSessionRole
{
public:

	FRemoteSessionClient(const TCHAR* InHostAddress);
	~FRemoteSessionClient();

	virtual void Tick(float DeltaTime) override;

	virtual bool IsConnected() const override;

protected:

	virtual void	OnBindEndpoints() override;
	
	void 			StartConnection();
	void 			CheckConnection();
	
	void OnChannelSelection(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch) override;

	FString				HostAddress;
	
	bool				IsConnecting;
    float               ConnectionTimeout;

	double				ConnectionAttemptTimer;
	double				TimeConnectionAttemptStarted;
	
	FDelegateHandle		ChannelCallbackHandle;
};
