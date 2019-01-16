// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertWorkspaceData.h"
#include "ConcertWorkspaceMessages.generated.h"

USTRUCT()
struct FConcertWorkspaceSyncEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	int32 RemainingWork;
};

USTRUCT()
struct FConcertWorkspaceSyncTransactionEvent : public FConcertWorkspaceSyncEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	uint64 TransactionIndex;

	UPROPERTY()
	TArray<uint8> TransactionData;
};

USTRUCT()
struct FConcertWorkspaceSyncPackageEvent : public FConcertWorkspaceSyncEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 PackageRevision;

	UPROPERTY()
	FConcertPackage Package;
};

USTRUCT()
struct FConcertWorkspaceSyncLockEvent : public FConcertWorkspaceSyncEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FGuid> LockedResources;
};

USTRUCT()
struct FConcertWorkspaceInitialSyncCompletedEvent
{
	GENERATED_BODY()
};

USTRUCT()
struct FConcertPackageUpdateEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FConcertPackage Package;
};

UENUM()
enum class EConcertResourceLockType : uint8
{
	None,
	Lock,
	Unlock,
};

USTRUCT()
struct FConcertResourceLockEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ClientId;

	UPROPERTY()
	TArray<FName> ResourceNames;

	UPROPERTY()
	EConcertResourceLockType LockType;
};

USTRUCT()
struct FConcertResourceLockRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ClientId;

	UPROPERTY()
	TArray<FName> ResourceNames;

	UPROPERTY()
	EConcertResourceLockType LockType;
};

USTRUCT()
struct FConcertResourceLockResponse
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FGuid> FailedResources;

	UPROPERTY()
	EConcertResourceLockType LockType;
};

UENUM()
enum class EConcertPlaySessionEventType : uint8
{
	BeginPlay,
	SwitchPlay,
	EndPlay,
};

USTRUCT()
struct FConcertPlaySessionEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EConcertPlaySessionEventType EventType;

	UPROPERTY()
	FGuid PlayEndpointId;

	UPROPERTY()
	FName PlayPackageName;

	UPROPERTY()
	bool bIsSimulating;
};
