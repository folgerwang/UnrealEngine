// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IConcertUICoreModule.h"

class FConcertUICoreModule : public IConcertUICoreModule
{
public:
	FConcertUICoreModule() = default;
	virtual ~FConcertUICoreModule() {}

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual FOnConcertUIStatusButtonExtension& GetConcertBrowserStatusButtonExtension() override
	{
		return OnConcertBrowserStatusButtonExtensionDelegate;
	}

	virtual FOnConcertUIServerButtonExtension& GetConcertBrowserServerButtonExtension() override
	{
		return OnConcertBrowserServerButtonExtensionDelegate;
	}

	virtual FOnConcertUISessionButtonExtension& GetConcertBrowserSessionButtonExtension() override
	{
		return OnConcertBrowserSessionButtonExtensionDelegate;
	}

	virtual FOnConcertUIClientButtonExtension& GetConcertBrowserClientButtonExtension() override
	{
		return OnConcertBrowserClientButtonExtensionDelegate;
	}

private:
	/** Extension point for status area buttons in the Concert Browser */
	FOnConcertUIStatusButtonExtension OnConcertBrowserStatusButtonExtensionDelegate;

	/** Extension point for server list buttons in the Concert Browser */
	FOnConcertUIServerButtonExtension OnConcertBrowserServerButtonExtensionDelegate;

	/** Extension point for session list buttons in the Concert Browser */
	FOnConcertUISessionButtonExtension OnConcertBrowserSessionButtonExtensionDelegate;

	/** Extension point for client list buttons in the Concert Browser */
	FOnConcertUIClientButtonExtension OnConcertBrowserClientButtonExtensionDelegate;
};


IMPLEMENT_MODULE(FConcertUICoreModule, ConcertUICore);
