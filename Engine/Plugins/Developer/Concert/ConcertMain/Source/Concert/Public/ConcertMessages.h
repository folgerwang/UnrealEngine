// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"

#include "ConcertTransportMessages.h"
#include "ConcertMessageData.h"
#include "ConcertMessages.generated.h"

/** Connection status for Concert client sessions */
UENUM()
enum class EConcertConnectionStatus : uint8
{
	/** Currently establishing connection to the server session */
	Connecting,
	/** Connection established and alive */
	Connected,
	/** Currently severing connection to the server session gracefully */
	Disconnecting,
	/** Disconnected */
	Disconnected,
};

/** Connection Result for Concert client session */
UENUM()
enum class EConcertConnectionResult : uint8
{
	/** Server has accepted connection */
	ConnectionAccepted,
	/** Server has refused the connection session messages beside other connection request are ignored */
	ConnectionRefused,
	/** Server already accepted connection */
	AlreadyConnected
};

/** Status for Concert session clients */
UENUM()
enum class EConcertClientStatus : uint8
{
	/** Client connected */
	Connected,
	/** Client disconnected */
	Disconnected,
	/** Client state updated */
	Updated,
};

/** Response codes for a session custom request */
UENUM()
enum class EConcertSessionResponseCode : uint8
{
	/** The request data was valid. A response was generated. */
	Success,
	/** The request data was valid, but the request failed. A response was generated. */
	Failed,
	/** The request data was invalid. No response was generated. */
	InvalidRequest,
};

USTRUCT()
struct FConcertAdmin_DiscoverServersEvent : public FConcertEndpointDiscoveryEvent
{
	GENERATED_BODY()

	// TODO: some query argument
};

USTRUCT()
struct FConcertAdmin_ServerDiscoveredEvent : public FConcertEndpointDiscoveryEvent
{
	GENERATED_BODY()

	/** Server designated name */
	UPROPERTY(VisibleAnywhere, Category = "Concert Message")
	FString ServerName;

	/** Basic information about the server instance */
	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertInstanceInfo InstanceInfo;

	/** Contains information on the server settings */
	UPROPERTY(VisibleAnywhere, Category = "Concert Message")
	EConcertSeverFlags ServerFlags;
};

USTRUCT()
struct FConcertAdmin_GetSavedSessionNamesRequest : public FConcertRequestData
{
	GENERATED_BODY()
};

USTRUCT()
struct FConcertAdmin_GetSavedSessionNamesResponse : public FConcertResponseData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Concert Message")
	TArray<FString> SavedSessionNames;
};

USTRUCT()
struct FConcertAdmin_CreateSessionRequest : public FConcertRequestData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FString SessionName;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertClientInfo OwnerClientInfo;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertSessionSettings SessionSettings;
};

USTRUCT()
struct FConcertAdmin_FindSessionRequest : public FConcertRequestData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FString SessionName;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertClientInfo OwnerClientInfo;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertSessionSettings SessionSettings;
};

USTRUCT()
struct FConcertAdmin_SessionInfoResponse : public FConcertResponseData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertSessionInfo SessionInfo; // TODO: Split session Id out of session info
};

USTRUCT()
struct FConcertAdmin_DeleteSessionRequest : public FConcertRequestData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Concert Message")
	FString SessionName;

	//For now only the user name and device name of the client is used to id him as the owner of a session
	UPROPERTY(VisibleAnywhere, Category = "Concert Message")
	FString UserName;
	UPROPERTY(VisibleAnywhere, Category = "Concert Message")
	FString DeviceName;
};

USTRUCT()
struct FConcertAdmin_GetSessionsRequest : public FConcertRequestData
{
	GENERATED_BODY()

	// TODO: filter?
};

USTRUCT()
struct FConcertAdmin_GetSessionsResponse : public FConcertResponseData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	TArray<FConcertSessionInfo> Sessions;
};

USTRUCT()
struct FConcertAdmin_GetSessionClientsRequest : public FConcertRequestData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FString SessionName;
};

USTRUCT()
struct FConcertAdmin_GetSessionClientsResponse : public FConcertResponseData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	TArray<FConcertSessionClientInfo> SessionClients;
};

USTRUCT()
struct FConcertSession_DiscoverAndJoinSessionEvent : public FConcertEndpointDiscoveryEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FGuid SessionServerEndpointId;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FConcertClientInfo ClientInfo;
};

USTRUCT()
struct FConcertSession_JoinSessionResultEvent : public FConcertEndpointDiscoveryEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FGuid SessionServerEndpointId;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	EConcertConnectionResult ConnectionResult;

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	TArray<FConcertSessionClientInfo> SessionClients;
};

USTRUCT()
struct FConcertSession_LeaveSessionEvent : public FConcertEventData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	FGuid SessionServerEndpointId;
};

USTRUCT()
struct FConcertSession_ClientListUpdatedEvent : public FConcertEventData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Concert Message")
	TArray<FConcertSessionClientInfo> SessionClients;
};

USTRUCT()
struct FConcertSession_CustomEvent : public FConcertEventData
{
	GENERATED_BODY()

	virtual bool IsSafeToHandle() const override
	{
		return !(GIsSavingPackage || IsGarbageCollecting());
	}

	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	FGuid SourceEndpointId;

	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	TArray<FGuid> DestinationEndpointIds;

	/** The serialized payload that we're hosting. */
	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	FConcertSessionSerializedPayload SerializedPayload;
};

USTRUCT()
struct FConcertSession_CustomRequest : public FConcertRequestData
{
	GENERATED_BODY()

	virtual bool IsSafeToHandle() const override
	{
		return !(GIsSavingPackage || IsGarbageCollecting());
	}

	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	FGuid SourceEndpointId;

	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	FGuid DestinationEndpointId;

	/** The serialized payload that we're hosting. */
	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	FConcertSessionSerializedPayload SerializedPayload;
};

USTRUCT()
struct FConcertSession_CustomResponse : public FConcertResponseData
{
	GENERATED_BODY()

	virtual bool IsSafeToHandle() const override
	{
		return !(GIsSavingPackage || IsGarbageCollecting());
	}

	/** Set the internal Concert response code from the custom response code from the request handler */
	void SetResponseCode(const EConcertSessionResponseCode InResponseCode)
	{
		switch (InResponseCode)
		{
		case EConcertSessionResponseCode::Success:
			ResponseCode = EConcertResponseCode::Success;
			break;
		case EConcertSessionResponseCode::Failed:
			ResponseCode = EConcertResponseCode::Failed;
			break;
		case EConcertSessionResponseCode::InvalidRequest:
			ResponseCode = EConcertResponseCode::InvalidRequest;
			break;
		default:
			checkf(false, TEXT("Unknown EConcertSessionResponseCode!"));
			break;
		}
	}

	/** The serialized payload that we're hosting. */
	UPROPERTY(VisibleAnywhere, Category="Concert Custom Message")
	FConcertSessionSerializedPayload SerializedPayload;
};
