// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "QosRegionManager.h"
#include "QosEvaluator.generated.h"

class FQosDatacenterStats;
class FTimerManager;
class IAnalyticsProvider;
struct FIcmpEchoResult;
enum class EQosResponseType : uint8;

/**
 * Input parameters to start a qos ping check
 */
struct FQosParams
{
	/** Number of ping requests per region */
	int32 NumTestsPerRegion;
	/** Amount of time to wait for each request */
	float Timeout;
};

/*
 * Delegate triggered when an evaluation of ping for all servers in a search query have completed
 *
 * @param Result the ping operation result
 */
DECLARE_DELEGATE_OneParam(FOnQosPingEvalComplete, EQosCompletionResult /** Result */);

/** 
 * Delegate triggered when all QoS search results have been investigated 
 *
 * @param Result the QoS operation result
 * @param DatacenterInstances The per-datacenter ping information
 */
DECLARE_DELEGATE_TwoParams(FOnQosSearchComplete, EQosCompletionResult /** Result */, const TArray<FDatacenterQosInstance>& /** DatacenterInstances */);

/**
 * Evaluates QoS metrics to determine the best datacenter under current conditions
 * Additionally capable of generically pinging an array of servers that have a QosBeaconHost active
 */
UCLASS(config = Engine)
class QOS_API UQosEvaluator : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * QoS services
	 */

	/**
	 * Find all the advertised datacenters and begin the process of evaluating ping results
	 * Will return the default datacenter in the event of failure or no advertised datacenters
	 *
	 * @param InParams parameters defining the 
	 * @param InRegions array of regions to query
	 * @param InDatacenters array of datacenters to query
	 * @param InCompletionDelegate delegate to fire when a datacenter choice has been made
	 */
	void FindDatacenters(const FQosParams& InParams, const TArray<FQosRegionInfo>& InRegions, const TArray<FQosDatacenterInfo>& InDatacenters, const FOnQosSearchComplete& InCompletionDelegate);

	/**
	 * Is a QoS operation active
	 *
	 * @return true if QoS is active, false otherwise
	 */
	bool IsActive() const { return bInProgress; }

	/**
	 * Cancel the current QoS operation at the earliest opportunity
	 */
	void Cancel();

	void SetWorld(UWorld* InWorld);

protected:

	/**
	 * Use the udp ping code to ping known servers
	 *
	 * @param InParams parameters defining the request
	 * @param InCompletionDelegate delegate to fire when all regions have completed their tests
	 */
	void PingRegionServers(const FQosParams& InParams, const FOnQosSearchComplete& InCompletionDelegate);

private:

	/** Reference to external UWorld */
	TWeakObjectPtr<UWorld> ParentWorld;

	FOnQosPingEvalComplete OnQosPingEvalComplete;

	/** Start time of total test */
	double StartTimestamp;
	/** A QoS operation is in progress */
	UPROPERTY()
	bool bInProgress;
	/** Should cancel occur at the next available opportunity */
	UPROPERTY()
	bool bCancelOperation;

	/** Array of datacenters currently being evaluated */
	UPROPERTY(Transient)
	TArray<FDatacenterQosInstance> Datacenters;

	/**
	 * @return true if all ping requests have completed (new method)
	 */
	bool AreAllRegionsComplete();

	void OnPingResultComplete(const FString& RegionId, int32 NumTests, const FIcmpEchoResult& Result);

	/**
	 * Take all found ping results and process them before consumption at higher levels
	 *
	 * @param TimeToDiscount amount of time to subtract from calculation to compensate for external factors (frame rate, etc)
	 */
	void CalculatePingAverages(int32 TimeToDiscount = 0);

public:

	/**
	 * Analytics
	 */

	void SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider);

private:

	void StartAnalytics();
	void EndAnalytics(EQosCompletionResult CompletionResult);

	/** Reference to the provider to submit data to */
	TSharedPtr<IAnalyticsProvider> AnalyticsProvider;
	/** Stats related to these operations */
	TSharedPtr<FQosDatacenterStats> QosStats;

private:

	/**
	 * Helpers
	 */

	/** Quick access to the current world */
	UWorld* GetWorld() const;

	/** Quick access to the world timer manager */
	FTimerManager& GetWorldTimerManager() const;
};

inline const TCHAR* ToString(EQosDatacenterResult Result)
{
	switch (Result)
	{
		case EQosDatacenterResult::Invalid:
		{
			return TEXT("Invalid");
		}
		case EQosDatacenterResult::Success:
		{
			return TEXT("Success");
		}
		case EQosDatacenterResult::Incomplete:
		{
			return TEXT("Incomplete");
		}
	}

	return TEXT("");
}

inline const TCHAR* ToString(EQosCompletionResult Result)
{
	switch (Result)
	{
		case EQosCompletionResult::Invalid:
		{
			return TEXT("Invalid");
		}
		case EQosCompletionResult::Success:
		{
			return TEXT("Success");
		}
		case EQosCompletionResult::Failure:
		{
			return TEXT("Failure");
		}
		case EQosCompletionResult::Canceled:
		{
			return TEXT("Canceled");
		}
	}

	return TEXT("");
}
