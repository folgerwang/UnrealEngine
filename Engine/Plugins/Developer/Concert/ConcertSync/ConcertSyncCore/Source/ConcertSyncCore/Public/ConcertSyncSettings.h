// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "ConcertSyncSettings.generated.h"

USTRUCT()
struct CONCERTSYNCCORE_API FTransactionClassFilter
{
	GENERATED_BODY()

	/**
	 * Object class to filter transaction object on.
	 */
	UPROPERTY(config, EditAnywhere, Category="Sync Settings")
	FSoftClassPath ObjectClass;

	/**
	 *	Optional Outer Class that will allow object only if their one of their outer match this class.
	 */
	UPROPERTY(config, EditAnywhere, Category="Sync Settings")
	FSoftClassPath ObjectOuterClass;
};

UCLASS(config=Engine)
class CONCERTSYNCCORE_API UConcertSyncConfig : public UObject
{
	GENERATED_BODY()

public:
	UConcertSyncConfig()
		: bInteractiveHotReload(false)
		, bShowPresenceInPIE(true)
		, SnapshotTransactionsPerSecond(30.0f)
	{}

	/**
	 * Should we ask before hot-reloading changed packages?
	 * If disabled we will clobber any local changes when reloading packages.
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="Transaction Settings")
	bool bInteractiveHotReload;

	/**
	 * Should we show presence when in PIE?
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="Transaction Settings")
	bool bShowPresenceInPIE;

	/**
	 * Number of snapshot transactions (eg, moving an object or dragging a slider) that should be sent per-second to other clients.
	 */
	UPROPERTY(config, EditAnywhere, Category="Transaction Settings", meta=(ClampMin="1", UIMin="1"))
	float SnapshotTransactionsPerSecond;

	/**
	 * Array of Transaction class filter.
	 * Only objects that pass those filters will be included in transaction updates.
	 * @note If this is empty, then all class types will send transaction updates.
	 */
	UPROPERTY(config, EditAnywhere, Category="Transaction Settings")
	TArray<FTransactionClassFilter> IncludeObjectClassFilters;

	/**
	 * Array of additional Transaction class filter.
	 * Objects that matches those filters, will prevent the whole transaction from propagation.
	 * @note These filters takes precedence over the IncludeObjectClassFilters
	 */
	UPROPERTY(config, EditAnywhere, Category = "Transaction Settings")
	TArray<FTransactionClassFilter> ExcludeTransactionClassFilters;

	/**
	 * Array of transient class properties that we should send transaction updates for even if usually filtered out.
	 */
	UPROPERTY(config, EditAnywhere, Category="Transaction Settings", meta=(AllowedClasses="Property"))
	TArray<FSoftObjectPath> AllowedTransientProperties;
};
