// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSession.h"
#include "ConcertMessages.h"

class IConcertLocalEndpoint;
struct FConcertClientSettings;

/** Implementation of a Concert Client Session */
class FConcertClientSession : public IConcertClientSession
{
public:
	FConcertClientSession(const FConcertSessionInfo& InSessionInfo, const FConcertClientInfo& InClientInfo, const FConcertClientSettings& InSettings, TSharedPtr<IConcertLocalEndpoint> Endpoint);
	virtual ~FConcertClientSession();

	virtual const FString& GetName() const override
	{
		return SessionInfo.SessionName;
	}

	virtual const FConcertSessionInfo& GetSessionInfo() const override
	{
		return SessionInfo;
	}

	virtual FString GetSessionWorkingDirectory() const override;

	virtual TArray<FGuid> GetSessionClientEndpointIds() const override;
	virtual TArray<FConcertSessionClientInfo> GetSessionClients() const override;
	virtual bool FindSessionClient(const FGuid& EndpointId, FConcertSessionClientInfo& OutSessionClientInfo) const override;

	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual FConcertScratchpadRef GetScratchpad() const override;
	virtual FConcertScratchpadPtr GetClientScratchpad(const FGuid& ClientEndpointId) const override;

	virtual EConcertConnectionStatus GetConnectionStatus() const override
	{
		return ConnectionStatus;
	}
	
	virtual FGuid GetSessionClientEndpointId() const override
	{
		return ClientSessionEndpoint->GetEndpointContext().EndpointId;
	}

	virtual FGuid GetSessionServerEndpointId() const override
	{
		return SessionInfo.ServerEndpointId;
	}

	virtual const FConcertClientInfo& GetLocalClientInfo() const override
	{
		return ClientInfo;
	}

	virtual void Connect() override;
	virtual void Disconnect() override;
	virtual void Resume() override;
	virtual void Suspend() override;
	virtual bool IsSuspended() const override;
	virtual FOnConcertClientSessionTick& OnTick() override;
	virtual FOnConcertClientSessionConnectionChanged& OnConnectionChanged() override;
	virtual FOnConcertClientSessionClientChanged& OnSessionClientChanged() override;

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
	void HandleJoinSessionResultEvent(const FConcertMessageContext& Context);
	
	/**  */
	void HandleClientListUpdatedEvent(const FConcertMessageContext& Context);

	/** */
	void HandleCustomEvent(const FConcertMessageContext& Context);

	/** */
	TFuture<FConcertSession_CustomResponse> HandleCustomRequest(const FConcertMessageContext& Context);

	/**  */
	void TickConnection(float DeltaSeconds, const FDateTime& UtcNow);

	/**  */
	void SendConnectionRequest();

	/**  */
	void SendDisconnection();

	/**  */
	void ConnectionAccepted(const TArray<FConcertSessionClientInfo>& InSessionClients);

	/** */
	void UpdateSessionClients(const TArray<FConcertSessionClientInfo>& InSessionClients);

	/** Session Information */
	FConcertSessionInfo SessionInfo;

	/** Information about this Client */
	FConcertClientInfo ClientInfo;

	/** The connection status to the server counterpart */
	EConcertConnectionStatus ConnectionStatus;

	/** This session endpoint where message are sent and received from. */
	IConcertLocalEndpointPtr ClientSessionEndpoint;

	/** Count of the number of times this session has been suspended */
	uint8 SuspendedCount;

	/** Ticker for the session */
	FDelegateHandle SessionTick;

	/** Last connection tick */
	FDateTime LastConnectionTick;

	/** Callback for when a connected session ticks */
	FOnConcertClientSessionTick OnTickDelegate;

	/** Callback for when the session connection state changes */
	FOnConcertClientSessionConnectionChanged OnConnectionChangedDelegate;

	/** Callback for when a session client state changes */
	FOnConcertClientSessionClientChanged OnSessionClientChangedDelegate;

	/** Delegate Handle for remote connection changed callback on the endpoint */
	FDelegateHandle RemoteConnectionChangedHandle;

	/** This client scratchpad */
	FConcertScratchpadPtr Scratchpad;

	/** Map of current other session clients */
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
};
