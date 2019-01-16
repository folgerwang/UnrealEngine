// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertServerActivityLedger.h"

#include "ConcertServerSyncCommandQueue.h"
#include "ConcertActivityEvents.h"
#include "IConcertSession.h"
#include "ConcertLogGlobal.h"
#include "UObject/StructOnScope.h"
#include "ConcertMessageData.h"

FConcertServerActivityLedger::FConcertServerActivityLedger(TSharedRef<IConcertServerSession> InSession, TSharedRef<FConcertServerSyncCommandQueue> SyncCommandQueue)
	: FConcertActivityLedger(EConcertActivityLedgerType::Persistent, InSession->GetSessionWorkingDirectory())
	, Session(MoveTemp(InSession))
	, SyncCommandQueue(MoveTemp(SyncCommandQueue))
{
}

FConcertServerActivityLedger::~FConcertServerActivityLedger()
{
}

void FConcertServerActivityLedger::DoInitialSync(const FGuid& InClientEndPoint)
{
	for (uint64 Index = 0; Index < GetActivityCount(); Index++)
	{
		SyncActivity(TArray<FGuid>{InClientEndPoint}, Index);
	}

	SyncCommandQueue->QueueCommand(InClientEndPoint,
		[this](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
		{
			Session->SendCustomEvent(FConcertActivitiesSyncedEvent(), InEndpointId, EConcertMessageFlags::ReliableOrdered);
		});
}

void FConcertServerActivityLedger::RecordPackageUpdate(const uint32 Revision, const FConcertPackageInfo& InPackageInfo, const FConcertClientInfo& InClientInfo)
{
	FConcertActivityLedger::RecordPackageUpdate(Revision, InPackageInfo, InClientInfo);

	//Currently the client can't construct its feed for the updated package events so we sync it here
	SyncActivity(Session->GetSessionClientEndpointIds(), GetActivityCount() - 1);
}

void FConcertServerActivityLedger::SyncActivity(const TArray<FGuid>& ClientEndpoints, const uint64 ActivityIndex)
{
	SyncCommandQueue->QueueCommand(ClientEndpoints, [this, ActivityIndex](const FConcertServerSyncCommandQueue::FSyncCommandContext& InSyncCommandContext, const FGuid& InEndpointId)
	{
		FStructOnScope Activity;
		if (FindActivity(ActivityIndex, Activity))
		{
			const UScriptStruct* ActivityType = CastChecked<const UScriptStruct>(Activity.GetStruct());

			if (ActivityType->IsChildOf(FConcertTransactionCreateActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertTransactionCreateActivityEvent*)Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertTransactionDeleteActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertTransactionDeleteActivityEvent*)Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertTransactionRenameActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertTransactionRenameActivityEvent*)Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertTransactionActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertTransactionActivityEvent*)Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertPackageAddedActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertPackageAddedActivityEvent*) Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertPackageDeletedActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertPackageDeletedActivityEvent*) Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertPackageRenamedActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertPackageRenamedActivityEvent*) Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertPackageUpdatedActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertPackageUpdatedActivityEvent*) Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertConnectionActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertConnectionActivityEvent*) Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
			else if (ActivityType->IsChildOf(FConcertDisconnectionActivityEvent::StaticStruct()))
			{
				Session->SendCustomEvent(*((FConcertDisconnectionActivityEvent*)Activity.GetStructMemory()), InEndpointId, EConcertMessageFlags::ReliableOrdered);
			}
		}
	});
}

void FConcertServerActivityLedger::AddActivityCallback(UScriptStruct* ActivityType, const void* ActivityData)
{
	UE_LOG(LogConcert, Display, TEXT("Session %s activity: %s"), *Session->GetName(), *((FConcertActivityEvent*)ActivityData)->ToLongDisplayText().ToString());
}
