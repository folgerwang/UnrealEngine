// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ConcertUIExtension.h"


/**
 * Interface for the Concert UI Core module.
 */
class IConcertUICoreModule : public IModuleInterface
{
public:
	/** Get the ConcertUICore module */
	static IConcertUICoreModule& Get()
	{
		static const FName ModuleName = TEXT("ConcertUICore");
		return FModuleManager::Get().GetModuleChecked<IConcertUICoreModule>(ModuleName);
	}

	/** Get the extension point for status area buttons in the Concert Browser */
	virtual FOnConcertUIStatusButtonExtension& GetConcertBrowserStatusButtonExtension() = 0;

	/** Get the extension point for server list buttons in the Concert Browser */
	virtual FOnConcertUIServerButtonExtension& GetConcertBrowserServerButtonExtension() = 0;

	/** Get the extension point for session list buttons in the Concert Browser */
	virtual FOnConcertUISessionButtonExtension& GetConcertBrowserSessionButtonExtension() = 0;

	/** Get the extension point for client list buttons in the Concert Browser */
	virtual FOnConcertUIClientButtonExtension& GetConcertBrowserClientButtonExtension() = 0;
};
