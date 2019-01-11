// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IConcertModule.h"

#include "UObject/Class.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"

#include "ConcertSettings.h"
#include "ConcertServer.h"
#include "ConcertClient.h"

/**
 * Implements the Concert module
 */
class FConcertModule : public IConcertModule
{
public:

	virtual void StartupModule() override
	{
		AppPreExitDelegateHandle = FCoreDelegates::OnPreExit.AddRaw(this, &FConcertModule::HandleAppPreExit);
	}

	virtual void ShutdownModule() override
	{
		// Unhook AppPreExit and call it
		if (AppPreExitDelegateHandle.IsValid())
		{
			FCoreDelegates::OnPreExit.Remove(AppPreExitDelegateHandle);
			AppPreExitDelegateHandle.Reset();
		}
		HandleAppPreExit();

		// Shutdown server instance, if any
		Server.Reset();

		// Shutdown client instance, if any
		Client.Reset();

		EndpointProvider.Reset();
	}

	// Server/Client shutdown is dependent on the UObject system which is currently shutdown on AppExit
	void HandleAppPreExit()
	{
		// if UObject system isn't initialized, skip shutdown
		if (!UObjectInitialized())
		{
			return;
		}

		if (Server.IsValid())
		{
			Server->Shutdown();
		}

		if (Client.IsValid())
		{
			Client->Shutdown();
		}
	}

	UConcertServerConfig* ParseServerSettings(const TCHAR* CommandLine)
	{
		UConcertServerConfig* ServerConfig = GetMutableDefault<UConcertServerConfig>();

		if (CommandLine != nullptr)
		{
			bool bSaveConfig = false;
			FString Arg;
			if (FParse::Value(CommandLine, TEXT("-CONCERTSESSION="), Arg))
			{
				bSaveConfig = true;
				ServerConfig->DefaultSessionName = Arg;
			}
			if (FParse::Value(CommandLine, TEXT("-CONCERTSAVESESSIONAS="), Arg))
			{
				bSaveConfig = true;
				ServerConfig->DefaultSessionSettings.SaveSessionAs = Arg;
			}
			if (FParse::Value(CommandLine, TEXT("-CONCERTSESSIONTORESTORE="), Arg))
			{
				bSaveConfig = true;
				ServerConfig->DefaultSessionSettings.SessionToRestore = Arg;
			}
			if (FParse::Value(CommandLine, TEXT("-CONCERTPROJECT="), Arg))
			{
				bSaveConfig = true;
				ServerConfig->DefaultSessionSettings.ProjectName = Arg;
			}
			if (FParse::Value(CommandLine, TEXT("-CONCERTVERSION="), Arg))
			{
				bSaveConfig = true;
				ServerConfig->DefaultSessionSettings.CompatibleVersion = Arg;
			}
			uint32 BaseRevision = 0;
			if (FParse::Value(CommandLine, TEXT("-CONCERTREVISION="), BaseRevision))
			{
				bSaveConfig = true;
				ServerConfig->DefaultSessionSettings.BaseRevision = BaseRevision;
			}

			// Ignore session restriction if argument is present
			ServerConfig->ServerSettings.bIgnoreSessionSettingsRestriction = FParse::Param(CommandLine, TEXT("CONCERTIGNORE"));

			// Clean server sessions working directory if argument is present
			ServerConfig->bCleanWorkingDir = FParse::Param(CommandLine, TEXT("CONCERTCLEAN"));
			
			if (bSaveConfig)
			{
				ServerConfig->SaveConfig();
			}
		}
		return ServerConfig;
	}

	virtual IConcertServerPtr GetServerInstance() override
	{
		if (Server.IsValid())
		{
			return Server.ToSharedRef();
		}
		Server = MakeShared<FConcertServer, ESPMode::ThreadSafe>();
		Server->SetEndpointProvider(GetEndpointProvider());
		return Server.ToSharedRef();
	}

	virtual IConcertClientPtr GetClientInstance() override
	{
		if (Client.IsValid())
		{
			return Client.ToSharedRef();
		}
		Client = MakeShared<FConcertClient, ESPMode::ThreadSafe>();
		Client->SetEndpointProvider(GetEndpointProvider());
		return Client.ToSharedRef();
	}

private:
	TSharedPtr<IConcertEndpointProvider> GetEndpointProvider()
	{
		if (EndpointProvider.IsValid())
		{
			return EndpointProvider;
		}
		EndpointProvider = IConcertTransportModule::Get().CreateEndpointProvider();
		return EndpointProvider;
	}

	/** Delegate Handle for the PreExit callback, needed to execute UObject related shutdowns */
	FDelegateHandle AppPreExitDelegateHandle;

	TSharedPtr<IConcertEndpointProvider> EndpointProvider;

	TSharedPtr<FConcertServer, ESPMode::ThreadSafe> Server;
	TSharedPtr<FConcertClient, ESPMode::ThreadSafe> Client;
};

IMPLEMENT_MODULE(FConcertModule, Concert);
