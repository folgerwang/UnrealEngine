// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClient.h"

#include "ConcertClientSession.h"
#include "ConcertLogger.h"
#include "ConcertLogGlobal.h"

#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/AsyncTaskNotification.h"

#define LOCTEXT_NAMESPACE "ConcertClient"

class FConcertAutoConnection
{
public:
	FConcertAutoConnection(FConcertClient* InClient, UConcertClientConfig* InSettings)
		: Client(InClient)
		, Settings(InSettings)
	{
		// Make sure discovery is enabled on the client
		Client->StartDiscovery();
		Client->OnSessionConnectionChanged().AddRaw(this, &FConcertAutoConnection::HandleConnectionChanged);
		Client->OnSessionStartup().AddRaw(this, &FConcertAutoConnection::HandleSessionStartup);
		AutoConnectionTick = FTicker::GetCoreTicker().AddTicker(TEXT("ConcertAutoConnect"), 1, [this](float) {
			Tick();
			return true;
		});

	}

	~FConcertAutoConnection()
	{
		Client->StopDiscovery();
		Client->OnSessionConnectionChanged().RemoveAll(this);
		Client->OnSessionStartup().RemoveAll(this);

		if (AutoConnectionTick.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(AutoConnectionTick);
			AutoConnectionTick.Reset();
		}
	}

private:
	void Tick()
	{
		// Already connected
		if (IsConnected())
		{
			// Once connected if we aren't in auto connection mode, shut ourselves down
			if (!Settings->bAutoConnect)
			{
				Client->AutoConnection.Reset();
			}
			return;
		}

		// Ongoing Connection request
		if (OngoingConnectionRequest.IsValid())
		{
			if (OngoingConnectionRequest.IsReady())
			{
				TSharedFuture<EConcertResponseCode> SessionCreated = OngoingConnectionRequest.Get();
				if (SessionCreated.IsReady())
				{
					const EConcertResponseCode RequestResponseCode = SessionCreated.Get();
					if (RequestResponseCode != EConcertResponseCode::Success)
					{
						// if the auto connect setting is off and the server refused our request, we stop trying to connect 
						if (!Settings->bAutoConnect && RequestResponseCode == EConcertResponseCode::Failed)
						{
							Client->AutoConnection.Reset();
							return;
						}

						// if unsuccessful, clear the ongoing request to retry
						OngoingConnectionRequest = TFuture<TSharedFuture<EConcertResponseCode>>();
					}
				}
			}
			return;
		}

		check(!IsConnecting());

		// Clear our current session before initiating a new connection request
		CurrentSession.Reset();

		// Create or/and Join Session 
		for (const FConcertServerInfo& ServerInfo : Client->GetKnownServers())
		{
			if (ServerInfo.ServerName == Settings->DefaultServerURL)
			{
				CreateOrJoinDefaultSession(ServerInfo);
				//We only want to connect to the first valid sever we found
				break;
			}
		}
	}

	bool IsConnected() const
	{
		return CurrentSession.IsValid() ? CurrentSession.Pin()->GetConnectionStatus() == EConcertConnectionStatus::Connected : false;
	}

	bool IsConnecting() const
	{
		return CurrentSession.IsValid() ? CurrentSession.Pin()->GetConnectionStatus() == EConcertConnectionStatus::Connecting : false;
	}

	void CreateOrJoinDefaultSession(const FConcertServerInfo& ServerInfo)
	{
		// Get the Server sessions list
		OngoingConnectionRequest = Client->GetServerSessions(ServerInfo.AdminEndpointId)
			.Next([LocalSettings = Settings](FConcertAdmin_GetSessionsResponse Response)
			{
				if (Response.ResponseCode == EConcertResponseCode::Success)
				{
					// Find our default session
					for (const FConcertSessionInfo& SessionInfo : Response.Sessions)
					{
						if (SessionInfo.SessionName == LocalSettings->DefaultSessionName)
						{
							return TPair<bool, bool>(true, true); // request successful, session found
							break;
						}
					}
					return TPair<bool, bool>(true, false); // request successful, session not found
				}
				return TPair<bool, bool>(false, false); // request failed, session not found
			})
			.Next([LocalClient = Client, LocalSettings = Settings, ServerEndpoint = ServerInfo.AdminEndpointId](TPair<bool, bool> RequestSessionPair)
			{
				// Request was successful 
				if (RequestSessionPair.Key)
				{
					// we found the session, just join
					if (RequestSessionPair.Value)
					{
						return LocalClient->InternalJoinSession(ServerEndpoint, LocalSettings->DefaultSessionName).Share();
					}
					// no session found, create it
					else
					{
						FConcertCreateSessionArgs CreateSessionArgs;
						CreateSessionArgs.SessionName = LocalSettings->DefaultSessionName;
						CreateSessionArgs.SessionToRestore = LocalSettings->DefaultSessionToRestore;
						CreateSessionArgs.SaveSessionAs = LocalSettings->DefaultSaveSessionAs;
						return LocalClient->InternalCreateSession(ServerEndpoint, CreateSessionArgs).Share();
					}
				}
				// Resolve now
				TPromise<EConcertResponseCode> ResponsePromise;
				//The server can't refuse a get sessions request so the only option is a time out
				EConcertResponseCode ResponseCode;
				ResponseCode = EConcertResponseCode::TimedOut;
				ResponsePromise.SetValue(ResponseCode);
				return ResponsePromise.GetFuture().Share();
			});
	}

	void HandleConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus ConnectionStatus)
	{
		// Once we get connected or disconnected, clear our ongoing request if we have one, if it comes from our current session
		if (CurrentSession.IsValid() 
			&& CurrentSession.Pin().Get() == &InSession
			&& (ConnectionStatus == EConcertConnectionStatus::Connected || ConnectionStatus == EConcertConnectionStatus::Disconnected))
		{
			OngoingConnectionRequest = TFuture<TSharedFuture<EConcertResponseCode>>();
		}
	}

	void HandleSessionStartup(TSharedRef<IConcertClientSession> InSession)
	{
		CurrentSession = InSession;
	}

	TFuture<TSharedFuture<EConcertResponseCode>> OngoingConnectionRequest;
	FDelegateHandle AutoConnectionTick;
	FConcertClient* Client;
	TWeakPtr<IConcertClientSession> CurrentSession;
	UConcertClientConfig* Settings;
};

class FConcertPendingConnection : public TSharedFromThis<FConcertPendingConnection>
{
public:
	struct FConfig
	{
		FText PendingTitleText;
		FText SuccessTitleText;
		FText FailureTitleText;
		bool bIsAutoConnection = false;
	};

	FConcertPendingConnection(FConcertClient* InClient, const FConfig& InConfig)
		: Client(InClient)
		, Config(InConfig)
	{
	}

	~FConcertPendingConnection()
	{
		if (ConnectionTick.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(ConnectionTick);
		}
		
		// Abort any remaining work
		if (ConnectionTasks.Num() > 0)
		{
			ConnectionTasks[0]->Abort();

			// If the task immediately aborted then use its error message (if available), otherwise use a generic one
			FText AbortedErrorMessage = ConnectionTasks[0]->GetStatus() == EConcertResponseCode::Pending ? FText() : ConnectionTasks[0]->GetError();
			if (AbortedErrorMessage.IsEmpty())
			{
				AbortedErrorMessage = LOCTEXT("ConnectionAborted", "Connection Process Aborted.");
			}

			Notification->SetKeepOpenOnFailure(false); // Don't keep the notification open if aborted
			SetResult(EConcertResponseCode::Failed, AbortedErrorMessage);
		}
	}

	/** Execute this connection request */
	TFuture<EConcertResponseCode> Execute(TArray<TUniquePtr<IConcertClientConnectionTask>>&& InConnectionTasks)
	{
		checkf(ConnectionTasks.Num() == 0, TEXT("Execute has already been called!"));
		ConnectionTasks = MoveTemp(InConnectionTasks);
		checkf(ConnectionTasks.Num() != 0, TEXT("Execute was not given any tasks!"));

		// Set-up the task notification
		FAsyncTaskNotificationConfig NotificationConfig;
		NotificationConfig.bCanCancel.Bind(this, &FConcertPendingConnection::CanCancel);
		NotificationConfig.bKeepOpenOnFailure = !Config.bIsAutoConnection;
		NotificationConfig.TitleText = Config.PendingTitleText;
		NotificationConfig.ProgressText = ConnectionTasks[0]->GetDescription();
		NotificationConfig.LogCategory = &LogConcert;
		Notification = MakeUnique<FAsyncTaskNotification>(NotificationConfig);

		ConnectionTasks[0]->Execute();

		ConnectionTick = FTicker::GetCoreTicker().AddTicker(TEXT("ConcertPendingConnection"), 0.1f, [this](float) {
			Tick();
			return true;
		});

		return ConnectionResult.GetFuture();
	}

private:
	bool CanCancel() const
	{
		return ConnectionTasks.Num() > 0 && ConnectionTasks[0]->CanCancel();
	}

	void Tick()
	{
		// We should only Tick while we have tasks to process
		check(ConnectionTasks.Num() > 0);

		const bool bShouldCancel = Notification->ShouldCancel();
		if (bShouldCancel)
		{
			// Don't keep the notification open if cancelled
			Notification->SetKeepOpenOnFailure(false);
		}

		// Update the current task
		switch (ConnectionTasks[0]->GetStatus())
		{
			// Pending state - update the task
		case EConcertResponseCode::Pending:
			ConnectionTasks[0]->Tick(bShouldCancel);
			return;

			// Success state - move on to the next task
		case EConcertResponseCode::Success:
			ConnectionTasks.RemoveAt(0, 1, /*bAllowShrinking*/false);
			if (ConnectionTasks.Num() > 0)
			{
				Notification->SetProgressText(ConnectionTasks[0]->GetDescription());
				ConnectionTasks[0]->Execute();
			}
			else
			{
				// Processed everything without error
				SetResultAndDelete(EConcertResponseCode::Success); // do not use 'this' after this call!
			}
			return;

			// Error state - fail the connection
		default:
			SetResultAndDelete(ConnectionTasks[0]->GetStatus(), ConnectionTasks[0]->GetError()); // do not use 'this' after this call!
			return;
		}
	}

	/** Set the result */
	void SetResult(const EConcertResponseCode InResult, const FText InFailureReason = FText())
	{
		if (InResult == EConcertResponseCode::Success)
		{
			Notification->SetComplete(Config.SuccessTitleText, FText(), true);
		}
		else
		{
			Notification->SetComplete(Config.FailureTitleText, InFailureReason, false);
		}
		ConnectionTasks.Reset();
		ConnectionResult.SetValue(InResult);
	}

	/** Set the result and delete ourself - 'this' will be garbage after calling this function! */
	void SetResultAndDelete(const EConcertResponseCode InResult, const FText InFailureReason = FText())
	{
		// Set the result and delete ourself
		SetResult(InResult, InFailureReason);
		check(Client->PendingConnection.Get() == this);
		Client->PendingConnection.Reset();
	}

	FConcertClient* Client;
	FConfig Config;
	FDelegateHandle ConnectionTick;
	TPromise<EConcertResponseCode> ConnectionResult;
	TUniquePtr<FAsyncTaskNotification> Notification;
	TArray<TUniquePtr<IConcertClientConnectionTask>> ConnectionTasks;
};

template <typename RequestType>
class TConcertClientConnectionRequestTask : public IConcertClientConnectionTask
{
public:
	TConcertClientConnectionRequestTask(FConcertClient* InClient, RequestType&& InRequest, const FGuid& InServerAdminEndpointId)
		: Client(InClient)
		, Request(MoveTemp(InRequest))
		, ServerAdminEndpointId(InServerAdminEndpointId)
	{
	}

	virtual void Abort() override
	{
		Result.Reset();
	}

	virtual void Tick(const bool bShouldCancel) override
	{
	}

	virtual bool CanCancel() const override
	{
		return false;
	}

	virtual EConcertResponseCode GetStatus() const override
	{
		if (Result.IsValid())
		{
			return Result.IsReady() ? Result.Get() : EConcertResponseCode::Pending;
		}
		return EConcertResponseCode::Failed;
	}

	virtual FText GetError() const override
	{
		return Result.IsValid() ? ErrorText : LOCTEXT("RemoteConnectionAttemptAborted", "Remote Connection Attempt Aborted.");
	}

	virtual FText GetDescription() const override
	{
		return LOCTEXT("AttemptingRemoteConnection", "Attempting Remote Connection...");
	}

protected:
	FConcertClient* Client;
	RequestType Request;
	FGuid ServerAdminEndpointId;
	TFuture<EConcertResponseCode> Result;
	FText ErrorText;
};

class FConcertClientJoinSessionTask : public TConcertClientConnectionRequestTask<FConcertAdmin_FindSessionRequest>
{
public:
	FConcertClientJoinSessionTask(FConcertClient* InClient, FConcertAdmin_FindSessionRequest&& InRequest, const FGuid& InServerAdminEndpointId)
		: TConcertClientConnectionRequestTask(InClient, MoveTemp(InRequest), InServerAdminEndpointId)
	{
	}

	virtual void Execute() override
	{
		Result = Client->ClientAdminEndpoint->SendRequest<FConcertAdmin_FindSessionRequest, FConcertAdmin_SessionInfoResponse>(Request, ServerAdminEndpointId)
			.Next([this](const FConcertAdmin_SessionInfoResponse& SessionInfoResponse)
			{
				if (SessionInfoResponse.ResponseCode == EConcertResponseCode::Success)
				{
					Client->CreateClientSession(SessionInfoResponse.SessionInfo);
				}
				else
				{
					ErrorText = SessionInfoResponse.Reason;
				}
				return SessionInfoResponse.ResponseCode;
			});
	}
};

class FConcertClientCreateSessionTask : public TConcertClientConnectionRequestTask<FConcertAdmin_CreateSessionRequest>
{
public:
	FConcertClientCreateSessionTask(FConcertClient* InClient, FConcertAdmin_CreateSessionRequest&& InRequest, const FGuid& InServerAdminEndpointId)
		: TConcertClientConnectionRequestTask(InClient, MoveTemp(InRequest), InServerAdminEndpointId)
	{
	}

	virtual void Execute() override
	{
		Result = Client->ClientAdminEndpoint->SendRequest<FConcertAdmin_CreateSessionRequest, FConcertAdmin_SessionInfoResponse>(Request, ServerAdminEndpointId)
			.Next([this](const FConcertAdmin_SessionInfoResponse& SessionInfoResponse)
			{
				if (SessionInfoResponse.ResponseCode == EConcertResponseCode::Success)
				{
					Client->CreateClientSession(SessionInfoResponse.SessionInfo);
				}
				else
				{
					ErrorText = SessionInfoResponse.Reason;
				}
				return SessionInfoResponse.ResponseCode;
			});
	}
};

FConcertClient::FConcertClient()
	: DiscoveryCount(0)
	, bClientSessionPendingDestroy(false)
{
}

FConcertClient::~FConcertClient()
{
	// if the ClientAdminEndpoint is valid, Shutdown wasn't called
	check(!ClientAdminEndpoint.IsValid());
}

void FConcertClient::SetEndpointProvider(const TSharedPtr<IConcertEndpointProvider>& Provider)
{
	EndpointProvider = Provider;
}

void FConcertClient::Configure(const UConcertClientConfig* InSettings)
{
	ClientInfo.Initialize();
	check(InSettings != nullptr);
	Settings = TStrongObjectPtr<UConcertClientConfig>(const_cast<UConcertClientConfig*>(InSettings));
	// Set the display name from the settings or default to username (i.e. app session owner)
	ClientInfo.DisplayName = Settings->ClientSettings.DisplayName.IsEmpty() ? ClientInfo.UserName : Settings->ClientSettings.DisplayName;
	ClientInfo.AvatarColor = Settings->ClientSettings.AvatarColor;
	ClientInfo.DesktopAvatarActorClass = Settings->ClientSettings.DesktopAvatarActorClass.ToString();
	ClientInfo.VRAvatarActorClass = Settings->ClientSettings.VRAvatarActorClass.ToString();
}

bool FConcertClient::IsConfigured() const
{
	// if the instance id hasn't been set yet, then Configure wasn't called.
	return ClientInfo.InstanceInfo.InstanceId.IsValid();
}

const FConcertClientInfo& FConcertClient::GetClientInfo() const
{
	return ClientInfo;
}

bool FConcertClient::IsStarted() const
{
	return ClientAdminEndpoint.IsValid();
}

void FConcertClient::Startup()
{
	check(IsConfigured());
	if (!ClientAdminEndpoint.IsValid() && EndpointProvider.IsValid())
	{
		// Create the client administration endpoint
		ClientAdminEndpoint = EndpointProvider->CreateLocalEndpoint(TEXT("Admin"), Settings->EndpointSettings, &FConcertLogger::CreateLogger);
	}

	FCoreDelegates::OnEndFrame.AddRaw(this, &FConcertClient::OnEndFrame);
}

void FConcertClient::Shutdown()
{
	FCoreDelegates::OnEndFrame.RemoveAll(this);

	// Remove Auto Connection routine, if any
	AutoConnection.Reset();

	while (IsDiscoveryEnabled())
	{
		StopDiscovery();
	}
	ClientAdminEndpoint.Reset();
	KnownServers.Empty();

	if (ClientSession.IsValid())
	{
		ClientSession->Disconnect();
		OnSessionShutdownDelegate.Broadcast(ClientSession.ToSharedRef());
		ClientSession->Shutdown();
		ClientSession.Reset();
	}
}

bool FConcertClient::IsDiscoveryEnabled() const
{
	return DiscoveryCount > 0;
}

void FConcertClient::StartDiscovery()
{
	++DiscoveryCount;
	if (ClientAdminEndpoint.IsValid() && !DiscoveryTick.IsValid())
	{
		ClientAdminEndpoint->RegisterEventHandler<FConcertAdmin_ServerDiscoveredEvent>(this, &FConcertClient::HandleServerDiscoveryEvent);

		DiscoveryTick = FTicker::GetCoreTicker().AddTicker(TEXT("Discovery"), 1, [this](float DeltaSeconds) {
			const FDateTime UtcNow = FDateTime::UtcNow();
			SendDiscoverServersEvent();
			TimeoutDiscovery(UtcNow);
			return true;
		});
	}
}

void FConcertClient::StopDiscovery()
{
	check(IsDiscoveryEnabled());
	--DiscoveryCount;
	if (DiscoveryCount > 0)
	{
		return;
	}

	if (ClientAdminEndpoint.IsValid())
	{
		ClientAdminEndpoint->UnregisterEventHandler<FConcertAdmin_ServerDiscoveredEvent>();
	}
	if (DiscoveryTick.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(DiscoveryTick);
		DiscoveryTick.Reset();
	}
}

void FConcertClient::DefaultConnect()
{
	check(IsStarted());
	if (AutoConnection.IsValid())
	{
		return;
	}

	AutoConnection = MakeUnique<FConcertAutoConnection>(this, Settings.Get());
}

void FConcertClient::ResetAutoConnect()
{
	AutoConnection.Reset();
}

bool FConcertClient::HasAutoConnection() const
{
	return AutoConnection.IsValid();
}

TArray<FConcertServerInfo> FConcertClient::GetKnownServers() const
{
	TArray<FConcertServerInfo> ServerArray;
	ServerArray.Empty(KnownServers.Num());
	for (const auto& Server : KnownServers)
	{
		ServerArray.Emplace(Server.Value.ServerInfo);
	}
	return ServerArray;
}

FSimpleMulticastDelegate& FConcertClient::OnKnownServersUpdated()
{
	return ServersUpdatedDelegate;
}

FOnConcertClientSessionStartupOrShutdown& FConcertClient::OnSessionStartup()
{
	return OnSessionStartupDelegate;
}

FOnConcertClientSessionStartupOrShutdown& FConcertClient::OnSessionShutdown()
{
	return OnSessionShutdownDelegate;
}

FOnConcertClientSessionGetPreConnectionTasks& FConcertClient::OnGetPreConnectionTasks()
{
	return OnGetPreConnectionTasksDelegate;
}

FOnConcertClientSessionConnectionChanged& FConcertClient::OnSessionConnectionChanged()
{
	return OnSessionConnectionChangedDelegate;
}

EConcertConnectionStatus FConcertClient::GetSessionConnectionStatus() const
{
	return ClientSession.IsValid() ? ClientSession->GetConnectionStatus() : EConcertConnectionStatus::Disconnected;
}

TFuture<EConcertResponseCode> FConcertClient::CreateSession(const FGuid& ServerAdminEndpointId, const FConcertCreateSessionArgs& CreateSessionArgs)
{
	// We don't want the client to get automatically reconnected to it's default session if something wrong happens
	AutoConnection.Reset();
	return InternalCreateSession(ServerAdminEndpointId, CreateSessionArgs);
}

TFuture<EConcertResponseCode> FConcertClient::JoinSession(const FGuid& ServerAdminEndpointId, const FString& SessionName)
{
	// We don't want the client to get automatically reconnected to it's default session if something wrong happens
	AutoConnection.Reset();
	return InternalJoinSession(ServerAdminEndpointId, SessionName);
}

TFuture<EConcertResponseCode> FConcertClient::DeleteSession(const FGuid & ServerAdminEndpointId, const FString & SessionName)
{
	FConcertAdmin_DeleteSessionRequest DeleteSessionRequest;
	DeleteSessionRequest.SessionName = SessionName;

	// Fill the information for the client identification
	DeleteSessionRequest.UserName = ClientInfo.UserName;
	DeleteSessionRequest.DeviceName = ClientInfo.DeviceName;

	FAsyncTaskNotificationConfig NotificationConfig;
	NotificationConfig.bKeepOpenOnFailure = true;
	NotificationConfig.TitleText = FText::Format(LOCTEXT("DeletingSessionFmt", "Deleting Session '{0}'..."), FText::FromString(DeleteSessionRequest.SessionName));
	NotificationConfig.LogCategory = &LogConcert;

	FAsyncTaskNotification Notification(NotificationConfig);

	return ClientAdminEndpoint->SendRequest<FConcertAdmin_DeleteSessionRequest, FConcertResponseData>(DeleteSessionRequest, ServerAdminEndpointId)
		.Next([this, DeleteSessionRequest, Notification = MoveTemp(Notification)](const FConcertResponseData& RequestResponse) mutable
		{
			if (RequestResponse.ResponseCode == EConcertResponseCode::Success)
			{
				Notification.SetComplete(FText::Format(LOCTEXT("DeletedSessionFmt", "Deleted Session '{0}'"), FText::FromString(DeleteSessionRequest.SessionName)), FText(), true);
			}
			else
			{
				Notification.SetComplete(FText::Format(LOCTEXT("FailedToDeleteSessionFmt", "Failed to Delete Session '{0}'"), FText::FromString(DeleteSessionRequest.SessionName)), RequestResponse.Reason, false);
			}
			return RequestResponse.ResponseCode;
		});
}

void FConcertClient::DisconnectSession()
{
	// We don't want the client to get automatically reconnected to it's default session
	AutoConnection.Reset();
	InternalDisconnectSession();
}

void FConcertClient::ResumeSession()
{
	if (ClientSession.IsValid())
	{
		ClientSession->Resume();
	}
}

void FConcertClient::SuspendSession()
{
	if (ClientSession.IsValid())
	{
		ClientSession->Suspend();
	}
}

bool FConcertClient::IsSessionSuspended() const
{
	return ClientSession.IsValid() && ClientSession->IsSuspended();
}

bool FConcertClient::IsOwnerOf(const FConcertSessionInfo& InSessionInfo) const
{
	return ClientInfo.UserName == InSessionInfo.OwnerUserName && ClientInfo.DeviceName == InSessionInfo.OwnerDeviceName;
}

TSharedPtr<IConcertClientSession> FConcertClient::GetCurrentSession() const
{
	return ClientSession;
}

TFuture<FConcertAdmin_GetSessionsResponse> FConcertClient::GetServerSessions(const FGuid& ServerAdminEndpointId) const
{
	FConcertAdmin_GetSessionsRequest GetSessionsRequest = FConcertAdmin_GetSessionsRequest();
	return ClientAdminEndpoint->SendRequest<FConcertAdmin_GetSessionsRequest, FConcertAdmin_GetSessionsResponse>(GetSessionsRequest, ServerAdminEndpointId)
		.Next([this, GetSessionsRequest](const FConcertAdmin_GetSessionsResponse& GetSessionsRequestResponse)
		{
			return GetSessionsRequestResponse;
		});
}

TFuture<FConcertAdmin_GetSessionClientsResponse> FConcertClient::GetSessionClients(const FGuid& ServerAdminEndpointId, const FString& SessionName) const
{
	FConcertAdmin_GetSessionClientsRequest GetSessionClientsRequest;
	GetSessionClientsRequest.SessionName = SessionName;
	return ClientAdminEndpoint->SendRequest<FConcertAdmin_GetSessionClientsRequest, FConcertAdmin_GetSessionClientsResponse>(GetSessionClientsRequest, ServerAdminEndpointId)
		.Next([this, GetSessionClientsRequest](const FConcertAdmin_GetSessionClientsResponse& GetSessionClientsResponse)
		{
			return GetSessionClientsResponse;
		});
}

TFuture<FConcertAdmin_GetSavedSessionNamesResponse> FConcertClient::GetSavedSessionNames(const FGuid& ServerAdminEndpointId) const
{
	FConcertAdmin_GetSavedSessionNamesRequest GetSavedSessionNamesRequest;
	return ClientAdminEndpoint->SendRequest<FConcertAdmin_GetSavedSessionNamesRequest, FConcertAdmin_GetSavedSessionNamesResponse>(GetSavedSessionNamesRequest, ServerAdminEndpointId);
}

TFuture<EConcertResponseCode> FConcertClient::InternalCreateSession(const FGuid& ServerAdminEndpointId, const FConcertCreateSessionArgs& CreateSessionArgs)
{
	// Cancel any pending connection (will be aborted)
	PendingConnection.Reset();

	// Build the tasks to execute
	TArray<TUniquePtr<IConcertClientConnectionTask>> ConnectionTasks;

	// Collect pre-connection tasks
	OnGetPreConnectionTasksDelegate.Broadcast(*this, ConnectionTasks);

	// Create session task
	{
		// Fill create session request;
		FConcertAdmin_CreateSessionRequest CreateSessionRequest;
		CreateSessionRequest.SessionName = CreateSessionArgs.SessionName;
		CreateSessionRequest.OwnerClientInfo = ClientInfo;
	
		// Session settings
		CreateSessionRequest.SessionSettings.Initialize();
		CreateSessionRequest.SessionSettings.SessionToRestore = CreateSessionArgs.SessionToRestore;
		CreateSessionRequest.SessionSettings.SaveSessionAs = CreateSessionArgs.SaveSessionAs;
;

		ConnectionTasks.Emplace(MakeUnique<FConcertClientCreateSessionTask>(this, MoveTemp(CreateSessionRequest), ServerAdminEndpointId));
	}

	// Pending connection config
	const FText SessionNameText = FText::FromString(CreateSessionArgs.SessionName);
	FConcertPendingConnection::FConfig PendingConnectionConfig;
	PendingConnectionConfig.PendingTitleText = FText::Format(LOCTEXT("CreatingSessionFmt", "Creating Session '{0}'..."), SessionNameText);
	PendingConnectionConfig.SuccessTitleText = FText::Format(LOCTEXT("CreatedSessionFmt", "Created Session '{0}'"), SessionNameText);
	PendingConnectionConfig.FailureTitleText = FText::Format(LOCTEXT("FailedToCreateSessionFmt", "Failed to Create Session '{0}'"), SessionNameText);
	PendingConnectionConfig.bIsAutoConnection = AutoConnection.IsValid();

	// Kick off a pending connection to execute the tasks
	PendingConnection = MakeShared<FConcertPendingConnection>(this, PendingConnectionConfig);
	return PendingConnection->Execute(MoveTemp(ConnectionTasks));
}

TFuture<EConcertResponseCode> FConcertClient::InternalJoinSession(const FGuid& ServerAdminEndpointId, const FString& SessionName)
{
	// Cancel any pending connection (will be aborted)
	PendingConnection.Reset();

	// Build the tasks to execute
	TArray<TUniquePtr<IConcertClientConnectionTask>> ConnectionTasks;

	// Collect pre-connection tasks
	OnGetPreConnectionTasksDelegate.Broadcast(*this, ConnectionTasks);

	// Find session task
	{
		// Fill find session request
		FConcertAdmin_FindSessionRequest FindSessionRequest;
		FindSessionRequest.SessionName = SessionName;
		FindSessionRequest.OwnerClientInfo = ClientInfo;

		// Session settings
		FindSessionRequest.SessionSettings.Initialize();

		ConnectionTasks.Emplace(MakeUnique<FConcertClientJoinSessionTask>(this, MoveTemp(FindSessionRequest), ServerAdminEndpointId));
	}

	// Pending connection config
	const FText SessionNameText = FText::FromString(SessionName);
	FConcertPendingConnection::FConfig PendingConnectionConfig;
	PendingConnectionConfig.PendingTitleText = FText::Format(LOCTEXT("ConnectingToSessionFmt", "Connecting to Session '{0}'..."), SessionNameText);
	PendingConnectionConfig.SuccessTitleText = FText::Format(LOCTEXT("ConnectedToSessionFmt", "Connected to Session '{0}'"), SessionNameText);
	PendingConnectionConfig.FailureTitleText = FText::Format(LOCTEXT("FailedToConnectToSessionFmt", "Failed to Connect to Session '{0}'"), SessionNameText);
	PendingConnectionConfig.bIsAutoConnection = AutoConnection.IsValid();

	// Kick off a pending connection to execute the tasks
	PendingConnection = MakeShared<FConcertPendingConnection>(this, PendingConnectionConfig);
	return PendingConnection->Execute(MoveTemp(ConnectionTasks));
}

void FConcertClient::InternalDisconnectSession()
{
	if (ClientSession.IsValid())
	{
		ClientSession->Disconnect();
		OnSessionShutdownDelegate.Broadcast(ClientSession.ToSharedRef());
		ClientSession->Shutdown();
		ClientSession.Reset();
	}

	bClientSessionPendingDestroy = false;
}

void FConcertClient::OnEndFrame()
{
	if (bClientSessionPendingDestroy)
	{
		InternalDisconnectSession();
		bClientSessionPendingDestroy = false;
	}
}

void FConcertClient::TimeoutDiscovery(const FDateTime& UtcNow)
{
	const FTimespan DiscoveryTimeoutSpan = FTimespan(0, 0, Settings->ClientSettings.DiscoveryTimeoutSeconds);

	bool TimeoutOccured = false;
	for (auto It = KnownServers.CreateIterator(); It; ++It)
	{
		if (It->Value.LastDiscoveryTime + DiscoveryTimeoutSpan <= UtcNow)
		{
			TimeoutOccured = true;
			UE_LOG(LogConcert, Display, TEXT("Server %s lost."), *It->Value.ServerInfo.ServerName);
			It.RemoveCurrent();
			continue;
		}
	}

	if (TimeoutOccured)
	{
		ServersUpdatedDelegate.Broadcast();
	}
}

void FConcertClient::SendDiscoverServersEvent()
{
	ClientAdminEndpoint->PublishEvent(FConcertAdmin_DiscoverServersEvent());
}

void FConcertClient::HandleServerDiscoveryEvent(const FConcertMessageContext& Context)
{
	const FConcertAdmin_ServerDiscoveredEvent* Message = Context.GetMessage<FConcertAdmin_ServerDiscoveredEvent>();

	FKnownServer* Info = KnownServers.Find(Message->ConcertEndpointId);
	if (Info == nullptr)
	{
		UE_LOG(LogConcert, Display, TEXT("Server %s discovered."), *Message->ServerName);
		KnownServers.Emplace(Message->ConcertEndpointId, FKnownServer{ Context.UtcNow, FConcertServerInfo{ Message->ConcertEndpointId, Message->ServerName, Message->InstanceInfo, Message->ServerFlags } });
		ServersUpdatedDelegate.Broadcast();
	}
	else
	{
		Info->LastDiscoveryTime = Context.UtcNow;
	}
}

void FConcertClient::CreateClientSession(const FConcertSessionInfo& SessionInfo)
{
	InternalDisconnectSession();
	ClientSession = MakeShared<FConcertClientSession>(SessionInfo, ClientInfo, Settings->ClientSettings, EndpointProvider->CreateLocalEndpoint(SessionInfo.SessionName, Settings->EndpointSettings, &FConcertLogger::CreateLogger));
	OnSessionStartupDelegate.Broadcast(ClientSession.ToSharedRef());
	ClientSession->OnConnectionChanged().AddRaw(this, &FConcertClient::HandleSessionConnectionChanged);
	ClientSession->Startup();
	ClientSession->Connect();
}

void FConcertClient::HandleSessionConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus Status)
{
	// If this session disconnected, make sure we fully destroy it at the end of the frame
	if (Status == EConcertConnectionStatus::Disconnected)
	{
		bClientSessionPendingDestroy = true;
	}

	OnSessionConnectionChangedDelegate.Broadcast(InSession, Status);
}

#undef LOCTEXT_NAMESPACE
