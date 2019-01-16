// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertActivityLedger.h"
#include "CoreMinimal.h"

class FConcertServerSyncCommandQueue;
class IConcertServerSession;

struct FConcertClientInfo;

class FConcertServerActivityLedger : public FConcertActivityLedger
{
public:
	FConcertServerActivityLedger(TSharedRef<IConcertServerSession> Session, TSharedRef<FConcertServerSyncCommandQueue> SyncCommandQueue);

	virtual ~FConcertServerActivityLedger();

	void DoInitialSync(const FGuid& InClientEndpoint);

	virtual void RecordPackageUpdate(const uint32 Revision, const FConcertPackageInfo& InPackageInfo, const FConcertClientInfo& InClientInfo) override;

private:

	void SyncActivity(const TArray<FGuid>& ClientEndpoints, const uint64 ActivityIndex);

	virtual void AddActivityCallback(UScriptStruct* ActivityType, const void* ActivityData) override;

	/** */
	TSharedRef<IConcertServerSession> Session;

	/** */
	TSharedRef<FConcertServerSyncCommandQueue> SyncCommandQueue;
};
