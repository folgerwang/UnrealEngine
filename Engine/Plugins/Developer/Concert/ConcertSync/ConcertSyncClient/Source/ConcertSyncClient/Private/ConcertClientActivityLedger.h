// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertActivityLedger.h"
#include "ConcertActivityEvents.h"

class IConcertClientSession;
struct FConcertSessionContext;
struct FConcertClientInfo;

class FConcertClientActivityLedger : public FConcertActivityLedger
{
public:
	FConcertClientActivityLedger(TSharedRef<IConcertClientSession> Session);

	virtual ~FConcertClientActivityLedger();

	virtual void RecordClientConectionStatusChanged(EConcertClientStatus ClientStatus, const FConcertClientInfo& InClientInfo) override;
	virtual void RecordFinalizedTransaction(const FConcertTransactionFinalizedEvent& InTransactionFinalizedEvent, uint64 TransactionIndex, const FConcertClientInfo& InClientInfo) override;
	virtual void RecordPackageUpdate(const uint32 Revision, const FConcertPackageInfo& InPackageInfo, const FConcertClientInfo& InClientInfo) override;

private:
	template<typename ActivityType>
	void HandleActivityReceived(const FConcertSessionContext& Context, const ActivityType& Activity)
	{
		static_assert(TIsDerivedFrom<ActivityType, FConcertActivityEvent>::IsDerived, "An activity must always derive from FConcertActivityEvent.");
		AddActivity(Activity);
	}

	void HandleActivitiesSynced(const FConcertSessionContext& Context, const FConcertActivitiesSyncedEvent& Event);

	void HandleSessionClientChanged(IConcertClientSession& InSession, EConcertClientStatus ClientStatus, const FConcertSessionClientInfo& InSessionClientInfo);

	virtual void AddActivityCallback(UScriptStruct* InActivityType, const void* ActivityData) override;

	TSharedRef<IConcertClientSession> Session;

	bool bIsSynced;
};
