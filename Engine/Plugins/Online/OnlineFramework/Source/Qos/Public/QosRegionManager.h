// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "QosRegionManager.generated.h"

class IAnalyticsProvider;
class UQosEvaluator;

#define UNREACHABLE_PING 9999

/** Enum for single region QoS return codes */
UENUM()
enum class EQosDatacenterResult : uint8
{
	/** Incomplete, invalid result */
	Invalid,
	/** QoS operation was successful */
	Success,
	/** QoS operation with one or more ping failures */
	Incomplete
};

/** Enum for possible QoS return codes */
UENUM()
enum class EQosCompletionResult : uint8
{
	/** Incomplete, invalid result */
	Invalid,
	/** QoS operation was successful */
	Success,
	/** QoS operation ended in failure */
	Failure,
	/** QoS operation was canceled */
	Canceled
};

/**
 * Individual ping server details
 */
USTRUCT()
struct FQosPingServerInfo
{
	GENERATED_USTRUCT_BODY()

	/** Address of server */
	UPROPERTY()
	FString Address;
	/** Port of server */
	UPROPERTY()
	int32 Port;
};

/**
 * Metadata about datacenters that can be queried
 */
USTRUCT()
struct FQosDatacenterInfo
{
	GENERATED_USTRUCT_BODY()

	/** Id for this datacenter */
	UPROPERTY()
	FString Id;
	/** Parent Region */
	UPROPERTY()
	FString RegionId;
	/** Is this region tested (only valid if region is enabled) */
	UPROPERTY()
	bool bEnabled;
	/** Addresses of ping servers */
	UPROPERTY()
	TArray<FQosPingServerInfo> Servers;

	FQosDatacenterInfo()
		: bEnabled(true)
	{
	}

	bool IsValid() const
	{
		return !Id.IsEmpty() && !RegionId.IsEmpty();
	}

	bool IsPingable() const
	{
		return bEnabled && IsValid();
	}

	FString ToDebugString() const
	{
		return FString::Printf(TEXT("[%s][%s]"), *RegionId, *Id);
	}
};

/**
 * Metadata about regions made up of datacenters
 */
USTRUCT()
struct FQosRegionInfo
{
	GENERATED_USTRUCT_BODY()

	/** Localized name of the region */
	UPROPERTY()
	FText DisplayName;
	/** Id for the region, all datacenters must reference one of these */
	UPROPERTY()
	FString RegionId;
	/** Is this region tested at all (if false, overrides individual datacenters) */
	UPROPERTY()
	bool bEnabled;
	/** Is this region visible in the UI (can be saved by user, replaced with auto if region disappears */
	UPROPERTY()
	bool bVisible;
	/** Can this region be considered for auto detection */
	UPROPERTY()
	bool bAutoAssignable;

	FQosRegionInfo()
		: bEnabled(true)
		, bVisible(true)
		, bAutoAssignable(true)
	{
	}

	bool IsValid() const
	{
		return !RegionId.IsEmpty();
	}

	/** @return true if this region is supposed to be tested */
	bool IsPingable() const
	{
		return bEnabled;
	}

	/** @return true if a user can select this region in game */
	bool IsUsable() const
	{
		return bVisible && IsPingable();
	}

	/** @return true if this region can be auto assigned */
	bool IsAutoAssignable() const
	{
		return bAutoAssignable && IsUsable();
	}
};

/** Runtime information about a given region */
USTRUCT()
struct QOS_API FDatacenterQosInstance
{
	GENERATED_USTRUCT_BODY()

	/** Information about the datacenter */
	UPROPERTY(Transient)
	FQosDatacenterInfo Definition;
	/** Success of the qos evaluation */
	UPROPERTY(Transient)
	EQosDatacenterResult Result;
	/** Avg ping times across all search results */
	UPROPERTY(Transient)
	int32 AvgPingMs;
	/** Transient list of ping times obtained for this datacenter */
	UPROPERTY(Transient)
	TArray<int32> PingResults;
	/** Number of good results */
	int32 NumResponses;
	/** Last time this datacenter was checked */
	UPROPERTY(Transient)
	FDateTime LastCheckTimestamp;
	/** Is the parent region usable */
	UPROPERTY(Transient)
	bool bUsable;

	FDatacenterQosInstance()
		: Result(EQosDatacenterResult::Invalid)
		, AvgPingMs(UNREACHABLE_PING)
		, NumResponses(0)
		, LastCheckTimestamp(0)
		, bUsable(true)
	{
	}

	FDatacenterQosInstance(const FQosDatacenterInfo& InMeta, bool bInUsable)
		: Definition(InMeta)
		, Result(EQosDatacenterResult::Invalid)
		, AvgPingMs(UNREACHABLE_PING)
		, NumResponses(0)
		, LastCheckTimestamp(0)
		, bUsable(bInUsable)
	{
	}

	/** reset the data to its default state */
	void Reset()
	{
		// Only the transient values get reset
		Result = EQosDatacenterResult::Invalid;
		AvgPingMs = UNREACHABLE_PING;
		PingResults.Empty();
		NumResponses = 0;
		LastCheckTimestamp = FDateTime(0);
		bUsable = false;
	}
};

USTRUCT()
struct QOS_API FRegionQosInstance
{
	GENERATED_USTRUCT_BODY()

	/** Information about the region */
	UPROPERTY(Transient)
	FQosRegionInfo Definition;

	/** Array of all known datacenters and their status */
	UPROPERTY()
	TArray<FDatacenterQosInstance> DatacenterOptions;

	FRegionQosInstance()
	{
	}

	FRegionQosInstance(const FQosRegionInfo& InMeta)
		: Definition(InMeta)
	{
	}

	/** @return the region id for this region instance */
	const FString& GetRegionId() const
	{ 
		return Definition.RegionId; 
	}

	/** @return if this region data is usable externally */
	bool IsUsable() const
	{
		return Definition.IsUsable();
	}

	/** @return true if this region can be consider for auto detection */
	bool IsAutoAssignable() const
	{
		bool bValidResults = (GetRegionResult() == EQosDatacenterResult::Success) || (GetRegionResult() == EQosDatacenterResult::Incomplete);
		return Definition.IsAutoAssignable() && IsUsable() && bValidResults;
	}

	/** @return the result of this region ping request */
	EQosDatacenterResult GetRegionResult() const;
	/** @return the ping recorded in the best sub region */
	int32 GetBestAvgPing() const;
	/** @return the subregion with the best ping */
	FString GetBestSubregion() const;
	/** @return sorted list of subregions by best ping */
	void GetSubregionPreferences(TArray<FString>& OutSubregions) const;
};

/**
 * Main Qos interface for actions related to server quality of service
 */
UCLASS(config = Engine)
class QOS_API UQosRegionManager : public UObject
{

	GENERATED_UCLASS_BODY()

public:

	/**
	 * Start running the async QoS evaluation 
	 */
	void BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete);

	/**
	 * Get the region ID for this instance, checking ini and commandline overrides.
	 * 
	 * Dedicated servers will have this value specified on the commandline
	 * 
	 * Clients pull this value from the settings (or command line) and do a ping test to determine if the setting is viable.
	 *
	 * @return the current region identifier
	 */
	FString GetRegionId() const;

	/**
	 * Get the region ID with the current best ping time, checking ini and commandline overrides.
	 * 
	 * @return the default region identifier
	 */
	FString GetBestRegion() const;

	/**
	 * Get the list of regions that the client can choose from (returned from search and must meet min ping requirements)
	 *
	 * If this list is empty, the client cannot play.
	 */
	const TArray<FRegionQosInstance>& GetRegionOptions() const;

	/**
	 * Get a sorted list of subregions within a region
	 *
	 * @param RegionId region of interest
	 * @param OutSubregions list of subregions in sorted order
	 */
	void GetSubregionPreferences(const FString& RegionId, TArray<FString>& OutSubregions) const;

	/**
	 * @return true if this is a usable region, false otherwise
	 */
	bool IsUsableRegion(const FString& InRegionId) const;

	/**
	 * Try to set the selected region ID (must be present in GetRegionOptions)
	 *
	 * @param bForce if true then use selected region even if QoS eval has not completed successfully
	 */
	bool SetSelectedRegion(const FString& RegionId, bool bForce=false);

	/** Clear the region to nothing, used for logging out */
	void ClearSelectedRegion();

	/**
	 * Force the selected region creating a fake RegionOption if necessary
	 */
	void ForceSelectRegion(const FString& RegionId);

	/**
	 * Get the datacenter id for this instance, checking ini and commandline overrides
	 * This is only relevant for dedicated servers (so they can advertise). 
	 * Client does not search on this in any way
	 *
	 * @return the default datacenter identifier
	 */
	static FString GetDatacenterId();

	/**
	 * Get the subregion id for this instance, checking ini and commandline overrides
	 * This is only relevant for dedicated servers (so they can advertise). Client does
	 * not search on this (but may choose to prioritize results later)
	 */
	static FString GetAdvertisedSubregionId();

	/** @return true if a reasonable enough number of results were returned from all known regions, false otherwise */
	bool AllRegionsFound() const;

	/**
	 * Debug output for current region / datacenter information
	 */
	void DumpRegionStats() const;

	void RegisterQoSSettingsChangedDelegate(const FSimpleDelegate& OnQoSSettingsChanged);

public:

	/** Begin UObject interface */
	virtual void PostReloadConfig(class UProperty* PropertyThatWasLoaded) override;
	/** End UObject interface */

private:

	/**
	 * Double check assumptions based on current region/datacenter definitions
	 */
	void SanityCheckDefinitions() const;

	void OnQosEvaluationComplete(EQosCompletionResult Result, const TArray<FDatacenterQosInstance>& DatacenterInstances);

	/**
	 * Use the existing set value, or if it is currently invalid, set the next best region available
	 */
	void TrySetDefaultRegion();

	/**
	 * @return max allowable ping to any region and still be allowed to play
	 */
	int32 GetMaxPingMs() const;

	/** Number of times to ping a given region using random sampling of available servers */
	UPROPERTY(Config)
	int32 NumTestsPerRegion;

	/** Timeout value for each ping request */
	UPROPERTY(Config)
	float PingTimeout;

	/** Metadata about existing regions */
	UPROPERTY(Config)
	TArray<FQosRegionInfo> RegionDefinitions;

	/** Metadata about datacenters within existing regions */
	UPROPERTY(Config)
	TArray<FQosDatacenterInfo> DatacenterDefinitions;

	UPROPERTY()
	FDateTime LastCheckTimestamp;

	/** Reference to the evaluator for making datacenter determinations (null when not active) */
	UPROPERTY()
	UQosEvaluator* Evaluator;
	/** Result of the last datacenter test */
	UPROPERTY()
	EQosCompletionResult QosEvalResult;
	/** Array of all known regions and the datacenters in them */
	UPROPERTY()
	TArray<FRegionQosInstance> RegionOptions;

	/** Value forced to be the region (development) */
	UPROPERTY()
	FString ForceRegionId;
	/** Was the region forced via commandline */
	UPROPERTY()
	bool bRegionForcedViaCommandline;
	/** Value set by the game to be the current region */
	UPROPERTY()
	FString SelectedRegionId;

	TArray<FSimpleDelegate> OnQosEvalCompleteDelegate;
	FSimpleDelegate OnQoSSettingsChangedDelegate;
};

