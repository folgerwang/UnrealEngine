// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "ConcertSettings.h"
#include "ConcertMessageData.generated.h"

class FStructOnScope;

UENUM()
enum class EConcertSeverFlags : uint8
{
	None = 0,
	//The server will ignore the session requirement when someone try to join a session
	IgnoreSessionRequirement = 1 << 0,
};
ENUM_CLASS_FLAGS(EConcertSeverFlags)


/** Holds info on an instance communicating through concert */
USTRUCT()
struct FConcertInstanceInfo
{
	GENERATED_BODY();

	/** Initialize this instance information based on the current environment */
	CONCERT_API void Initialize();

	/** Create a user friendly display string for using in things such as tooltips. */
	CONCERT_API FText ToDisplayString() const;

	/** Holds the instance identifier. */
	UPROPERTY(VisibleAnywhere, Category="Instance Info")
	FGuid InstanceId;

	/** Holds the instance name. */
	UPROPERTY(VisibleAnywhere, Category="Instance Info")
	FString InstanceName;

	/** Holds the instance type (Editor, Game, Server, etc). */
	UPROPERTY(VisibleAnywhere, Category="Instance Info")
	FString InstanceType; // TODO: enum?
};

/** Holds info on a Concert server */
USTRUCT()
struct FConcertServerInfo
{
	GENERATED_BODY();

	/** Initialize this server information based on the current environment */
	CONCERT_API void Initialize();

	/** Create a user friendly display string for using in things such as tooltips. */
	CONCERT_API FText ToDisplayString() const;

	/** Server endpoint for performing administration tasks (FConcertAdmin_X messages) */
	UPROPERTY()
	FGuid AdminEndpointId;

	UPROPERTY()
	FString ServerName;

	/** Basic server information */
	UPROPERTY(VisibleAnywhere, Category="Server Info")
	FConcertInstanceInfo InstanceInfo;

	/** Contains information on the server settings */
	UPROPERTY(VisibleAnywhere, Category = "Server Info")
	EConcertSeverFlags ServerFlags;
};

/** Holds info on a client connected through concert */
USTRUCT()
struct FConcertClientInfo
{
	GENERATED_BODY()

	/** Initialize this client information based on the current environment */
	CONCERT_API void Initialize();

	/** Create a user friendly display string for using in things such as tooltips. */
	CONCERT_API FText ToDisplayString() const;

	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FConcertInstanceInfo InstanceInfo;

	/** Holds the name of the device that the instance is running on. */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FString DeviceName;

	/** Holds the name of the platform that the instance is running on. */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FString PlatformName;

	/** Holds the name of the user that owns this instance. */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FString UserName;

	/** Holds the display name of the user that owns this instance. */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FString DisplayName;

	/** Holds the color of the user avatar in a session. */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FLinearColor AvatarColor;

	/** Holds the string representation of the desktop actor class to be used as the avatar for a representation of a client */
	UPROPERTY(VisibleAnywhere, Category = "Client Info")
	FString DesktopAvatarActorClass;

	/** Holds the string representation of the VR actor class to be used as the avatar for a representation of a client */
	UPROPERTY(VisibleAnywhere, Category = "Client Info")
	FString VRAvatarActorClass;

	/** True if this instance was built with editor-data */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	bool bHasEditorData;

	/** True if this platform requires cooked data */
	UPROPERTY(VisibleAnywhere, Category="Client Info")
	bool bRequiresCookedData;
};

/** Holds information on session client */
USTRUCT()
struct FConcertSessionClientInfo
{
	GENERATED_BODY()

	/** Create a user friendly display string for using in things such as tooltips. */
	CONCERT_API FText ToDisplayString() const;

	UPROPERTY()
	FGuid ClientEndpointId;

	UPROPERTY(VisibleAnywhere, Category="Client Info")
	FConcertClientInfo ClientInfo;
};

/** Holds info on a session */
USTRUCT()
struct FConcertSessionInfo
{
	GENERATED_BODY()

	/** Create a user friendly display string for using in things such as tooltips. */
	CONCERT_API FText ToDisplayString() const;

	UPROPERTY(VisibleAnywhere, Category="Session Info")
	FGuid ServerInstanceId;

	UPROPERTY(VisibleAnywhere, Category="Session Info")
	FGuid ServerEndpointId;

	UPROPERTY(VisibleAnywhere, Category="Session Info")
	FGuid OwnerInstanceId;

	UPROPERTY(VisibleAnywhere, Category="Session Info")
	FString SessionName;

	UPROPERTY(VisibleAnywhere, Category="Session Info")
	FString OwnerUserName;

	UPROPERTY(VisibleAnywhere, Category = "Session Info")
	FString OwnerDeviceName;

	/** Settings pertaining to project, build version, change list number etc */ 
	UPROPERTY(VisibleAnywhere, Category="Session Info")
	FConcertSessionSettings Settings;
};

USTRUCT()
struct FConcertSessionSerializedPayload
{
	GENERATED_BODY()

	/** Initialize this payload from the given data */
	CONCERT_API bool SetPayload(const FStructOnScope& InPayload);
	CONCERT_API bool SetPayload(const UScriptStruct* InPayloadType, const void* InPayloadData);

	/** Extract the payload into an in-memory instance */
	CONCERT_API bool GetPayload(FStructOnScope& OutPayload) const;

	/** Get a hash of the payload data */
	CONCERT_API uint32 GetPayloadDataHash() const;

	/** The typename of the user-defined payload. */
	UPROPERTY(VisibleAnywhere, Category="Payload")
	FName PayloadTypeName;

	/** The uncompressed size of the user-defined payload data. */
	UPROPERTY(VisibleAnywhere, Category="Payload")
	int32 UncompressedPayloadSize;

	/** The data of the user-defined payload (stored as compressed binary for compact transfer). */
	UPROPERTY()
	TArray<uint8> CompressedPayload;
};