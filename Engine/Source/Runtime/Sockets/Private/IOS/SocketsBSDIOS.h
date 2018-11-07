// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

	FSocketBSDIOS(SOCKET InSocket, ESocketType InSocketType, const FString& InSocketDescription, ISocketSubsystem* InSubsystem)
		:FSocketBSD(InSocket, InSocketType, InSocketDescription, InSubsystem)
	{
	}

	virtual ~FSocketBSDIOS()
	{	
		FSocketBSD::Close();
	}

};

#endif
