// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientLiveTransactionAuthors.h"
#include "ConcertActivityLedger.h"
#include "ConcertTransactionLedger.h"
#include "ConcertActivityEvents.h"
#include "IConcertSession.h"

//------------------------------------------------------------------------------
// FConcertClientPackageModifiedByTracker implementation.
//------------------------------------------------------------------------------

FConcertClientLiveTransactionAuthors::FConcertClientLiveTransactionAuthors(TSharedRef<IConcertClientSession> InSession)
	: Session(MoveTemp(InSession))
{
}

FConcertClientLiveTransactionAuthors::~FConcertClientLiveTransactionAuthors()
{
}

void FConcertClientLiveTransactionAuthors::AddLiveTransaction(const TArray<FName>& PackageNames, const FConcertClientInfo& TransactionAuthors, uint64 InTransactionIndex)
{
	for (const FName& PackageName : PackageNames)
	{
		AddLiveTransaction(PackageName, TransactionAuthors, InTransactionIndex);
	}
}

void FConcertClientLiveTransactionAuthors::AddLiveTransaction(const FName& PackageName, const FConcertClientInfo& TransactionAuthors, uint64 LastTransactionIndex)
{
	const FConcertClientInfo& ThisClient = Session->GetLocalClientInfo();

	// Don't track the modification performed by this client. We are only interested to know who else modified a package to flag the UI with a "modified by other" icon.
	if (TransactionAuthors.InstanceInfo.InstanceId == ThisClient.InstanceInfo.InstanceId)
	{
		return;
	}

	// Find or add the package entry.
	TMap<FClientInstanceGuid, FTransactionInfo>& TransactionInfoMap = OtherClientsLiveTransactionInfo.FindOrAdd(PackageName);

	// If this client has already live transaction(s) on the package.
	if (FTransactionInfo* TransactionInfo = TransactionInfoMap.Find(TransactionAuthors.InstanceInfo.InstanceId))
	{
		// Update the transaction index to the last value.
		check(TransactionInfo->LastTransactionIndex < LastTransactionIndex);
		TransactionInfo->LastTransactionIndex = LastTransactionIndex;
	}
	// If the client who made the transaction is connected (it cannot be this client, this was tested at function first line)
	else if (Session->GetSessionClients().FindByPredicate([&TransactionAuthors](const FConcertSessionClientInfo& Other) { return TransactionAuthors.InstanceInfo.InstanceId == Other.ClientInfo.InstanceInfo.InstanceId; }))
	{
		TransactionInfoMap.Add(TransactionAuthors.InstanceInfo.InstanceId, FTransactionInfo{LastTransactionIndex, TransactionAuthors});
	}
	// If the "disconnected" client doesn't match this client identity. (It is not this client instance nor any other connected clients, so we deduce it is a disconnected one that did not save upon existing and for which we got the identity from the activity ledger)
	else if (TransactionAuthors.UserName != ThisClient.UserName || TransactionAuthors.DeviceName != ThisClient.DeviceName || TransactionAuthors.PlatformName != ThisClient.PlatformName || TransactionAuthors.DisplayName != ThisClient.DisplayName)
	{
		// It looks like the client who performed the transaction was not a previous instance of this client. (Like a client rejoining after a crash)
		TransactionInfoMap.Add(TransactionAuthors.InstanceInfo.InstanceId, FTransactionInfo{LastTransactionIndex, TransactionAuthors});
	}
	// else -> It seems like the transaction was performed by a previous instance of this client. We are only interested to track who else than us modified a package.
}

void FConcertClientLiveTransactionAuthors::TrimLiveTransactions(const FName& PackageName, uint64 UpToIndex)
{
	// Find the package.
	if (TMap<FClientInstanceGuid, FTransactionInfo>* TransactionInfoMap = OtherClientsLiveTransactionInfo.Find(PackageName))
	{
		// Visit all clients that have live transaction on the package.
		for (auto ClientGuidTransactionInfoIter = TransactionInfoMap->CreateIterator(); ClientGuidTransactionInfoIter; ++ClientGuidTransactionInfoIter)
		{
			// If all live transaction from the visited client/package were trimmed (save to disk).
			if (ClientGuidTransactionInfoIter->Value.LastTransactionIndex < UpToIndex)
			{
				// That client doesn't have any remaining live transaction on that package, remove the client.
				ClientGuidTransactionInfoIter.RemoveCurrent();
			}
		}

		// If all live transactions for all client has been trimmed (saved to disk), stop tracking the package.
		if (TransactionInfoMap->Num() == 0)
		{
			OtherClientsLiveTransactionInfo.Remove(PackageName);
		}
	}
}

bool FConcertClientLiveTransactionAuthors::IsPackageAuthoredByOtherClients(const FName& PackageName, int* OutOtherClientsWithModifNum, TArray<FConcertClientInfo>* OutOtherClientsWithModifInfo, int OtherClientsWithModifMaxFetchNum) const
{
	int OtherClientWithModifNum = 0;
	int ClientInfoInArray = 0;

	if (const TMap<FClientInstanceGuid, FTransactionInfo>* TransactionInfoMap = OtherClientsLiveTransactionInfo.Find(PackageName))
	{
		OtherClientWithModifNum = TransactionInfoMap->Num();

		// The caller wants to know which other client(s) modified the specified package.
		if (OutOtherClientsWithModifInfo && OtherClientsWithModifMaxFetchNum > 0 && OtherClientWithModifNum > 0)
		{
			for (const TPair<FClientInstanceGuid, FTransactionInfo>& ClientGuidTransactionInfoPair : *TransactionInfoMap)
			{
				OutOtherClientsWithModifInfo->Emplace(ClientGuidTransactionInfoPair.Value.AuthorInfo);
				if (--OtherClientsWithModifMaxFetchNum == 0)
					break;
			}
		}
	}

	// The caller wants to know how many other client(s) modified the specified package.
	if (OutOtherClientsWithModifNum)
	{
		*OutOtherClientsWithModifNum = OtherClientWithModifNum;
	}

	// Returns if the specified package was modified by other clients.
	return OtherClientWithModifNum > 0;
}


//------------------------------------------------------------------------------
// Free functions.
//------------------------------------------------------------------------------

void ResolveLiveTransactionAuthors(const FConcertTransactionLedger& TransactionLedger, const FConcertActivityLedger& ActivityLedger, FConcertClientLiveTransactionAuthors& LiveTransactionAuthors)
{
	// Get all live transactions for which we must find the owner, i.e. the client who made the transaction. (The transaction ledger doesn't track the user information with the transactions).
	TArray<uint64> UnresolvedLiveTransactions = TransactionLedger.GetAllLiveTransactions();

	// Read the activity feed, which has the transaction index/client instance ID info, until all live transactions are matched to
	// a user instance id or until we reached the end of the activity feed.
	int64 ActivityIndex = ActivityLedger.GetActivityCount();
	while (UnresolvedLiveTransactions.Num() > 0 && --ActivityIndex >= 0)
	{
		// Find the activity corresponding the to activity index.
		FStructOnScope ActivityEvent;
		ActivityLedger.FindActivity(ActivityIndex, ActivityEvent);

		// If the recorded activity was a transaction activity.
		if (ActivityEvent.GetStruct()->IsChildOf(FConcertTransactionActivityEvent::StaticStruct()))
		{
			// If the activity correspond to a live transaction, resolve it, removing it from the list of unresolved live transactions.
			const FConcertTransactionDeleteActivityEvent* Event = reinterpret_cast<const FConcertTransactionDeleteActivityEvent*>(ActivityEvent.GetStructMemory());
			if (UnresolvedLiveTransactions.Remove(Event->TransactionIndex) == 1)
			{
				// The activity has the instance Id of the client who performed it.
				LiveTransactionAuthors.AddLiveTransaction(Event->PackageName, Event->ClientInfo, Event->TransactionIndex);
			}
		}
	}

	// We should have found a client instance ID for each live transaction by inspecting the activity feed. If not, maybe the activitity feed was truncated?
	check(UnresolvedLiveTransactions.Num() == 0);
}

