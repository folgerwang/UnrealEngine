// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessages.h"
#include "CoreMinimal.h"
#include "ConcertMessageData.h"

#include "ConcertActivityEvents.generated.h"


/** This struct has for purpose to notify the client activity ledger that the initial sync was done */
USTRUCT()
struct CONCERTSYNCCORE_API FConcertActivitiesSyncedEvent
{
	GENERATED_BODY()
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertActivityEvent();

	/**
	 * Get the display text of a activity.
	 */
	virtual FText ToDisplayText(bool bRichText = false) const;

	/**
	 * Get a full detailed text of activity
	 */
	virtual FText ToLongDisplayText() const;

	UPROPERTY()
	FDateTime TimeStamp;

	UPROPERTY()
	FConcertClientInfo ClientInfo;

protected:
	FText GetClientDisplayName() const;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertConnectionActivityEvent : public FConcertActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertConnectionActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertDisconnectionActivityEvent : public FConcertActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertDisconnectionActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertTransactionActivityEvent : public FConcertActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertTransactionActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
	virtual FText ToLongDisplayText() const override;
	

	UPROPERTY()
	FText TransactionTitle;

	UPROPERTY()
	uint64 TransactionIndex;

	UPROPERTY()
	FName ObjectName;

	UPROPERTY()
	FName PackageName;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertTransactionRenameActivityEvent : public FConcertTransactionActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertTransactionRenameActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;

	UPROPERTY()
	FName NewObjectName;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertTransactionDeleteActivityEvent : public FConcertTransactionActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertTransactionDeleteActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertTransactionCreateActivityEvent : public FConcertTransactionActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertTransactionCreateActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
};

// Can also be viewed as package saved
USTRUCT()
struct CONCERTSYNCCORE_API FConcertPackageUpdatedActivityEvent : public FConcertActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertPackageUpdatedActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
	virtual FText ToLongDisplayText() const override;

	UPROPERTY()
	FName PackageName;

	UPROPERTY()
	uint32 Revision;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertPackageAddedActivityEvent : public FConcertPackageUpdatedActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertPackageAddedActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertPackageDeletedActivityEvent : public FConcertPackageUpdatedActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertPackageDeletedActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
};

USTRUCT()
struct CONCERTSYNCCORE_API FConcertPackageRenamedActivityEvent : public FConcertPackageUpdatedActivityEvent
{
	GENERATED_BODY()

	virtual ~FConcertPackageRenamedActivityEvent();

	virtual FText ToDisplayText(bool bRichText = false) const override;
	
	UPROPERTY()
	FName NewPackageName;
};
