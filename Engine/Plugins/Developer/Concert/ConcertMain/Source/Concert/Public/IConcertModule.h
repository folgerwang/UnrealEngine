// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IConcertServer;
class IConcertClient;

typedef TSharedPtr<IConcertServer, ESPMode::ThreadSafe> IConcertServerPtr;
typedef TSharedPtr<IConcertClient, ESPMode::ThreadSafe> IConcertClientPtr;

class UConcertServerConfig;

/**
 * Interface for the Main Concert module.
 */
class IConcertModule : public IModuleInterface
{
public:
	/** Get the Concert module */
	static IConcertModule& Get()
	{
		static const FName ModuleName = TEXT("Concert");
		return FModuleManager::Get().GetModuleChecked<IConcertModule>(ModuleName);
	}

	/** Parse command line server settings and save them
	 * @param CommandLine the application command line arguments
	 * @return the server settings, modified or not by the command line
	 */
	virtual UConcertServerConfig* ParseServerSettings(const TCHAR* CommandLine) = 0;

	/** Get the server instance for Concert */
	virtual IConcertServerPtr GetServerInstance(/*Name*/) = 0;

	/** Get the client instance for Concert */
	virtual IConcertClientPtr GetClientInstance(/*Name*/) = 0;
};
