// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertClient.h"
#include "IConcertTransportModule.h"
#include "ConcertSettings.h"

#include "UObject/StrongObjectPtr.h"

class FConcertClientSession;
class FConcertAutoConnection;
class FConcertPendingConnection;
class FConcertClientJoinSessionTask;
class FConcertClientCreateSessionTask;

/** Implements the Concert client */
class FConcertClient : public IConcertClient
{
public:
	FConcertClient();
	virtual ~FConcertClient();

	virtual void Configure(const UConcertClientConfig* InSettings) override;
	virtual bool IsConfigured() const override;
	virtual const FConcertClientInfo& GetClientInfo() const override;

	virtual bool IsStarted() const override;
	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual bool IsDiscoveryEnabled() const override;
	virtual void StartDiscovery() override;
	virtual void StopDiscovery() override;

	virtual void DefaultConnect() override;
	virtual void ResetAutoConnect() override;
	virtual bool HasAutoConnection() const override;

	virtual TArray<FConcertServerInfo> GetKnownServers() const override;
	virtual FSimpleMulticastDelegate& OnKnownServersUpdated() override;

	virtual FOnConcertClientSessionStartupOrShutdown& OnSessionStartup() override;
	virtual FOnConcertClientSessionStartupOrShutdown& OnSessionShutdown() override;

	virtual FOnConcertClientSessionGetPreConnectionTasks& OnGetPreConnectionTasks() override;
	virtual FOnConcertClientSessionConnectionChanged& OnSessionConnectionChanged() override;

	virtual EConcertConnectionStatus GetSessionConnectionStatus() const override;
	virtual TFuture<EConcertResponseCode> CreateSession(const FGuid& ServerAdminEndpointId, const FConcertCreateSessionArgs& CreateSessionArgs) override;
	virtual TFuture<EConcertResponseCode> JoinSession(const FGuid& ServerAdminEndpointId, const FString& SessionName) override;
	virtual TFuture<EConcertResponseCode> DeleteSession(const FGuid& ServerAdminEndpointId, const FString& SessionName) override;
	virtual void DisconnectSession() override;
	virtual void ResumeSession() override;
	virtual void SuspendSession() override;
	virtual bool IsSessionSuspended() const override;
	virtual bool IsOwnerOf(const FConcertSessionInfo& InSessionInfo) const override;
	virtual TSharedPtr<IConcertClientSession> GetCurrentSession() const override;

	virtual TFuture<FConcertAdmin_GetSessionsResponse> GetServerSessions(const FGuid& ServerAdminEndpointId) const override;
	virtual TFuture<FConcertAdmin_GetSessionClientsResponse> GetSessionClients(const FGuid& ServerAdminEndpointId, const FString& SessionName) const override;
	virtual TFuture<FConcertAdmin_GetSavedSessionNamesResponse> GetSavedSessionNames(const FGuid& ServerAdminEndpointId) const override;

	/** Set the endpoint provider for the client */
	void SetEndpointProvider(const TSharedPtr<IConcertEndpointProvider>& Provider);

private:
	/** internal friend class for auto connection. */
	friend class FConcertAutoConnection;

	/** internal friend classes for pending connections. */
	friend class FConcertPendingConnection;
	friend class FConcertClientJoinSessionTask;
	friend class FConcertClientCreateSessionTask;

	TFuture<EConcertResponseCode> InternalCreateSession(const FGuid& ServerAdminEndpointId, const FConcertCreateSessionArgs& CreateSessionArgs);
	TFuture<EConcertResponseCode> InternalJoinSession(const FGuid& ServerAdminEndpointId, const FString& SessionName);
	void InternalDisconnectSession();

	/** */
	void OnEndFrame();

	/** Remove server from the known server list when they haven't discovered them for a while */
	void TimeoutDiscovery(const FDateTime& UtcNow);

	/** Broadcast a message to discover Concert servers */
	void SendDiscoverServersEvent();
	
	/** Handle any answers from Concert server to our search queries */
	void HandleServerDiscoveryEvent(const FConcertMessageContext& Context);

	/** Create a Concert client session based on the session information provided */
	void CreateClientSession(const FConcertSessionInfo& SessionInfo);

	/** Internal handler bound to the current session (if any) to propagate via our own OnSessionConnectionChanged delegate */
	void HandleSessionConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus Status);

	/** Endpoint provider */
	TSharedPtr<IConcertEndpointProvider> EndpointProvider;
	
	/** Administration endpoint for the client (i.e. creating, joining sessions) */
	TSharedPtr<IConcertLocalEndpoint> ClientAdminEndpoint;

	/** Count of the number of times the discovery has been enabled */
	uint32 DiscoveryCount;

	/** Ticker for Discovering Concert Servers */
	FDelegateHandle DiscoveryTick;

	struct FKnownServer
	{
		FDateTime LastDiscoveryTime;
		FConcertServerInfo ServerInfo;
	};
	/** Map of discovered Concert Servers */
	TMap<FGuid, FKnownServer> KnownServers;

	/** Holds a delegate to be invoked when the server list was updated. */
	FSimpleMulticastDelegate ServersUpdatedDelegate;

	/** Information about this Client */
	FConcertClientInfo ClientInfo;

	/** Delegate for client session startup */
	FOnConcertClientSessionStartupOrShutdown OnSessionStartupDelegate;

	/** Delegate for client session shutdown */
	FOnConcertClientSessionStartupOrShutdown OnSessionShutdownDelegate;

	/** Delegate that is called to get the pre-connection tasks for a client session */
	FOnConcertClientSessionGetPreConnectionTasks OnGetPreConnectionTasksDelegate;

	/** Delegate for when the session connection state changes */
	FOnConcertClientSessionConnectionChanged OnSessionConnectionChangedDelegate;

	/** Pointer to the Concert Session the client is connected to */
	TSharedPtr<FConcertClientSession> ClientSession;

	/** True if the client session disconnected this frame and should be fully destroyed at the end of the frame (this is mainly to handle timeouts) */
	bool bClientSessionPendingDestroy;

	/** Client settings object we were configured with */
	TStrongObjectPtr<UConcertClientConfig> Settings;

	/** Holds the auto connection routine, if any. */
	TUniquePtr<FConcertAutoConnection> AutoConnection;

	/** Holds the pending connection routine, if any (shared as it is used as a weak pointer with UI). */
	TSharedPtr<FConcertPendingConnection> PendingConnection;
};