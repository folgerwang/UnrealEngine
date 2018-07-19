// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "BSDSockets/IPAddressBSD.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

class FInternetAddrBSDIOS : public FInternetAddrBSD
{

public:

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override
	{
		UE_LOG(LogSockets, Verbose, TEXT("SetBroadcastAddress() FInternetAddrBSDIOS"));
		SetIp(INADDR_BROADCAST);
		SetPort(0);
	}

	// TODO: Determine if we need to override the others.
};

#endif
