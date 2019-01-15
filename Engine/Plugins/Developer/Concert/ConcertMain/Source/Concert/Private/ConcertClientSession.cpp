// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientSession.h"
#include "ConcertLogGlobal.h"
#include "IConcertEndpoint.h"
#include "Scratchpad/ConcertScratchpad.h"

#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"

// TODO: change ptr to ref
FConcertClientSession::FConcertClientSession(const FConcertSessionInfo& InSessionInfo, const FConcertClientInfo& InClientInfo, const FConcertClientSettings& InSettings, TSharedPtr<IConcertLocalEndpoint> Endpoint)
	: SessionInfo(InSessionInfo)
	, ClientInfo(InClientInfo)
	, ConnectionStatus(EConcertConnectionStatus::Disconnected)
	, ClientSessionEndpoint(MoveTemp(Endpoint))
	, SuspendedCount(0)
	, LastConnectionTick(0)
	, SessionTickFrequency(0, 0, InSettings.SessionTickFrequencySeconds)
{}

FConcertClientSession::~FConcertClientSession()
{
	// if the SessionTick is valid, Shutdown wasn't called
	check(!SessionTick.IsValid());
}

FString FConcertClientSession::GetSessionWorkingDirectory() const
{
	return FPaths::ProjectIntermediateDir() / TEXT("Concert") / FString::Printf(TEXT("%s_%s"), *GetName(), *FApp::GetInstanceId().ToString());
}

TArray<FGuid> FConcertClientSession::GetSessionClientEndpointIds() const
{
	TArray<FGuid> EndpointIds;
	SessionClients.GenerateKeyArray(EndpointIds);
	return EndpointIds;
}

TArray<FConcertSessionClientInfo> FConcertClientSession::GetSessionClients() const
{
	TArray<FConcertSessionClientInfo> ClientInfos;
	ClientInfos.Reserve(SessionClients.Num());
	for (const auto& SessionClientPair : SessionClients)
	{
		ClientInfos.Add(SessionClientPair.Value.ClientInfo);
	}
	return ClientInfos;
}

bool FConcertClientSession::FindSessionClient(const FGuid& EndpointId, FConcertSessionClientInfo& OutSessionClientInfo) const
{
	if (const FSessionClient* FoundSessionClient =  SessionClients.Find(EndpointId))
	{
		OutSessionClientInfo = FoundSessionClient->ClientInfo;
		return true;
	}
	return false;
}

void FConcertClientSession::Startup()
{
	// if the session tick isn't valid we haven't started
	if (!SessionTick.IsValid())
	{
		// Register to connection changed event
		RemoteConnectionChangedHandle = ClientSessionEndpoint->OnRemoteEndpointConnectionChanged().AddRaw(this, &FConcertClientSession::HandleRemoteConnectionChanged);

		// Setup the session handlers
		ClientSessionEndpoint->RegisterEventHandler<FConcertSession_JoinSessionResultEvent>(this, &FConcertClientSession::HandleJoinSessionResultEvent);
		ClientSessionEndpoint->RegisterEventHandler<FConcertSession_ClientListUpdatedEvent>(this, &FConcertClientSession::HandleClientListUpdatedEvent);

		// Setup Handlers for custom session messages
		ClientSessionEndpoint->RegisterEventHandler<FConcertSession_CustomEvent>(this, &FConcertClientSession::HandleCustomEvent);
		ClientSessionEndpoint->RegisterRequestHandler<FConcertSession_CustomRequest, FConcertSession_CustomResponse>(this, &FConcertClientSession::HandleCustomRequest);

		// Create your local scratchpad
		Scratchpad = MakeShared<FConcertScratchpad, ESPMode::ThreadSafe>();

		// Setup the session tick
		SessionTick = FTicker::GetCoreTicker().AddTicker(TEXT("ClientSession"), 0, [this](float DeltaSeconds) {
			const FDateTime UtcNow = FDateTime::UtcNow();
			TickConnection(DeltaSeconds, UtcNow);
			return true;
		});

		UE_LOG(LogConcert, Display, TEXT("Initialized Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
	}
}

void FConcertClientSession::Shutdown()
{
	if (SessionTick.IsValid())
	{
		// Unregister connection changed
		ClientSessionEndpoint->OnRemoteEndpointConnectionChanged().Remove(RemoteConnectionChangedHandle);
		RemoteConnectionChangedHandle.Reset();

		// Unregister the session handlers
		ClientSessionEndpoint->UnregisterEventHandler<FConcertSession_JoinSessionResultEvent>();
		ClientSessionEndpoint->UnregisterEventHandler<FConcertSession_ClientListUpdatedEvent>();

		// Unregister handlers for the custom session messages
		ClientSessionEndpoint->UnregisterEventHandler<FConcertSession_CustomEvent>();
		ClientSessionEndpoint->UnregisterRequestHandler<FConcertSession_CustomRequest>();

		// Reset your scratchpad
		Scratchpad.Reset();

		// Unregister the session tick
		FTicker::GetCoreTicker().RemoveTicker(SessionTick);
		SessionTick.Reset();

		UE_LOG(LogConcert, Display, TEXT("Shutdown Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
	}
}

void FConcertClientSession::Connect()
{
	if (ConnectionStatus == EConcertConnectionStatus::Disconnected)
	{
		// Start connection handshake with server session
		ConnectionStatus = EConcertConnectionStatus::Connecting;
		OnConnectionChangedDelegate.Broadcast(*this, ConnectionStatus);
		UE_LOG(LogConcert, Display, TEXT("Connecting to Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
		SendConnectionRequest();
	}
}

void FConcertClientSession::Disconnect()
{
	if (ConnectionStatus != EConcertConnectionStatus::Disconnected)
	{
		if (ConnectionStatus == EConcertConnectionStatus::Connected)
		{
			SendDisconnection();
		}
		ConnectionStatus = EConcertConnectionStatus::Disconnected;
		UpdateSessionClients(TArray<FConcertSessionClientInfo>());

		// Send Disconnected event
		OnConnectionChangedDelegate.Broadcast(*this, ConnectionStatus);

		UE_LOG(LogConcert, Display, TEXT("Disconnected from Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
	}
}

void FConcertClientSession::Resume()
{
	check(IsSuspended());
	--SuspendedCount;

	UE_LOG(LogConcert, Display, TEXT("Resumed Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
}

void FConcertClientSession::Suspend()
{
	++SuspendedCount;

	UE_LOG(LogConcert, Display, TEXT("Suspended Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
}

bool FConcertClientSession::IsSuspended() const
{
	return ConnectionStatus == EConcertConnectionStatus::Connected && SuspendedCount > 0;
}

FOnConcertClientSessionTick& FConcertClientSession::OnTick()
{
	return OnTickDelegate;
}

FOnConcertClientSessionConnectionChanged& FConcertClientSession::OnConnectionChanged()
{
	return OnConnectionChangedDelegate;
}

FOnConcertClientSessionClientChanged& FConcertClientSession::OnSessionClientChanged()
{
	return OnSessionClientChangedDelegate;
}

FConcertScratchpadRef FConcertClientSession::GetScratchpad() const
{
	return Scratchpad.ToSharedRef();
}

FConcertScratchpadPtr FConcertClientSession::GetClientScratchpad(const FGuid& ClientEndpointId) const
{
	if (const FSessionClient* FoundSessionClient = SessionClients.Find(ClientEndpointId))
	{
		return FoundSessionClient->Scratchpad;
	}
	return nullptr;
}

void FConcertClientSession::InternalRegisterCustomEventHandler(const FName& EventMessageType, const TSharedRef<IConcertSessionCustomEventHandler>& Handler)
{
	CustomEventHandlers.Add(EventMessageType, Handler);
}

void FConcertClientSession::InternalUnregisterCustomEventHandler(const FName& EventMessageType)
{
	CustomEventHandlers.Remove(EventMessageType);
}

void FConcertClientSession::InternalSendCustomEvent(const UScriptStruct* EventType, const void* EventData, const TArray<FGuid>& DestinationEndpointIds, EConcertMessageFlags Flags)
{
	if (DestinationEndpointIds.Num() == 0)
	{
		return;
	}

	// TODO: don't send if not connected

	// Serialize the event
	FConcertSession_CustomEvent CustomEvent;
	CustomEvent.SerializedPayload.SetPayload(EventType, EventData);

	// Set the source endpoint
	CustomEvent.SourceEndpointId = GetSessionClientEndpointId();

	// Set the destination endpoints
	CustomEvent.DestinationEndpointIds = DestinationEndpointIds;

	// Send the event
	ClientSessionEndpoint->SendEvent(CustomEvent, SessionInfo.ServerEndpointId, Flags);
}

void FConcertClientSession::InternalRegisterCustomRequestHandler(const FName& RequestMessageType, const TSharedRef<IConcertSessionCustomRequestHandler>& Handler)
{
	CustomRequestHandlers.Add(RequestMessageType, Handler);
}

void FConcertClientSession::InternalUnregisterCustomRequestHandler(const FName& RequestMessageType)
{
	CustomRequestHandlers.Remove(RequestMessageType);
}

void FConcertClientSession::InternalSendCustomRequest(const UScriptStruct* RequestType, const void* RequestData, const FGuid& DestinationEndpointId, const TSharedRef<IConcertSessionCustomResponseHandler>& Handler)
{
	// TODO: don't send if not connected

	// Serialize the request
	FConcertSession_CustomRequest CustomRequest;
	CustomRequest.SerializedPayload.SetPayload(RequestType, RequestData);

	// Set the source endpoint
	CustomRequest.SourceEndpointId = GetSessionClientEndpointId();

	// Set the destination endpoint
	CustomRequest.DestinationEndpointId = DestinationEndpointId;

	ClientSessionEndpoint->SendRequest<FConcertSession_CustomRequest, FConcertSession_CustomResponse>(CustomRequest, SessionInfo.ServerEndpointId)
		.Next([Handler = Handler, SessionEndpointId = CustomRequest.SourceEndpointId](const FConcertSession_CustomResponse& Response)
		{
			// TODO: Improve all of this? generalized erased Promise?
			const void* ResponseStruct = nullptr;
			FStructOnScope ResponseRawPayload;

			if (Response.ResponseCode != EConcertResponseCode::Success)
			{
				// TODO: error
			}
			// Attempt to deserialize the payload
			else if (!Response.SerializedPayload.GetPayload(ResponseRawPayload))
			{
				// TODO: error
			}
			else
			{
				ResponseStruct = ResponseRawPayload.GetStructMemory();
			}

			// Dispatch to external handler
			Handler->HandleResponse(ResponseStruct);
		});
}

void FConcertClientSession::HandleRemoteConnectionChanged(const FConcertEndpointContext& RemoteEndpointContext, EConcertRemoteEndpointConnection Connection)
{
	if (RemoteEndpointContext.EndpointId == SessionInfo.ServerEndpointId && (Connection == EConcertRemoteEndpointConnection::TimedOut || Connection == EConcertRemoteEndpointConnection::ClosedRemotely))
	{
		Disconnect();
	}
}

void FConcertClientSession::HandleJoinSessionResultEvent(const FConcertMessageContext& Context)
{
	const FConcertSession_JoinSessionResultEvent* Message = Context.GetMessage<FConcertSession_JoinSessionResultEvent>();

	// Discard answer not from the expecting session
	if (Message->SessionServerEndpointId != SessionInfo.ServerEndpointId)
	{
		return;
	}

	// If we aren't actively connecting, discard the message
	if (ConnectionStatus != EConcertConnectionStatus::Connecting)
	{
		return;
	}

	// Check the session answer
	switch (Message->ConnectionResult)
	{
		// Connection was refused, go back to disconnected
		case EConcertConnectionResult::ConnectionRefused:
			ConnectionStatus = EConcertConnectionStatus::Disconnected;
			OnConnectionChangedDelegate.Broadcast(*this, ConnectionStatus);
			UE_LOG(LogConcert, Display, TEXT("Disconnected from Concert session '%s' (Owner: %s): Connection Refused."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
			break;
		case EConcertConnectionResult::AlreadyConnected:
			// falls through
		case EConcertConnectionResult::ConnectionAccepted:
			ConnectionAccepted(Message->SessionClients);
			break;
		default:
			break;
	}
}

void FConcertClientSession::HandleClientListUpdatedEvent(const FConcertMessageContext& Context)
{
	const FConcertSession_ClientListUpdatedEvent* Message = Context.GetMessage<FConcertSession_ClientListUpdatedEvent>();

	check(Message->ConcertEndpointId == SessionInfo.ServerEndpointId);

	UpdateSessionClients(Message->SessionClients);
}

void FConcertClientSession::HandleCustomEvent(const FConcertMessageContext& Context)
{
	const FConcertSession_CustomEvent* Message = Context.GetMessage<FConcertSession_CustomEvent>();

	FConcertScratchpadPtr SenderScratchpad = GetClientScratchpad(Message->SourceEndpointId);

	// Attempt to deserialize the payload
	FStructOnScope RawPayload;
	if (Message->SerializedPayload.GetPayload(RawPayload))
	{
		// Dispatch to external handler
		FConcertSessionContext SessionContext{ Message->SourceEndpointId, Message->GetMessageFlags(), SenderScratchpad };
		TSharedPtr<IConcertSessionCustomEventHandler> Handler = CustomEventHandlers.FindRef(RawPayload.GetStruct()->GetFName());
		if (Handler.IsValid())
		{
			Handler->HandleEvent(SessionContext, RawPayload.GetStructMemory());
		}
		else
		{
			// TODO: Unhandled event
		}
	}
}

TFuture<FConcertSession_CustomResponse> FConcertClientSession::HandleCustomRequest(const FConcertMessageContext& Context)
{
	const FConcertSession_CustomRequest* Message = Context.GetMessage<FConcertSession_CustomRequest>();

	FConcertScratchpadPtr SenderScratchpad = GetClientScratchpad(Message->SourceEndpointId);

	// Default response
	FConcertSession_CustomResponse ResponseData;
	ResponseData.ResponseCode = EConcertResponseCode::UnknownRequest;

	// Attempt to deserialize the payload
	FStructOnScope RawPayload;
	if (Message->SerializedPayload.GetPayload(RawPayload))
	{
		// Dispatch to external handler
		TSharedPtr<IConcertSessionCustomRequestHandler> Handler = CustomRequestHandlers.FindRef(RawPayload.GetStruct()->GetFName()); // TODO: thread safety?

		if (Handler.IsValid())
		{
			FStructOnScope ResponsePayload(Handler->GetResponseType());
			FConcertSessionContext SessionContext{ Message->SourceEndpointId, Message->GetMessageFlags(), SenderScratchpad };
			ResponseData.SetResponseCode(Handler->HandleRequest(SessionContext, RawPayload.GetStructMemory(), ResponsePayload.GetStructMemory()));
			if (ResponseData.ResponseCode == EConcertResponseCode::Success || ResponseData.ResponseCode == EConcertResponseCode::Failed)
			{
				ResponseData.SerializedPayload.SetPayload(ResponsePayload);
			}
		}
		else
		{
			// TODO: unhandled Request
		}
	}

	return FConcertSession_CustomResponse::AsFuture(MoveTemp(ResponseData));
}

void FConcertClientSession::TickConnection(float DeltaSeconds, const FDateTime& UtcNow)
{
	if (LastConnectionTick + SessionTickFrequency <= UtcNow)
	{
		switch (ConnectionStatus)
		{
		case EConcertConnectionStatus::Connecting:
			SendConnectionRequest();
			break;
		default:
			// do nothing
			break;
		}
		LastConnectionTick = UtcNow;
	}

	// External callback when connected
	if (ConnectionStatus == EConcertConnectionStatus::Connected)
	{
		OnTickDelegate.Broadcast(*this, DeltaSeconds);
	}
}

void FConcertClientSession::SendConnectionRequest()
{
	FConcertSession_DiscoverAndJoinSessionEvent DiscoverAndJoinSessionEvent;
	DiscoverAndJoinSessionEvent.SessionServerEndpointId = SessionInfo.ServerEndpointId;
	DiscoverAndJoinSessionEvent.ClientInfo = ClientInfo;
	ClientSessionEndpoint->PublishEvent(DiscoverAndJoinSessionEvent);
}

void FConcertClientSession::SendDisconnection()
{
	FConcertSession_LeaveSessionEvent LeaveSessionEvent;
	LeaveSessionEvent.SessionServerEndpointId = SessionInfo.ServerEndpointId;
	ClientSessionEndpoint->SendEvent(LeaveSessionEvent, SessionInfo.ServerEndpointId);
}

void FConcertClientSession::ConnectionAccepted(const TArray<FConcertSessionClientInfo>& InSessionClients)
{
	check(ConnectionStatus != EConcertConnectionStatus::Connected);
	ConnectionStatus = EConcertConnectionStatus::Connected;

	// Raise connected event
	OnConnectionChangedDelegate.Broadcast(*this, ConnectionStatus);

	UE_LOG(LogConcert, Display, TEXT("Connected to Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);

	UpdateSessionClients(InSessionClients);
}

void FConcertClientSession::UpdateSessionClients(const TArray<FConcertSessionClientInfo>& InSessionClients)
{
	// Add any new clients, or update existing ones
	TSet<FGuid> AvailableClientIds;
	AvailableClientIds.Reserve(InSessionClients.Num());
	for (const FConcertSessionClientInfo& SessionClientInfo : InSessionClients)
	{
		if (ClientSessionEndpoint->GetEndpointContext().EndpointId != SessionClientInfo.ClientEndpointId)
		{
			AvailableClientIds.Add(SessionClientInfo.ClientEndpointId);

			if (SessionClients.Contains(SessionClientInfo.ClientEndpointId))
			{
				// TODO: Client updates?
				//OnSessionClientChangedDelegate.Broadcast(*this, EConcertClientStatus::Updated, SessionClientInfo);
			}
			else
			{
				const FSessionClient& SessionClient = SessionClients.Add(SessionClientInfo.ClientEndpointId, FSessionClient{ SessionClientInfo, MakeShared<FConcertScratchpad, ESPMode::ThreadSafe>() });
				OnSessionClientChangedDelegate.Broadcast(*this, EConcertClientStatus::Connected, SessionClientInfo);
				UE_LOG(LogConcert, Display, TEXT("User '%s' (Endpoint: %s) joined Concert session '%s' (Owner: %s)."), *SessionClient.ClientInfo.ClientInfo.UserName, *SessionClient.ClientInfo.ClientEndpointId.ToString(), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
			}
		}
	}

	// Remove any old clients
	for (auto SessionClientIt = SessionClients.CreateIterator(); SessionClientIt; ++SessionClientIt)
	{
		if (!AvailableClientIds.Contains(SessionClientIt.Key()))
		{
			const FSessionClient& SessionClient = SessionClientIt.Value();
			OnSessionClientChangedDelegate.Broadcast(*this, EConcertClientStatus::Disconnected, SessionClient.ClientInfo);
			UE_LOG(LogConcert, Display, TEXT("User '%s' (Endpoint: %s) left Concert session '%s' (Owner: %s)."), *SessionClient.ClientInfo.ClientInfo.UserName, *SessionClient.ClientInfo.ClientEndpointId.ToString(), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
			SessionClientIt.RemoveCurrent();
		}
	}
}
