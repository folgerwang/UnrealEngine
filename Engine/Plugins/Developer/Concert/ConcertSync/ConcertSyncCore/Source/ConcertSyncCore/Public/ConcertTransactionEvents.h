// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "IdentifierTable/ConcertIdentifierTableData.h"
#include "ConcertTransactionEvents.generated.h"

USTRUCT()
struct FConcertObjectId
{
	GENERATED_BODY()

	FConcertObjectId()
		: ObjectPersistentFlags(0)
	{
	}

	explicit FConcertObjectId(const UObject* InObject)
		: ObjectClassPathName(*InObject->GetClass()->GetPathName())
		, ObjectOuterPathName(InObject->GetOuter() ? FName(*InObject->GetOuter()->GetPathName()) : FName())
		, ObjectName(InObject->GetFName())
		, ObjectPersistentFlags(InObject->GetFlags() & RF_Load)
	{
	}

	FConcertObjectId(const FName InObjectClassPathName, const FName InObjectOuterPathName, const FName InObjectName, const uint32 InObjectFlags)
		: ObjectClassPathName(InObjectClassPathName)
		, ObjectOuterPathName(InObjectOuterPathName)
		, ObjectName(InObjectName)
		, ObjectPersistentFlags(InObjectFlags & RF_Load)
	{
	}

	UPROPERTY()
	FName ObjectClassPathName;

	UPROPERTY()
	FName ObjectOuterPathName;

	UPROPERTY()
	FName ObjectName;

	UPROPERTY()
	uint32 ObjectPersistentFlags;
};

USTRUCT()
struct FConcertSerializedObjectData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAllowCreate;

	UPROPERTY()
	bool bIsPendingKill;

	UPROPERTY()
	FName NewName;

	UPROPERTY()
	FName NewOuterPathName;

	UPROPERTY()
	TArray<uint8> SerializedData;
};

USTRUCT()
struct FConcertSerializedPropertyData
{
	GENERATED_BODY()

	UPROPERTY()
	FName PropertyName;

	UPROPERTY()
	TArray<uint8> SerializedData;
};

USTRUCT()
struct FConcertExportedObject
{
	GENERATED_BODY()

	UPROPERTY()
	FConcertObjectId ObjectId;

	UPROPERTY()
	int32 ObjectPathDepth;

	UPROPERTY()
	FConcertSerializedObjectData ObjectData;

	UPROPERTY()
	TArray<FConcertSerializedPropertyData> PropertyDatas;

	UPROPERTY()
	TArray<uint8> SerializedAnnotationData;
};

USTRUCT()
struct FConcertTransactionEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid TransactionId;

	UPROPERTY()
	FGuid OperationId;

	UPROPERTY()
	FGuid TransactionEndpointId;

	UPROPERTY()
	uint8 TransactionUpdateIndex;

	UPROPERTY()
	TArray<FName> ModifiedPackages;

	UPROPERTY()
	FConcertObjectId PrimaryObjectId;

	UPROPERTY()
	TArray<FConcertExportedObject> ExportedObjects;
};

USTRUCT()
struct FConcertTransactionFinalizedEvent : public FConcertTransactionEventBase
{
	GENERATED_BODY()

	UPROPERTY()
	FConcertLocalIdentifierState LocalIdentifierState;

	UPROPERTY()
	FText Title;
};

USTRUCT()
struct FConcertTransactionSnapshotEvent : public FConcertTransactionEventBase
{
	GENERATED_BODY()
};

USTRUCT()
struct FConcertTransactionRejectedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid TransactionId;
};