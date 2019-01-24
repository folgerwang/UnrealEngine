// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertMessages.h"
#include "ConcertMessageData.h"

class IConcertClientSession;

/**
 * Tracks which client authored which package using the set of transactions that
 * haven't been saved to disk yet, also known as 'Live Transactions'. The purpose
 * of this class is to answer the question "has anybody, other than this client, modified
 * a package?". This is useful when multiple clients are concurrently editing the
 * same package in the same session. When someone is about to save, it might be
 * important to know if somebody else has modified the package and if so, review the
 * other people changes. This feature is integrated in the editor to visually mark
 * the assets modified by other clients.
 *
 * @par User authentication
 * The UE Editor doesn't use a mechanism like login/password to authenticate
 * a users. To uniquely identify a user, Concert generates a unique GUID
 * for each UE Editor instance. The same person may open/close the editor
 * several times or run multiple instances in parallel. For each editor instance,
 * he will get a new unique GUID. When the same user runs the editor in
 * parallel, the user will be recognized as two different people. When a user
 * exit the editor (or crash), then rejoin a session from a new editor instance,
 * the implementation will try to match its new identity to its previous one
 * and then assign all live transactions performed using the previous identity
 * to the new identity, if the user name, display name, machine name, ... match.
 *
 * @par Thread-safety
 * This class is currently called form the UI and Concert network layer, both
 * running in the game thread. For this reason, the class doesn't implement
 * internal synchronization.
 *
 * @par Design considerations
 * The transaction ledger doesn't track the users performing the transactions.
 * The functionality provided by this class could arguably be moved in the
 * transaction ledger, but this class could easily be implemented client
 * side only using the information already available in the transaction and
 * activity ledger.
 *
 * @note
 * For completeness, the functions below could be implemented, but they were not because they were not required for the actual use case.
 *     - bool IsPackageAuthoredByThisClient(const FName& PackageName) const;
 *     - const FConcertClientInfo& GetThisClientInfo() const;
 *     - TArray<FName> GetPackagesAuthoredBy(const FConcertClientInfo& ClientInfo) const;
 *     - TArray<FName> GetAuthoredPackages() const;
 *     - const FConcertClientInfo& GetLastPackageAuthor(const FName& PackageName) const;
 */
class FConcertClientLiveTransactionAuthors
{
public:
	/** Constructor.
	 * @param Session This local client session, used to identify this client against other clients connected to the session.
	 */
	FConcertClientLiveTransactionAuthors(TSharedRef<IConcertClientSession> Session);

	/** Destructor. */
	~FConcertClientLiveTransactionAuthors();

	/**
	 * Adds a live transaction on the specified package from the specified client. Invoked when an asset is edited.
	 * @param PackageName The package affected by the transaction.
	 * @param TransactionAuthor The author of the transaction.
	 * @param InTransactionIndex The index of the transaction.
	 * @see FConcertTransactionLedger::OnAddFinalizedTransaction
	 * @see FConcertTransactionLedger::GetAllLiveTransactions
	 */
	void AddLiveTransaction(const FName& PackageName, const FConcertClientInfo& TransactionAuthor, uint64 InTransactionIndex);

	/**
	 * Adds a live transaction on the specified packages from the specified client. Invoked when an asset is edited.
	 * @param PackageNames The list of packages affected by the transaction.
	 * @param TransactionAuthor The author of the transaction.
	 * @param InTransactionIndex The index of the transaction.
	 * @see FConcertTransactionLedger::OnAddFinalizedTransaction
	 * @see FConcertTransactionLedger::GetAllLiveTransactions
	 */
	void AddLiveTransaction(const TArray<FName>& PackageNames, const FConcertClientInfo& TransactionAuthor, uint64 InTransactionIndex);

	/**
	 * Trims transactions on the specified package up to the specified index. Invoked when a package is saved.
	 * @param PackageName The package for which the transaction were trimmed.
	 * @param UpToIndex The end index (exclusive) indicating that all previous transactions on the package were trimmed.
	 * @see FConcertTransactionLedger::OnLiveTransactionsTrimmed
	 */
	void TrimLiveTransactions(const FName& PackageName, uint64 UpToIndex);

	/**
	 * Returns true if the specified packages has live transaction(s) from any other client(s) than the one corresponding
	 * to the client session passed at construction and possibly returns information about the other clients.
	 * @param[in] PackageName The package name.
	 * @param[out] OutOtherClientsWithModifNum If not null, will contain how many other client(s) have modified the specified package.
	 * @param[out] OutOtherClientsWithModifInfo If not null, will contain the other client who modified the packages, up to OtherClientsWithModifMaxFetchNum.
	 * @param[in] OtherClientsWithModifMaxFetchNum The maximum number of client info to store in OutOtherClientsWithModifInfo if the latter is not null.
	 */
	bool IsPackageAuthoredByOtherClients(const FName& PackageName, int* OutOtherClientsWithModifNum = nullptr, TArray<FConcertClientInfo>* OutOtherClientsWithModifInfo = nullptr, int OtherClientsWithModifMaxFetchNum = 0) const;

private:
	/** Alias for FGuid to make the code more explicit about what the Guid is for. */
	typedef FGuid FClientInstanceGuid;

	/** Keep the last transaction index made by a client. */
	struct FTransactionInfo
	{
		/** The last live transaction index recorded for the author that hasn't yet been trimmed. */
		uint64 LastTransactionIndex;

		/** The client who performed the transaction(s). */
		FConcertClientInfo AuthorInfo;
	};

	/** Maps package names to the list of clients (other than this client) that have live transactions on a package. */
	TMap<FName, TMap<FClientInstanceGuid, FTransactionInfo>> OtherClientsLiveTransactionInfo;

	/** The client session. */
	TSharedRef<IConcertClientSession> Session;
};

class FConcertTransactionLedger;
class FConcertActivityLedger;

/**
 * Gets all live transactions from the transaction ledger and try to find the author of each live transaction by inspecting the
 * activity ledger.
 * @param[in] TransactionLedger The transaction ledger for the session.
 * @param[in] ActivityLedger The activity ledger for the session.
 * @param[out] OutTransactionAuthors The object tracking the transaction authors. The object is expected to be freshly constructed.
 */
void ResolveLiveTransactionAuthors(const FConcertTransactionLedger& TransactionLedger, const FConcertActivityLedger& ActivityLedger, FConcertClientLiveTransactionAuthors& OutTransactionAuthors);

