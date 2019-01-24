// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "Sockets.h"
#include "BSDSockets/SocketsBSD.h"

class FInternetAddr;

#if PLATFORM_HAS_BSD_IPV6_SOCKETS


/**
 * Implements a BSD network socket on IOS.
 */
class FSocketBSDIOS
	: public FSocketBSD
{
public:

	FSocketBSDIOS(SOCKET InSocket, ESocketType InSocketType, const FString& InSocketDescription, ESocketProtocolFamily InSocketProtocol, ISocketSubsystem* InSubsystem)
		:FSocketBSD(InSocket, InSocketType, InSocketDescription, InSocketProtocol, InSubsystem)
	{
	}

	virtual ~FSocketBSDIOS()
	{	
		FSocketBSD::Close();
	}

};

#endif
