// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// Includes
#include "NetAnalytics.h"


/**
 * Container class for separating analytics variables and processing, from the main NetConnection code
 */
struct FNetConnAnalyticsVars
{
public:
	/** Default constructor */
	FNetConnAnalyticsVars();

	bool operator == (const FNetConnAnalyticsVars& A) const;

	void CommitAnalytics(FNetConnAnalyticsVars& AggregatedData);

public:
	/** The number of packets that were exclusively ack packets */
	uint64 OutAckOnlyCount;

	/** The number of packets that were just keep-alive packets */
	uint64 OutKeepAliveCount;
};


/**
 * NetConnection implementation for basic aggregated net analytics data
 */
struct FNetConnAnalyticsData : public TBasicNetAnalyticsData<FNetConnAnalyticsVars>
{
public:
	virtual void SendAnalytics() override;
};
