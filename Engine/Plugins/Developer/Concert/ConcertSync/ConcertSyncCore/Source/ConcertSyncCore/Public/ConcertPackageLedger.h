// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FConcertFileCache;

struct FConcertPackage;
struct FConcertPackageInfo;

enum class EConcertPackageLedgerType : uint8
{
	/** This is a persistent ledger (eg, belonging to a server session) */
	Persistent,
	/** This is a transient ledger (eg, belonging to a client session) */
	Transient,
};

/**
 * In-memory index of a package ledger, which references on-disk files that contain the bulk of the package data.
 */
class CONCERTSYNCCORE_API FConcertPackageLedger
{
public:
	/**
	* Create a new ledger
	* @note The ledger path must not be empty.
	*/
	FConcertPackageLedger(const EConcertPackageLedgerType InLedgerType, const FString& InLedgerPath);

	~FConcertPackageLedger();

	/**
	 * Non-copyable
	 */
	FConcertPackageLedger(const FConcertPackageLedger&) = delete;
	FConcertPackageLedger& operator=(const FConcertPackageLedger&) = delete;

	/**
	 * Get the path to this ledger on-disk.
	 */
	const FString& GetLedgerPath() const;

	/**
	 * Get the file extension of ledger entries on-disk.
	 */
	const FString& GetLedgerEntryExtension() const;

	/**
	 * Load this ledger from the existing content on-disk.
	 * @return true if a package was loaded
	 */
	bool LoadLedger();

	/**
	 * Clear this ledger, removing any content on-disk.
	 * @note Happens automatically when destroying a transient ledger.
	 */
	void ClearLedger();

	/**
	 * Add a new revision of a package to this ledger.
	 * @return The revision of the added package.
	 */
	uint32 AddPackage(const FConcertPackage& InPackage);

	/**
	 * Add a new revision of a package to this ledger.
	 * @return The revision of the added package.
	 */
	uint32 AddPackage(const FConcertPackageInfo& InPackageInfo, const TArray<uint8>& InPackageData);

	/**
	 * Add a package to this ledger at the given revision.
	 * @note Will clobber any existing package at that revision!
	 */
	void AddPackage(const uint32 InRevision, const FConcertPackage& InPackage);

	/**
	 * Add a package to this ledger at the given revision.
	 * @note Will clobber any existing package at that revision!
	 */
	void AddPackage(const uint32 InRevision, const FConcertPackageInfo& InPackageInfo, const TArray<uint8>& InPackageData);

	/**
	 * Find the given package from this ledger, optionally at the given revision, otherwise at the head revision.
	 * @return True if the package was found, false otherwise.
	 */
	bool FindPackage(const FName InPackageName, FConcertPackage& OutPackage, const uint32* InRevision = nullptr) const;

	/**
	 * Find the given package from this ledger, optionally at the given revision, otherwise at the head revision.
	 * @note This version allows you to retrieve either just the info, or just the data (or both!), by passing null to the argument you don't want.
	 * @return True if the package was found, false otherwise.
	 */
	bool FindPackage(const FName InPackageName, FConcertPackageInfo* OutPackageInfo, TArray<uint8>* OutPackageData, const uint32* InRevision = nullptr) const;

	/**
	 * Get the name of every package tracked by this ledger.
	 * @return An array of package names.
	 */
	TArray<FName> GetAllPackageNames() const;

	/**
	 * Get the head revision of the given package.
	 * @return True if the package was found, false otherwise.
	 */
	bool GetPackageHeadRevision(const FName InPackageName, uint32& OutRevision) const;

private:
	friend class FConcertPackageVisitor;

	/** The type of this ledger */
	EConcertPackageLedgerType LedgerType;

	/** Path to this ledger on-disk */
	FString LedgerPath;

	/** Mapping from a package name to its head revision */
	TMap<FName, uint32> PackageHeadRevisions;

	/** In-memory cache of on-disk ledger entries */
	TSharedRef<FConcertFileCache> LedgerFileCache;
};
