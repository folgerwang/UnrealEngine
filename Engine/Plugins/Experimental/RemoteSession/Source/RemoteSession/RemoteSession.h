// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "RemoteSessionRole.h"


REMOTESESSION_API DECLARE_LOG_CATEGORY_EXTERN(LogRemoteSession, Log, All);

class REMOTESESSION_API IRemoteSessionModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override = 0;
	virtual void ShutdownModule() override = 0;
	/** End IModuleInterface implementation */

public:

	enum
	{
		kDefaultPort = 2049
	};

public:
	/** Client implementation */

	/** Initialize a client that will attempt to connect to the provided address */
	virtual void InitClient(const TCHAR* RemoteAddress) = 0;

	/** Returns true/false based on the connection state of the client */
	virtual bool IsClientConnected() const = 0;

	/** Stops the client. After this InitClient() must be called if a new connection is desired */
	virtual void StopClient() = 0;

	/** Returns a reference to the client role (if any) */
	virtual TSharedPtr<IRemoteSessionRole>		GetClient() const = 0;

public:
	/** Server implementation */

	/** Starts a RemoteSession server that listens for clients on the provided port */
	virtual void InitHost(const int16 Port=0) = 0;

	/** Returns true/false based on the running state of the host server */
	virtual bool IsHostRunning() const = 0;

	/** Returns true/false if a client is connected */
	virtual bool IsHostConnected() const = 0;

	/** Stops the server, after this InitHost() must be called if a new connection is desired */
	virtual void StopHost() = 0;

	/** Returns a reference to the server role (if any) */
	virtual TSharedPtr<IRemoteSessionRole>		GetHost() const = 0;

};
