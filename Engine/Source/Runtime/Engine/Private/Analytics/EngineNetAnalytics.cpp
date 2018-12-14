// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

// Includes
#include "Analytics/EngineNetAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineLogs.h"

/**
 * FNetConnAnalyticsVars
 */

FNetConnAnalyticsVars::FNetConnAnalyticsVars()
	: OutAckOnlyCount(0)
	, OutKeepAliveCount(0)
{
}

bool FNetConnAnalyticsVars::operator == (const FNetConnAnalyticsVars& A) const
{
	return OutAckOnlyCount == A.OutAckOnlyCount &&
			OutKeepAliveCount == A.OutKeepAliveCount;
}

void FNetConnAnalyticsVars::CommitAnalytics(FNetConnAnalyticsVars& AggregatedData)
{
	AggregatedData.OutAckOnlyCount += OutAckOnlyCount;
	AggregatedData.OutKeepAliveCount += OutKeepAliveCount;
}


/**
 * FNetConnAnalyticsData
 */

void FNetConnAnalyticsData::SendAnalytics()
{
	FNetConnAnalyticsVars NullVars;
	const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = Aggregator->GetAnalyticsProvider();

	if (!(*this == NullVars) && AnalyticsProvider.IsValid())
	{
		UE_LOG(LogNet, Log, TEXT("NetConnection Analytics:"));

		UE_LOG(LogNet, Log, TEXT(" - OutAckOnlyCount: %llu"), OutAckOnlyCount);
		UE_LOG(LogNet, Log, TEXT(" - OutKeepAliveCount: %llu"), OutKeepAliveCount);


		static const FString EZEventName = TEXT("Core.ServerNetConn");
		static const FString EZAttrib_OutAckOnlyCount = TEXT("OutAckOnlyCount");
		static const FString EZAttrib_OutKeepAliveCount = TEXT("OutKeepAliveCount");

		AnalyticsProvider->RecordEvent(EZEventName, MakeAnalyticsEventAttributeArray(
			EZAttrib_OutAckOnlyCount, OutAckOnlyCount,
			EZAttrib_OutKeepAliveCount, OutKeepAliveCount
		));
	}
}
