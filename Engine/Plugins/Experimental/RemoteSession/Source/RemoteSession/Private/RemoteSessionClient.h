// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionRole.h"


class FRemoteSessionClient : public FRemoteSessionRole
{
public:

	FRemoteSessionClient(const TCHAR* InHostAddress);
	~FRemoteSessionClient();

	virtual void Tick(float DeltaTime) override;

	virtual bool IsConnected() const override;

protected:

	void StartConnection();
	void CheckConnection();

	FString				HostAddress;
	
	bool				IsConnecting;
    float               ConnectionTimeout;

	double				ConnectionAttemptTimer;
	double				TimeConnectionAttemptStarted;
};
