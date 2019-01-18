// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSession.h"
#include "ConcertMessages.h"

class IConcertLocalEndpoint;
struct FConcertServerSettings;

/** Implementation for Concert Server sessions */
class FConcertServerSession : public IConcertServerSession
{
public:
	FConcertServerSession(const FConcertSessionInfo& InSessionInfo, const FConcertServerSettings& InSettings, TSharedPtr<IConcertLocalEndpoint> Endpoint, const FString& InWorkingDirectory);

	virtual ~FConcertServerSession();

	virtual const FString& GetName() const override
	{
		return SessionInfo.SessionName;
	}

	virtual const FConcertSessionInfo& GetSessionInfo() const override
	{
		return SessionInfo;
	}

	virtual TArray<FGuid> GetSessionClientEndpointIds() const override;
	virtual TArray<FConcertSessionClientInfo> GetSessionClients() const override;
	virtual bool FindSessionClient(const FGuid& EndpointId, FConcertSessionClientInfo& OutSessionClientInfo) const override;

	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual FConcertScratchpadRef GetScratchpad() const override;
	virtual FConcertScratchpadPtr GetClientScratchpad(const FGuid& ClientEndpointId) const override;

	virtual FOnConcertServerSessionTick& OnTick() override;
	virtual FOnConcertServerSessionClientChanged& OnSessionClientChanged() override;
	virtual FString GetSessionWorkingDirectory() const override;

protected:
	virtual void InternalRegisterCustomEventHandler(const FName& EventMessageType, const TSharedRef<IConcertSessionCustomEventHandler>& Handler) override;
	virtual void InternalUnregisterCustomEventHandler(const FName& EventMessageType) override;
	virtual void InternalSendCustomEvent(const UScriptStruct* EventType, const void* EventData, const TArray<FGuid>& DestinationEndpointIds, EConcertMessageFlags Flags) override;
	virtual void InternalRegisterCustomRequestHandler(const FName& RequestMessageType, const TSharedRef<IConcertSessionCustomRequestHandler>& Handler) override;
	virtual void InternalUnregisterCustomRequestHandler(const FName& RequestMessageType) override;
	virtual void InternalSendCustomRequest(const UScriptStruct* RequestType, const void* RequestData, const FGuid& DestinationEndpointId, const TSharedRef<IConcertSessionCustomResponseHandler>& Handler) override;

private:
	/** */
	void HandleRemoteConnectionChanged(const FConcertEndpointContext& RemoteEndpointContext, EConcertRemoteEndpointConnection Connection);

	/**  */
	void HandleDiscoverAndJoinSessionEvent(const FConcertMessageContext& Context);

	/**  */
	void HandleLeaveSessionEvent(const FConcertMessageContext& Context);
	
	/** */
	void HandleCustomEvent(const FConcertMessageContext& Context);

	/** */
	TFuture<FConcertSession_CustomResponse> HandleCustomRequest(const FConcertMessageContext& Context);

	/**  */
	void SendClientListUpdatedEvent();

	/**  */
	void TickConnections(float DeltaSeconds);

	/** */
	void CheckConnectionsTimeout();

	/** Session */
	FConcertSessionInfo SessionInfo;

	/** This session endpoint where message are sent and received from. */
	IConcertLocalEndpointPtr ServerSessionEndpoint;

	/** Ticker for the session */
	FDelegateHandle SessionTick;

	/** Callback for when a server session ticks */
	FOnConcertServerSessionTick OnTickDelegate;

	/** Callback for when a session client state changes */
	FOnConcertServerSessionClientChanged OnSessionClientChangedDelegate;

	/** Delegate Handle for remote connection changed callback on the endpoint */
	FDelegateHandle RemoteConnectionChangedHandle;

	/** This client scratchpad */
	FConcertScratchpadPtr Scratchpad;

	/** Map of current session clients */
	struct FSessionClient
	{
		FConcertSessionClientInfo ClientInfo;
		FConcertScratchpadPtr Scratchpad;
	};
	TMap<FGuid, FSessionClient> SessionClients;

	/** Map of session custom event handlers */
	TMap<FName, TSharedPtr<IConcertSessionCustomEventHandler>> CustomEventHandlers;

	/** Map of session custom request handlers */
	TMap<FName, TSharedPtr<IConcertSessionCustomRequestHandler>> CustomRequestHandlers;

	/** The timespan at which session updates are processed. */
	const FTimespan SessionTickFrequency;

	/** The directory where this session will store its files */
	const FString SessionDirectory;
};
