// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once


// Include
#include "CoreMinimal.h"
#include "NetAnalytics.h"


/**
 * Simple container class for separating the analytics related variables from OodleHandlerComponent
 */
struct FOodleAnalyticsVars : public FLocalNetAnalyticsStruct
{
public:
	/**
	 * Default Constructor
	 */
	FOodleAnalyticsVars();

	bool operator == (const FOodleAnalyticsVars& A) const;

	/**
	 * Implements the TThreadedNetAnalyticsData CommitAnalytics interface
	 */
	void CommitAnalytics(FOodleAnalyticsVars& AggregatedData);


public:
	/** The number of incoming compressed packets */
	uint64 InCompressedNum;

	/** The number of incoming packets that were not compressed */
	uint64 InNotCompressedNum;

	/** The compressed length + decompression data overhead, of all incoming packets. The most accurate measure of compression savings. */
	uint64 InCompressedWithOverheadLengthTotal;

	/** The compressed length of all incoming packets. Measures Oodle algorithm compression, minus overhead reducing final savings. */
	uint64 InCompressedLengthTotal;

	/** The decompressed length of all incoming packets. */
	uint64 InDecompressedLengthTotal;

	/** The number of outgoing compressed packets. */
	uint64 OutCompressedNum;

	/** The number of outgoing packets that were not compressed, due to Oodle failing to compress enough. */
	uint64 OutNotCompressedFailedNum;

	/** The number of outgoing packets that were not compressed, due to byte rounding of compressed packets, exceeding size limits. */
	uint64 OutNotCompressedBoundedNum;

	/** The number of outgoing packets that were not compressed, due to a higher level flag requesting they be sent uncompressed. */
	uint64 OutNotCompressedFlaggedNum;

	/** The number of outgoing packets that were not compressed, due to Oodle failing to compress - which exclusively contained ack data */
	uint64 OutNotCompressedFailedAckOnlyNum;

	/** The number of outgoing packets that were not compressed, due to Oodle failing to compress - which were KeepAlive packets */
	uint64 OutNotCompressedFailedKeepAliveNum;

	/** The compressed length + decompression data overhead, of all outgoing packets. The most accurate measure of compression savings. */
	uint64 OutCompressedWithOverheadLengthTotal;

	/** The compressed length of all outgoing packets. Measures Oodle algorithm compression, minus overhead reducing final savings. */
	uint64 OutCompressedLengthTotal;

	/** The length prior to compression, of all outgoing packets. */
	uint64 OutBeforeCompressedLengthTotal;
};

/**
 * Oodle implementation for threaded net analytics data - the threading is taken care of, just need to send off the analytics
 */
struct FOodleNetAnalyticsData :
#if NET_ANALYTICS_MULTITHREADING
	public TThreadedNetAnalyticsData<FOodleAnalyticsVars>
#else
	public FNetAnalyticsData, public FOodleAnalyticsVars
#endif
{
public:
	virtual void SendAnalytics() override;


#if !NET_ANALYTICS_MULTITHREADING
	FOodleAnalyticsVars* GetLocalData()
	{
		return this;
	}
#endif
};

