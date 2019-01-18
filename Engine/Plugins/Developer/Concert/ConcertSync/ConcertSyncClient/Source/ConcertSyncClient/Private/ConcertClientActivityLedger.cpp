// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientActivityLedger.h"

#include "ConcertLogGlobal.h"
#include "ConcertWorkspaceMessages.h"
#include "IConcertSession.h"
#include "IConcertSessionHandler.h"
#include "ConcertMessageData.h"

FConcertClientActivityLedger::FConcertClientActivityLedger(TSharedRef<IConcertClientSession> InSession)
	: FConcertActivityLedger(EConcertActivityLedgerType::Transient, InSession->GetSessionWorkingDirectory())
	, Session(MoveTemp(InSession))
	, bIsSynced(false)
{
	Session->RegisterCustomEventHandler<FConcertActivitiesSyncedEvent>(this, &FConcertClientActivityLedger::HandleActivitiesSynced);

	Session->RegisterCustomEventHandler<FConcertTransactionActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertTransactionCreateActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertTransactionDeleteActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertTransactionRenameActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);

	Session->RegisterCustomEventHandler<FConcertConnectionActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertDisconnectionActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);

	Session->RegisterCustomEventHandler<FConcertPackageUpdatedActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertPackageAddedActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertPackageDeletedActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);
	Session->RegisterCustomEventHandler<FConcertPackageRenamedActivityEvent>(this, &FConcertClientActivityLedger::HandleActivityReceived);

	Session->OnSessionClientChanged().AddRaw(this, &FConcertClientActivityLedger::HandleSessionClientChanged);
}

FConcertClientActivityLedger::~FConcertClientActivityLedger()
{
	Session->OnSessionClientChanged().RemoveAll(this);

	Session->UnregisterCustomEventHandler<FConcertPackageRenamedActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertPackageUpdatedActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertPackageAddedActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertPackageDeletedActivityEvent>();

	Session->UnregisterCustomEventHandler<FConcertDisconnectionActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertConnectionActivityEvent>();

	Session->UnregisterCustomEventHandler<FConcertTransactionRenameActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertTransactionDeleteActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertTransactionCreateActivityEvent>();
	Session->UnregisterCustomEventHandler<FConcertTransactionActivityEvent>();

	Session->UnregisterCustomEventHandler<FConcertActivitiesSyncedEvent>();
}

void FConcertClientActivityLedger::RecordClientConectionStatusChanged(EConcertClientStatus ClientStatus, const FConcertClientInfo& InClientInfo)
{
	if (bIsSynced)
	{
		FConcertActivityLedger::RecordClientConectionStatusChanged(ClientStatus, InClientInfo);
	}
}

void FConcertClientActivityLedger::RecordFinalizedTransaction(const FConcertTransactionFinalizedEvent& InTransactionFinalizedEvent, uint64 TransactionIndex, const FConcertClientInfo& InClientInfo)
{
	if (bIsSynced)
	{
		FConcertActivityLedger::RecordFinalizedTransaction(InTransactionFinalizedEvent, TransactionIndex, InClientInfo);
	}
}

void FConcertClientActivityLedger::RecordPackageUpdate(const uint32 Revision, const FConcertPackageInfo& InPackageInfo, const FConcertClientInfo& InClientInfo)
{
	if (bIsSynced)
	{
		FConcertActivityLedger::RecordPackageUpdate(Revision, InPackageInfo, InClientInfo);
	}
}

void FConcertClientActivityLedger::HandleActivitiesSynced(const FConcertSessionContext& Context, const FConcertActivitiesSyncedEvent& Event)
{
	bIsSynced = true;
}

void FConcertClientActivityLedger::HandleSessionClientChanged(IConcertClientSession& InSession, EConcertClientStatus ClientStatus, const FConcertSessionClientInfo& InSessionClientInfo)
{
	RecordClientConectionStatusChanged(ClientStatus, InSessionClientInfo.ClientInfo);
}

void FConcertClientActivityLedger::AddActivityCallback(UScriptStruct* InActivityType, const void* ActivityData)
{
	//Debug before doing a proper ui
	UE_LOG(LogConcert, Display, TEXT("Activity Feed: %s"), *((FConcertActivityEvent*)ActivityData)->ToLongDisplayText().ToString());
}

