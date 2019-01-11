// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertServerSession.h"
#include "ConcertLogGlobal.h"
#include "IConcertEndpoint.h"
#include "Scratchpad/ConcertScratchpad.h"

#include "Containers/Ticker.h"
#include "UObject/StructOnScope.h"

FConcertServerSession::FConcertServerSession(const FConcertSessionInfo& InSessionInfo, const FConcertServerSettings& InSettings, TSharedPtr<IConcertLocalEndpoint> Endpoint, const FString& InWorkingDirectory)
	: SessionInfo(InSessionInfo)
	, ServerSessionEndpoint(MoveTemp(Endpoint))
	, SessionTickFrequency(0, 0, InSettings.SessionTickFrequencySeconds)
	, SessionDirectory(InWorkingDirectory / InSessionInfo.SessionName)
{
	// Make sure the session has the correct server endpoint ID set
	SessionInfo.ServerEndpointId = ServerSessionEndpoint->GetEndpointContext().EndpointId;
}

FConcertServerSession::~FConcertServerSession()
{
	// if the SessionTick is valid, Shutdown wasn't called
	check(!SessionTick.IsValid());
}

TArray<FGuid> FConcertServerSession::GetSessionClientEndpointIds() const
{
	TArray<FGuid> EndpointIds;
	SessionClients.GenerateKeyArray(EndpointIds);
	return EndpointIds;
}

TArray<FConcertSessionClientInfo> FConcertServerSession::GetSessionClients() const
{
	TArray<FConcertSessionClientInfo> ClientInfos;
	ClientInfos.Reserve(SessionClients.Num());
	for (const auto& SessionClientPair : SessionClients)
	{
		ClientInfos.Add(SessionClientPair.Value.ClientInfo);
	}
	return ClientInfos;
}

bool FConcertServerSession::FindSessionClient(const FGuid& EndpointId, FConcertSessionClientInfo& OutSessionClientInfo) const
{
	if (const FSessionClient* FoundSessionClient = SessionClients.Find(EndpointId))
	{
		OutSessionClientInfo = FoundSessionClient->ClientInfo;
		return true;
	}
	return false;
}

void FConcertServerSession::Startup()
{
	if (!SessionTick.IsValid())
	{
		// Register to connection changed event
		RemoteConnectionChangedHandle = ServerSessionEndpoint->OnRemoteEndpointConnectionChanged().AddRaw(this, &FConcertServerSession::HandleRemoteConnectionChanged);

		// Setup the session handlers
		ServerSessionEndpoint->SubscribeEventHandler<FConcertSession_DiscoverAndJoinSessionEvent>(this, &FConcertServerSession::HandleDiscoverAndJoinSessionEvent);
		ServerSessionEndpoint->RegisterEventHandler<FConcertSession_LeaveSessionEvent>(this, &FConcertServerSession::HandleLeaveSessionEvent);

		// Setup Handlers for custom session messages
		ServerSessionEndpoint->RegisterEventHandler<FConcertSession_CustomEvent>(this, &FConcertServerSession::HandleCustomEvent);
		ServerSessionEndpoint->RegisterRequestHandler<FConcertSession_CustomRequest, FConcertSession_CustomResponse>(this, &FConcertServerSession::HandleCustomRequest);

		// Create your local scratchpad
		Scratchpad = MakeShared<FConcertScratchpad, ESPMode::ThreadSafe>();

		// Setup the session tick
		SessionTick = FTicker::GetCoreTicker().AddTicker(TEXT("ServerSession"), 0, [this](float DeltaSeconds) {
			TickConnections(DeltaSeconds);
			return true;
		});

		UE_LOG(LogConcert, Display, TEXT("Initialized Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
	}
}

void FConcertServerSession::Shutdown()
{
	if (SessionTick.IsValid())
	{
		// Unregister connection changed
		ServerSessionEndpoint->OnRemoteEndpointConnectionChanged().Remove(RemoteConnectionChangedHandle);
		RemoteConnectionChangedHandle.Reset();

		// Unregister the session handlers
		ServerSessionEndpoint->UnsubscribeEventHandler<FConcertSession_DiscoverAndJoinSessionEvent>();
		ServerSessionEndpoint->UnregisterEventHandler<FConcertSession_LeaveSessionEvent>();

		// Unregister handlers for the custom session messages
		ServerSessionEndpoint->UnregisterEventHandler<FConcertSession_CustomEvent>();
		ServerSessionEndpoint->UnregisterRequestHandler<FConcertSession_CustomRequest>();

		// Reset your scratchpad
		Scratchpad.Reset();

		// Unregister the session tick
		FTicker::GetCoreTicker().RemoveTicker(SessionTick);
		SessionTick.Reset();

		UE_LOG(LogConcert, Display, TEXT("Shutdown Concert session '%s' (Owner: %s)."), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);
	}
}

FOnConcertServerSessionTick& FConcertServerSession::OnTick()
{
	return OnTickDelegate;
}

FOnConcertServerSessionClientChanged& FConcertServerSession::OnSessionClientChanged()
{
	return OnSessionClientChangedDelegate;
}

FString FConcertServerSession::GetSessionWorkingDirectory() const 
{
	return SessionDirectory;
}

FConcertScratchpadRef FConcertServerSession::GetScratchpad() const
{
	return Scratchpad.ToSharedRef();
}

FConcertScratchpadPtr FConcertServerSession::GetClientScratchpad(const FGuid& ClientEndpointId) const
{
	if (const FSessionClient* FoundSessionClient = SessionClients.Find(ClientEndpointId))
	{
		return FoundSessionClient->Scratchpad;
	}
	return nullptr;
}

void FConcertServerSession::InternalRegisterCustomEventHandler(const FName& EventMessageType, const TSharedRef<IConcertSessionCustomEventHandler>& Handler)
{
	CustomEventHandlers.Add(EventMessageType, Handler);
}

void FConcertServerSession::InternalUnregisterCustomEventHandler(const FName& EventMessageType)
{
	CustomEventHandlers.Remove(EventMessageType);
}

void FConcertServerSession::InternalSendCustomEvent(const UScriptStruct* EventType, const void* EventData, const TArray<FGuid>& DestinationEndpointIds, EConcertMessageFlags Flags)
{
	if (DestinationEndpointIds.Num() == 0)
	{
		return;
	}

	// Serialize the event
	FConcertSession_CustomEvent CustomEvent;
	CustomEvent.SerializedPayload.SetPayload(EventType, EventData);

	// Set the source endpoint
	CustomEvent.SourceEndpointId = SessionInfo.ServerEndpointId;

	// Set the destination endpoints
	CustomEvent.DestinationEndpointIds = DestinationEndpointIds;

	// TODO: Optimize this so we can queue the event for multiple client endpoints at the same time
	for (const FGuid& DestinationEndpointId : DestinationEndpointIds)
	{
		// Send the event
		ServerSessionEndpoint->SendEvent(CustomEvent, DestinationEndpointId, Flags);
	}
}

void FConcertServerSession::InternalRegisterCustomRequestHandler(const FName& RequestMessageType, const TSharedRef<IConcertSessionCustomRequestHandler>& Handler)
{
	CustomRequestHandlers.Add(RequestMessageType, Handler);
}

void FConcertServerSession::InternalUnregisterCustomRequestHandler(const FName& RequestMessageType)
{
	CustomRequestHandlers.Remove(RequestMessageType);
}

void FConcertServerSession::InternalSendCustomRequest(const UScriptStruct* RequestType, const void* RequestData, const FGuid& DestinationEndpointId, const TSharedRef<IConcertSessionCustomResponseHandler>& Handler)
{
	// Serialize the request
	FConcertSession_CustomRequest CustomRequest;
	CustomRequest.SerializedPayload.SetPayload(RequestType, RequestData);

	// Set the source endpoint
	CustomRequest.SourceEndpointId = SessionInfo.ServerEndpointId;

	// Set the destination endpoint
	CustomRequest.DestinationEndpointId = DestinationEndpointId;

	ServerSessionEndpoint->SendRequest<FConcertSession_CustomRequest, FConcertSession_CustomResponse>(CustomRequest, DestinationEndpointId)
		.Next([Handler](const FConcertSession_CustomResponse& Response)
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

void FConcertServerSession::HandleRemoteConnectionChanged(const FConcertEndpointContext& RemoteEndpointContext, EConcertRemoteEndpointConnection Connection)
{
	if (Connection == EConcertRemoteEndpointConnection::TimedOut || Connection == EConcertRemoteEndpointConnection::ClosedRemotely)
	{
		// Find the client in our list
		FSessionClient SessionClient;
		if (SessionClients.RemoveAndCopyValue(RemoteEndpointContext.EndpointId, SessionClient))
		{
			OnSessionClientChangedDelegate.Broadcast(*this, EConcertClientStatus::Disconnected, SessionClient.ClientInfo);

			UE_LOG(LogConcert, Display, TEXT("User '%s' (Endpoint: %s) left Concert session '%s' (Owner: %s) due to %s."), 
				*SessionClient.ClientInfo.ClientInfo.UserName, 
				*SessionClient.ClientInfo.ClientEndpointId.ToString(), 
				*SessionInfo.SessionName, *SessionInfo.OwnerUserName, 
				(Connection == EConcertRemoteEndpointConnection::TimedOut ? TEXT("time-out") : TEXT("the remote peer closing the connection"))
			);

			// Send client disconnection notification to other clients
			SendClientListUpdatedEvent();
		}
	}
}

void FConcertServerSession::HandleDiscoverAndJoinSessionEvent(const FConcertMessageContext& Context)
{
	const FConcertSession_DiscoverAndJoinSessionEvent* Message = Context.GetMessage<FConcertSession_DiscoverAndJoinSessionEvent>();

	// If this this isn't a join request for this session, discard the message
	if (Message->SessionServerEndpointId != SessionInfo.ServerEndpointId)
	{
		return;
	}

	FConcertSession_JoinSessionResultEvent JoinReply;
	JoinReply.SessionServerEndpointId = SessionInfo.ServerEndpointId;

	if (SessionClients.Find(Context.SenderConcertEndpointId) != nullptr)
	{
		JoinReply.ConnectionResult = EConcertConnectionResult::AlreadyConnected;
		JoinReply.SessionClients = GetSessionClients();
	}
	// TODO: check connection requirement
	else // if (CheckConnectionRequirement(Message->ClientInfo))
	{
		// Accept the connection
		JoinReply.ConnectionResult = EConcertConnectionResult::ConnectionAccepted;
		JoinReply.SessionClients = GetSessionClients();
	}

	// Send the reply before we invoke the delegate and notify of the client list to ensure that the client knows it's connected before it starts receiving other messages
	ServerSessionEndpoint->SendEvent(JoinReply, Context.SenderConcertEndpointId, EConcertMessageFlags::ReliableOrdered);

	if (JoinReply.ConnectionResult == EConcertConnectionResult::ConnectionAccepted)
	{
		// Add the client to the list
		const FSessionClient& SessionClient = SessionClients.Add(Context.SenderConcertEndpointId, FSessionClient{ FConcertSessionClientInfo{ Context.SenderConcertEndpointId, Message->ClientInfo }, MakeShared<FConcertScratchpad, ESPMode::ThreadSafe>() });
		OnSessionClientChangedDelegate.Broadcast(*this, EConcertClientStatus::Connected, SessionClient.ClientInfo);

		UE_LOG(LogConcert, Display, TEXT("User '%s' (Endpoint: %s) joined Concert session '%s' (Owner: %s)."), *SessionClient.ClientInfo.ClientInfo.UserName, *SessionClient.ClientInfo.ClientEndpointId.ToString(), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);

		// Send client connection notification
		SendClientListUpdatedEvent();
	}
}

void FConcertServerSession::HandleLeaveSessionEvent(const FConcertMessageContext& Context)
{
	const FConcertSession_LeaveSessionEvent* Message = Context.GetMessage<FConcertSession_LeaveSessionEvent>();

	// If this this isn't a connection request for this session, discard the message
	if (Message->SessionServerEndpointId != SessionInfo.ServerEndpointId)
	{
		return;
	}

	// Find the client in our list
	FSessionClient SessionClient;
	if (SessionClients.RemoveAndCopyValue(Context.SenderConcertEndpointId, SessionClient))
	{
		OnSessionClientChangedDelegate.Broadcast(*this, EConcertClientStatus::Disconnected, SessionClient.ClientInfo);

		UE_LOG(LogConcert, Display, TEXT("User '%s' (Endpoint: %s) left Concert session '%s' (Owner: %s) by request."), *SessionClient.ClientInfo.ClientInfo.UserName, *SessionClient.ClientInfo.ClientEndpointId.ToString(), *SessionInfo.SessionName, *SessionInfo.OwnerUserName);

		// Send client disconnection notification to other clients
		SendClientListUpdatedEvent();
	}
}

void FConcertServerSession::HandleCustomEvent(const FConcertMessageContext& Context)
{
	const FConcertSession_CustomEvent* Message = Context.GetMessage<FConcertSession_CustomEvent>();

	// Process or forward this event
	for (const FGuid& DestinationEndpointId : Message->DestinationEndpointIds)
	{
		if (DestinationEndpointId == SessionInfo.ServerEndpointId)
		{
			FConcertScratchpadPtr SenderScratchpad = GetClientScratchpad(Message->SourceEndpointId);

			// Attempt to deserialize the payload
			FStructOnScope RawPayload;
			if (Message->SerializedPayload.GetPayload(RawPayload))
			{
				// Dispatch to external handler
				TSharedPtr<IConcertSessionCustomEventHandler> Handler = CustomEventHandlers.FindRef(RawPayload.GetStruct()->GetFName());
				if (Handler.IsValid())
				{
					FConcertSessionContext SessionContext{ Message->SourceEndpointId, Message->GetMessageFlags(), SenderScratchpad };
					Handler->HandleEvent(SessionContext, RawPayload.GetStructMemory());
				}
				else
				{
					// TODO: Unhandled event
				}
			}
		}
		else if (const FSessionClient* Client = SessionClients.Find(DestinationEndpointId))
		{
			// Forward onto the client
			ServerSessionEndpoint->SendEvent(*Message, Client->ClientInfo.ClientEndpointId, Message->IsReliable() ? EConcertMessageFlags::ReliableOrdered : EConcertMessageFlags::None);
		}
	}
}

TFuture<FConcertSession_CustomResponse> FConcertServerSession::HandleCustomRequest(const FConcertMessageContext& Context)
{
	const FConcertSession_CustomRequest* Message = Context.GetMessage<FConcertSession_CustomRequest>();

	// Default response
	FConcertSession_CustomResponse ResponseData;
	ResponseData.ResponseCode = EConcertResponseCode::UnknownRequest;

	if (Message->DestinationEndpointId == SessionInfo.ServerEndpointId)
	{
		FConcertScratchpadPtr SenderScratchpad = GetClientScratchpad(Message->SourceEndpointId);

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
	}
	else if (const FSessionClient* Client = SessionClients.Find(Message->DestinationEndpointId))
	{
		// Forward onto the client
		return ServerSessionEndpoint->SendRequest<FConcertSession_CustomRequest, FConcertSession_CustomResponse>(*Message, Client->ClientInfo.ClientEndpointId);
	}
	
	return FConcertSession_CustomResponse::AsFuture(MoveTemp(ResponseData));
}

void FConcertServerSession::SendClientListUpdatedEvent()
{
	// Notifying client connection is done by sending the current client list
	FConcertSession_ClientListUpdatedEvent ClientListUpdatedEvent;
	ClientListUpdatedEvent.SessionClients = GetSessionClients();
	for (const auto& SessionClientPair : SessionClients)
	{
		ServerSessionEndpoint->SendEvent(ClientListUpdatedEvent, SessionClientPair.Value.ClientInfo.ClientEndpointId, EConcertMessageFlags::ReliableOrdered);
	}
}

void FConcertServerSession::TickConnections(float DeltaSeconds)
{
	// External callback
	OnTickDelegate.Broadcast(*this, DeltaSeconds);
}