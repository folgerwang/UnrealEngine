// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "IConcertSession.h"
#include "ConcertMessages.h"
#include "ConcertTransportMessages.h"

class UConcertClientConfig;

class IConcertClient;
class IConcertClientConnectionTask;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcertClientSessionStartupOrShutdown, TSharedRef<IConcertClientSession>);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertClientSessionGetPreConnectionTasks, const IConcertClient&, TArray<TUniquePtr<IConcertClientConnectionTask>>&);

/** Interface for tasks executed during the Concert client connection flow (eg, validation, creation, connection) */
class IConcertClientConnectionTask
{
public:
	virtual ~IConcertClientConnectionTask() = default;

	/**
	 * Execute this task.
	 * Typically this puts the task into a pending state, however it is possible for the task to immediately complete once executed. Ideally this should not block for a long time!
	 */
	virtual void Execute() = 0;

	/**
	 * Abort this task immediately, and discard any pending work.
	 * @note It is expected that GetStatus and GetError will return some kind of error state after this has been called.
	 */
	virtual void Abort() = 0;

	/**
	 * Tick this task, optionally requesting that it should gracefully cancel.
	 */
	virtual void Tick(const bool bShouldCancel) = 0;

	/**
	 * Get whether this task can be gracefully cancelled.
	 */
	virtual bool CanCancel() const = 0;

	/**
	 * Get the current status of this task.
	 * @note It is required that the task return Pending while it is in-progress, and Success when it has finished successfully. Any other status is treated as an error state, and GetError will be called.
	 */
	virtual EConcertResponseCode GetStatus() const = 0;

	/**
	 * Get the extended error status of this task that can be used in the error notification (if any).
	 */
	virtual FText GetError() const = 0;

	/**
	 * Get a description of this task that can be used in the progress notification (if any).
	 */
	virtual FText GetDescription() const = 0;
};

struct FConcertCreateSessionArgs
{
	/** The desired name for the session */
	FString SessionName;

	/** Set a name if the session should restore a saved session */
	FString SessionToRestore;

	/** Set a name if this session should be saved when it's deleted/destroyed/closed on the server */
	FString SaveSessionAs;
};

/** Interface for Concert client */
class IConcertClient
{
public:
	virtual ~IConcertClient() {}

	/** 
	 *	Configure the client settings and its information
	 */
	virtual void Configure(const UConcertClientConfig* InSettings) = 0;

	/**
	 * Return true if the client has been configured.
	 */
	virtual bool IsConfigured() const = 0;

	/**
	 *	Get the client information set by Configure
	 */
	virtual const FConcertClientInfo& GetClientInfo() const = 0;

	/**
	 *	Returns if the client has already been started up.
	 */
	virtual bool IsStarted() const = 0;

	/**
	 *	Startup the client, this can be called multiple time
	 *	Configure needs to be called before startup
	 */
	virtual void Startup() = 0;

	/**
	 *	Shutdown the client, its discovery and session, if any.
	 *	This can be called multiple time with no ill effect.
	 *	However it depends on the UObject system so need to be called before its exit.
	 */
	virtual void Shutdown() = 0;
	
	/**
	 *	Returns true if server discovery is enabled.
	 */
	virtual bool IsDiscoveryEnabled() const = 0;


	/**
	 *	Start the discovery service for the client
	 *	This will look for Concert server and populate the known servers list
	 *	@see GetKnownServers
	 */
	virtual void StartDiscovery() = 0;

	/**
	 *	Stop the discovery service for the client
	 */
	virtual void StopDiscovery() = 0;

	/**
	 * Try to connect the client to his default session on his default server
	 */
	virtual void DefaultConnect() = 0;

	/**
	 * Disable current auto connection if currently enabled.
	 */
	virtual void ResetAutoConnect() = 0;

	/**
	 * Returns true if the client has an active auto connection routine.
	 */
	virtual bool HasAutoConnection() const = 0;

	/**
	 * Get the list of discovered server information
	 */
	virtual TArray<FConcertServerInfo> GetKnownServers() const = 0;

	/**
	 * Get the delegate callback for when the known server list is updated
	 */
	virtual FSimpleMulticastDelegate& OnKnownServersUpdated() = 0;

	/**
	 * Get the delegate that is called right before the client session startup
	 */
	virtual FOnConcertClientSessionStartupOrShutdown& OnSessionStartup() = 0;

	/**
	 * Get the delegate that is called right before the client session shutdown
	 */
	virtual FOnConcertClientSessionStartupOrShutdown& OnSessionShutdown() = 0;

	/**
	 * Get the delegate that is called to get the pre-connection tasks for a client session
	 */
	virtual FOnConcertClientSessionGetPreConnectionTasks& OnGetPreConnectionTasks() = 0;

	/**
	 * Get the delegate that is called when the session connection state changes
	 */
	virtual FOnConcertClientSessionConnectionChanged& OnSessionConnectionChanged() = 0;

	/**
	 * Get the connection status of client session or disconnected if no session is present
	 * @see EConcertConnectionStatus
	 */
	virtual EConcertConnectionStatus GetSessionConnectionStatus() const = 0;

	/** 
	 * Create a session on the server, matching the client configured settings.
	 * This also initiates the connection handshake for that session with the client.
	 * @param ServerAdminEndpointId	The Id of the Concert Server query endpoint
	 * @param CreateSessionArgs			The arguments that will be use for the creation of the session
	 * @return A future that will contains the final response code of the request
	 */
	virtual TFuture<EConcertResponseCode> CreateSession(const FGuid& ServerAdminEndpointId, const FConcertCreateSessionArgs& CreateSessionArgs) = 0;

	/**
	 * Join a session on the server, the settings of the sessions needs to be compatible with the client settings
	 * or the connection will be refused.
	 * @param ServerAdminEndpointId	The Id of the Concert Server query endpoint
	 * @param SessionName			The name of the session
	 * @return  A future that will contains the final response code of the request
	 */
	virtual TFuture<EConcertResponseCode> JoinSession(const FGuid& ServerAdminEndpointId, const FString& SessionName) = 0;

	/**
	 * Delete a session on the server if the client is the owner of the session.
	 * If the client is not the owner the request will be refused.
	 * @param ServerAdminEndpointId	The Id of the Concert Server query endpoint
	 * @param SessionName			The name of the session to delete
	 * @return A future that will contains the final response code of the request
	 */
	virtual TFuture<EConcertResponseCode> DeleteSession(const FGuid& ServerAdminEndpointId, const FString& SessionName) = 0;

	/** 
	 * Disconnect from the current session.
	 */
	virtual void DisconnectSession() = 0;

	/**
	 * Resume live-updates for the current session (must be paired with a call to SuspendSession).
	 */
	virtual void ResumeSession() = 0;

	/**
	 * Suspend live-updates for the current session.
	 */
	virtual void SuspendSession() = 0;

	/**
	 * Does the current session have live-updates suspended?
	 */
	virtual bool IsSessionSuspended() const = 0;

	/**
	 * Does the client think he is the owner of the session?
	 */
	virtual bool IsOwnerOf(const FConcertSessionInfo& InSessionInfo) const = 0;

	/**
	 * Get the current client session (if any).
	 */
	virtual TSharedPtr<IConcertClientSession> GetCurrentSession() const = 0;

	/** 
	 * Get the list of sessions available on a server
	 * @param ServerAdminEndpointId The Id of the Concert server admin endpoint
	 * @return A future for FConcertAdmin_GetSessionsResponse which contains a list of sessions
	 */
	virtual TFuture<FConcertAdmin_GetSessionsResponse> GetServerSessions(const FGuid& ServerAdminEndpointId) const = 0;

	/**
	 * Get the list of clients connected to a session on the server
	 * @param ServerAdminEndpointId	The Id of the Concert server admin endpoint
	 * @param SessionName			The name of the session
	 * @return A future for FConcertAdmin_GetSessionClientsResponse which contains a list of session clients
	 */
	virtual TFuture<FConcertAdmin_GetSessionClientsResponse> GetSessionClients(const FGuid& ServerAdminEndpointId, const FString& SessionName) const = 0;

	/**
	 * Get the list of the saved sessions data from a server
	 * @param ServerAdminEndpointId	The Id of the concert sever admin endpoint
	 * @return A future for FConcertAdmin_GetSavedSessionNamesResponse which contains the list of the saved session names.
	 */
	virtual TFuture<FConcertAdmin_GetSavedSessionNamesResponse> GetSavedSessionNames(const FGuid& ServerAdminEndpointId) const = 0;

};
