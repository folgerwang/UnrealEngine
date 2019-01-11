// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertServer.h"
#include "IConcertTransportModule.h"
#include "ConcertSettings.h"

#include "UObject/StrongObjectPtr.h"

class FConcertServerSession;

/** Implements Concert interface */
class FConcertServer : public IConcertServer
{
public: 
	FConcertServer();
	virtual ~FConcertServer();

	virtual void Configure(const UConcertServerConfig* InSettings) override;
	virtual bool IsConfigured() const override;
	virtual bool IsStarted() const override;
	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual FOnConcertServerSessionStartupOrShutdown& OnSessionStartup() override;
	virtual FOnConcertServerSessionStartupOrShutdown& OnSessionShutdown() override;
	virtual FConcertSessionInfo CreateSessionInfo() const override;
	virtual TArray<FConcertSessionInfo> GetSessionsInfo() const override;
	virtual TArray<TSharedPtr<IConcertServerSession>> GetSessions() const override;
	virtual TSharedPtr<IConcertServerSession> GetSession(const FName& SessionName) const override;
	virtual TSharedPtr<IConcertServerSession> CreateSession(const FConcertSessionInfo& SessionInfo) override;
	virtual bool DestroySession(const FName& SessionName) override;

	virtual TArray<FConcertSessionClientInfo> GetSessionClients(const FName& SessionName) const override;
	
	/** Set the endpoint provider for the server */
	void SetEndpointProvider(const TSharedPtr<IConcertEndpointProvider>& Provider);

private:
	/**  */
	void HandleDiscoverServersEvent(const FConcertMessageContext& Context);

	/**  */
	TFuture<FConcertAdmin_SessionInfoResponse> HandleCreateSessionRequest(const FConcertMessageContext& Context);

	/**  */
	TFuture<FConcertAdmin_SessionInfoResponse> HandleFindSessionRequest(const FConcertMessageContext& Context);

	/**  */
	TFuture<FConcertResponseData> HandleDeleteSessionRequest(const FConcertMessageContext& Context);

	/**  */
	TFuture<FConcertAdmin_GetSessionsResponse> HandleGetSessionsRequest(const FConcertMessageContext& Context);

	/** */
	TFuture<FConcertAdmin_GetSessionClientsResponse> HandleGetSessionClientsRequest(const FConcertMessageContext& Context);

	/** */
	TFuture<FConcertAdmin_GetSavedSessionNamesResponse> HandleGetSavedSessionNamesRequest(const FConcertMessageContext& Context);

	/** */
	TSharedPtr<IConcertServerSession> CreateServerSession(const FConcertAdmin_CreateSessionRequest& CreateSessionRequest);
	
	/** Restore the sessions from the working directory if the setting bCleanWorkingDir is false */
	void RestoreSessions();

	/** */
	bool CheckSessionRequirements(const TSharedPtr<IConcertServerSession>& ServerSession, const FConcertSessionSettings& SessionSettings, FText* OutFailureReason = nullptr);

	/** 
	 * Validate that the delete request come form the owner of the session that he want to delete
	 */
	bool IsRequestFromSessionOwner(const TSharedPtr<IConcertServerSession>& SessionToDelete, const FConcertAdmin_DeleteSessionRequest& DeleteSessionRequest);

	TSharedPtr<IConcertServerSession> InternalCreateSession(const FConcertSessionInfo& SessionInfo);

	/** Factory for creating Endpoint */
	TSharedPtr<IConcertEndpointProvider> EndpointProvider;
	
	/** Administration endpoint for the server (i.e. creating, joining sessions) */
	TSharedPtr<IConcertLocalEndpoint> ServerAdminEndpoint;
	
	/** Server and Instance Info */
	FConcertServerInfo ServerInfo;

	/** Delegate of server session startup */
	FOnConcertServerSessionStartupOrShutdown OnSessionStartupDelegate;

	/** Delegate of server session shutdown */
	FOnConcertServerSessionStartupOrShutdown OnSessionShutdownDelegate;

	/** Map of Active Sessions */
	TMap<FName, TSharedPtr<FConcertServerSession>> Sessions;

	/** Server settings object we were configured with */
	TStrongObjectPtr<UConcertServerConfig> Settings;
};
