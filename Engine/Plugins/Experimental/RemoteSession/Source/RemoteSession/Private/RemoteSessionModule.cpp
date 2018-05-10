// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RemoteSession.h"
#include "Framework/Application/SlateApplication.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "RemoteSessionHost.h"
#include "RemoteSessionClient.h"
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EDITOR
	#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "FRemoteSessionModule"


class FRemoteSessionModule : public IRemoteSessionModule, public FTickableGameObject
{
protected:

	TSharedPtr<FRemoteSessionHost>		Host;
	TSharedPtr<FRemoteSessionClient>		Client;

	int32								DefaultPort;
	int32								Quality;
	int32								Framerate;

	bool bAutoHostWithPIE;
	FDelegateHandle PostPieDelegate;
	FDelegateHandle EndPieDelegate;

public:

	void SetAutoStartWithPIE(bool bEnable)
	{
		bAutoHostWithPIE = bEnable;
	}

	void StartupModule()
	{
		bool bAutoHostWithGame = false;
		DefaultPort = IRemoteSessionModule::kDefaultPort;
		Quality = 85;
		Framerate = 30;
		bAutoHostWithPIE = false;

		GConfig->GetBool(TEXT("RemoteSession"), TEXT("bAutoHostWithGame"), bAutoHostWithGame, GEngineIni);
		GConfig->GetBool(TEXT("RemoteSession"), TEXT("bAutoHostWithPIE"), bAutoHostWithPIE, GEngineIni);
		GConfig->GetInt(TEXT("RemoteSession"), TEXT("HostPort"), DefaultPort, GEngineIni);
		GConfig->GetInt(TEXT("RemoteSession"), TEXT("Quality"), Quality, GEngineIni);
		GConfig->GetInt(TEXT("RemoteSession"), TEXT("Framerate"), Framerate, GEngineIni);

		if (PLATFORM_DESKTOP)
		{
			if (GIsEditor)
			{
#if WITH_EDITOR
				PostPieDelegate = FEditorDelegates::PostPIEStarted.AddRaw(this, &FRemoteSessionModule::OnPIEStarted);
				EndPieDelegate = FEditorDelegates::EndPIE.AddRaw(this, &FRemoteSessionModule::OnPIEEnded);
#endif
			}
			else if (bAutoHostWithGame)
			{
				InitHost();
			}
		}
	}

	void ShutdownModule()
	{
		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
#if WITH_EDITOR
		if (PostPieDelegate.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPieDelegate);
		}

		if (EndPieDelegate.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(EndPieDelegate);
		}
#endif
	}

	virtual void InitClient(const TCHAR* RemoteAddress) override
	{
		if (Client.IsValid())
		{
			Client = nullptr;
		}

		Client = MakeShareable(new FRemoteSessionClient(RemoteAddress));

	}

	virtual bool IsClientConnected() const override
	{
		return Client.IsValid() && Client->IsConnected();
	}

	virtual void StopClient() override
	{
		Client = nullptr;
	}

	virtual TSharedPtr<IRemoteSessionRole> GetClient() const override
	{
		return Client;
	}

	virtual void InitHost(const int16 Port = 0) override
	{
#if !UE_BUILD_SHIPPING
		if (Host.IsValid())
		{
			Host = nullptr;
		}

		TSharedPtr<FRemoteSessionHost> NewHost = MakeShareable(new FRemoteSessionHost(Quality, Framerate));

		int16 SelectedPort = Port ? Port : (int16)DefaultPort;

		if (NewHost->StartListening(SelectedPort))
		{
			Host = NewHost;
			UE_LOG(LogRemoteSession, Log, TEXT("Started listening on port %d"), SelectedPort);
		}
		else
		{
			UE_LOG(LogRemoteSession, Error, TEXT("Failed to start host listening on port %d"), SelectedPort);
		}
#else
		UE_LOG(LogRemoteSession, Log, TEXT("RemoteSession is disabled. Shipping=1"));
#endif
	}

	virtual bool IsHostRunning() const override
	{
		return Host.IsValid();
	}

	virtual bool IsHostConnected() const override
	{
		return Host.IsValid() && Host->IsConnected();
	}

	virtual void StopHost() override
	{
		Host = nullptr;
	}

	virtual TSharedPtr<IRemoteSessionRole>	GetHost() const override
	{
		return Host;
	}

	void OnPIEStarted(bool bSimulating)
	{
		if (bAutoHostWithPIE)
		{
			InitHost();
		}
	}

	void OnPIEEnded(bool bSimulating)
	{
		// always stop, incase it was started via the console
		StopHost();
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRemoteSession, STATGROUP_Tickables);
	}
	
	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual void Tick(float DeltaTime) override
	{
		if (Client.IsValid())
		{
			Client->Tick(DeltaTime);
		}

		if (Host.IsValid())
		{
			Host->Tick(DeltaTime);
		}
	}	
};
	
IMPLEMENT_MODULE(FRemoteSessionModule, RemoteSession)

FAutoConsoleCommand GRemoteHostCommand(
	TEXT("remote.host"),
	TEXT("Starts a remote viewer host"),
	FConsoleCommandDelegate::CreateStatic(
		[]()
	{
		if (FRemoteSessionModule* Viewer = FModuleManager::LoadModulePtr<FRemoteSessionModule>("RemoteSession"))
		{
			Viewer->InitHost();
		}
	})
);

FAutoConsoleCommand GRemoteDisconnectCommand(
	TEXT("remote.disconnect"),
	TEXT("Disconnect remote viewer"),
	FConsoleCommandDelegate::CreateStatic(
		[]()
	{
		if (FRemoteSessionModule* Viewer = FModuleManager::LoadModulePtr<FRemoteSessionModule>("RemoteSession"))
		{
			Viewer->StopClient();
			Viewer->StopHost();
		}
	})
);

FAutoConsoleCommand GRemoteAutoPIECommand(
	TEXT("remote.autopie"),
	TEXT("enables remote with pie"),
	FConsoleCommandDelegate::CreateStatic(
		[]()
{
	if (FRemoteSessionModule* Viewer = FModuleManager::LoadModulePtr<FRemoteSessionModule>("RemoteSession"))
	{
		Viewer->SetAutoStartWithPIE(true);
	}
})
);

#undef LOCTEXT_NAMESPACE
