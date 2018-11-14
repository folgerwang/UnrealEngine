// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Includes
#include "OodleAnalytics.h"
#include "OodleHandlerComponent.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"


/**
 * FOodleAnalyticsVars
 */

FOodleAnalyticsVars::FOodleAnalyticsVars()
	: FLocalNetAnalyticsStruct()
	, InCompressedNum(0)
	, InNotCompressedNum(0)
	, InCompressedWithOverheadLengthTotal(0)
	, InCompressedLengthTotal(0)
	, InDecompressedLengthTotal(0)
	, OutCompressedNum(0)
	, OutNotCompressedFailedNum(0)
	, OutNotCompressedBoundedNum(0)
	, OutNotCompressedFlaggedNum(0)
	, OutNotCompressedFailedAckOnlyNum(0)
	, OutNotCompressedFailedKeepAliveNum(0)
	, OutCompressedWithOverheadLengthTotal(0)
	, OutCompressedLengthTotal(0)
	, OutBeforeCompressedLengthTotal(0)
{
}

bool FOodleAnalyticsVars::operator == (const FOodleAnalyticsVars& A) const
{
	return A.InCompressedNum == InCompressedNum &&
		A.InNotCompressedNum == InNotCompressedNum &&
		A.InCompressedWithOverheadLengthTotal == InCompressedWithOverheadLengthTotal &&
		A.InCompressedLengthTotal == InCompressedLengthTotal &&
		A.InDecompressedLengthTotal == InDecompressedLengthTotal &&
		A.OutCompressedNum == OutCompressedNum &&
		A.OutNotCompressedFailedNum == OutNotCompressedFailedNum &&
		A.OutNotCompressedBoundedNum == OutNotCompressedBoundedNum &&
		A.OutNotCompressedFlaggedNum == OutNotCompressedFlaggedNum &&
		A.OutNotCompressedFailedAckOnlyNum == OutNotCompressedFailedAckOnlyNum &&
		A.OutNotCompressedFailedKeepAliveNum == OutNotCompressedFailedKeepAliveNum &&
		A.OutCompressedWithOverheadLengthTotal == OutCompressedWithOverheadLengthTotal &&
		A.OutCompressedLengthTotal == OutCompressedLengthTotal &&
		A.OutBeforeCompressedLengthTotal == OutBeforeCompressedLengthTotal;
}

void FOodleAnalyticsVars::CommitAnalytics(FOodleAnalyticsVars& AggregatedData)
{
	AggregatedData.InCompressedNum += InCompressedNum;
	AggregatedData.InNotCompressedNum += InNotCompressedNum;
	AggregatedData.InCompressedWithOverheadLengthTotal += InCompressedWithOverheadLengthTotal;
	AggregatedData.InCompressedLengthTotal += InCompressedLengthTotal;
	AggregatedData.InDecompressedLengthTotal += InDecompressedLengthTotal;
	AggregatedData.OutCompressedNum += OutCompressedNum;
	AggregatedData.OutNotCompressedFailedNum += OutNotCompressedFailedNum;
	AggregatedData.OutNotCompressedBoundedNum += OutNotCompressedBoundedNum;
	AggregatedData.OutNotCompressedFlaggedNum += OutNotCompressedFlaggedNum;
	AggregatedData.OutNotCompressedFailedAckOnlyNum += OutNotCompressedFailedAckOnlyNum;
	AggregatedData.OutNotCompressedFailedKeepAliveNum += OutNotCompressedFailedKeepAliveNum;
	AggregatedData.OutCompressedWithOverheadLengthTotal += OutCompressedWithOverheadLengthTotal;
	AggregatedData.OutCompressedLengthTotal += OutCompressedLengthTotal;
	AggregatedData.OutBeforeCompressedLengthTotal += OutBeforeCompressedLengthTotal;
}


/**
 * FOodleNetAnalyticsData
 */

void FOodleNetAnalyticsData::SendAnalytics()
{
	FOodleAnalyticsVars NullVars;
	const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = Aggregator->GetAnalyticsProvider();

	// Only send analytics if there is something to send
	if (!(*this == NullVars) && AnalyticsProvider.IsValid())
	{
		/** The number of outgoing packets that were not compressed, in total */
		uint64 OutNotCompressedNumTotal = OutNotCompressedFailedNum + OutNotCompressedBoundedNum + OutNotCompressedFlaggedNum;

		/**
		 * The below values measure Oodle algorithm compression, minus overhead reducing final savings.
		 */
			/** The percentage of compression savings, of all incoming packets. */
			int8 InSavingsPercentTotal = (1.0 - ((double)InCompressedLengthTotal / (double)InDecompressedLengthTotal)) * 100.0;

			/** The percentage of compression savings, of all outgoing packets. */
			int8 OutSavingsPercentTotal = (1.0 - ((double)OutCompressedLengthTotal / (double)OutBeforeCompressedLengthTotal)) * 100.0;

			/** The number of bytes saved due to compression, of all incoming packets. */
			int64 InSavingsBytesTotal = InDecompressedLengthTotal - InCompressedLengthTotal;

			/** The number of bytes saved due to compression, of all outgoing packets. */
			int64 OutSavingsBytesTotal = OutBeforeCompressedLengthTotal - OutCompressedLengthTotal;

		/**
		 * The below values measure compressed length + decompression data overhead, which reduces final savings.
		 * This is the most accurate measure of compression savings.
		 */
			/** The percentage of compression savings, of all incoming packets. */
			int8 InSavingsWithOverheadPercentTotal = (1.0 - ((double)InCompressedWithOverheadLengthTotal / (double)InDecompressedLengthTotal)) * 100.0;

			/** The percentage of compression savings, of all outgoing packets. */
			int8 OutSavingsWithOverheadPercentTotal = (1.0 - ((double)OutCompressedWithOverheadLengthTotal / (double)OutBeforeCompressedLengthTotal)) * 100.0;

			/** The number of bytes saved due to compression, of all incoming packets. */
			int64 InSavingsWithOverheadBytesTotal = InDecompressedLengthTotal - InCompressedWithOverheadLengthTotal;

			/** The number of bytes saved due to compression, of all outgoing packets. */
			int64 OutSavingsWithOverheadBytesTotal = OutBeforeCompressedLengthTotal - OutCompressedWithOverheadLengthTotal;


		UE_LOG(OodleHandlerComponentLog, Log, TEXT("Oodle Analytics:"));
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InCompressedNum: %llu"), InCompressedNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InNotCompressedNum: %llu"), InNotCompressedNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InCompressedWithOverheadLengthTotal: %llu"), InCompressedWithOverheadLengthTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InCompressedLengthTotal: %llu"), InCompressedLengthTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InDecompressedLengthTotal: %llu"), InDecompressedLengthTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutCompressedNum: %llu"), OutCompressedNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedNum: %llu"), OutNotCompressedFailedNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutNotCompressedBoundedNum: %llu"), OutNotCompressedBoundedNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutNotCompressedFlaggedNum: %llu"), OutNotCompressedFlaggedNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedAckOnlyNum: %llu"), OutNotCompressedFailedAckOnlyNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedKeepAliveNum: %llu"), OutNotCompressedFailedKeepAliveNum);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutCompressedWithOverheadLengthTotal: %llu"), OutCompressedWithOverheadLengthTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutCompressedLengthTotal: %llu"), OutCompressedLengthTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutBeforeCompressedLengthTotal: %llu"), OutBeforeCompressedLengthTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutNotCompressedNumTotal: %llu"), OutNotCompressedNumTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InSavingsPercentTotal: %i"), InSavingsPercentTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutSavingsPercentTotal: %i"), OutSavingsPercentTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InSavingsBytesTotal: %lli"), InSavingsBytesTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutSavingsBytesTotal: %lli"), OutSavingsBytesTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InSavingsWithOverheadPercentTotal: %i"), InSavingsWithOverheadPercentTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutSavingsWithOverheadPercentTotal: %i"), OutSavingsWithOverheadPercentTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - InSavingsWithOverheadBytesTotal: %lli"), InSavingsWithOverheadBytesTotal);
		UE_LOG(OodleHandlerComponentLog, Log, TEXT(" - OutSavingsWithOverheadBytesTotal: %lli"), OutSavingsWithOverheadBytesTotal);


		static const FString EZEventName = TEXT("Oodle.Stats");
		static const FString EZAttrib_InCompressedNum = TEXT("InCompressedNum");
		static const FString EZAttrib_InNotCompressedNum = TEXT("InNotCompressedNum");
		static const FString EZAttrib_InCompressedWithOverheadLengthTotal = TEXT("InCompressedWithOverheadLengthTotal");
		static const FString EZAttrib_InCompressedLengthTotal = TEXT("InCompressedLengthTotal");
		static const FString EZAttrib_InDecompressedLengthTotal = TEXT("InDecompressedLengthTotal");
		static const FString EZAttrib_OutCompressedNum = TEXT("OutCompressedNum");
		static const FString EZAttrib_OutNotCompressedFailedNum = TEXT("OutNotCompressedFailedNum");
		static const FString EZAttrib_OutNotCompressedBoundedNum = TEXT("OutNotCompressedBoundedNum");
		static const FString EZAttrib_OutNotCompressedFlaggedNum = TEXT("OutNotCompressedFlaggedNum");
		static const FString EZAttrib_OutNotCompressedFailedAckOnlyNum = TEXT("OutNotCompressedFailedAckOnlyNum");
		static const FString EZAttrib_OutNotCompressedFailedKeepAliveNum = TEXT("OutNotCompressedFailedKeepAliveNum");
		static const FString EZAttrib_OutCompressedWithOverheadLengthTotal = TEXT("OutCompressedWithOverheadLengthTotal");
		static const FString EZAttrib_OutCompressedLengthTotal = TEXT("OutCompressedLengthTotal");
		static const FString EZAttrib_OutBeforeCompressedLengthTotal = TEXT("OutBeforeCompressedLengthTotal");
		static const FString EZAttrib_OutNotCompressedNumTotal = TEXT("OutNotCompressedNumTotal");
		static const FString EZAttrib_InSavingsPercentTotal = TEXT("InSavingsPercentTotal");
		static const FString EZAttrib_OutSavingsPercentTotal = TEXT("OutSavingsPercentTotal");
		static const FString EZAttrib_InSavingsBytesTotal = TEXT("InSavingsBytesTotal");
		static const FString EZAttrib_OutSavingsBytesTotal = TEXT("OutSavingsBytesTotal");
		static const FString EZAttrib_InSavingsWithOverheadPercentTotal = TEXT("InSavingsWithOverheadPercentTotal");
		static const FString EZAttrib_OutSavingsWithOverheadPercentTotal = TEXT("OutSavingsWithOverheadPercentTotal");
		static const FString EZAttrib_InSavingsWithOverheadBytesTotal = TEXT("InSavingsWithOverheadBytesTotal");
		static const FString EZAttrib_OutSavingsWithOverheadBytesTotal = TEXT("OutSavingsWithOverheadBytesTotal");

		AnalyticsProvider->RecordEvent(EZEventName, MakeAnalyticsEventAttributeArray(
			EZAttrib_InCompressedNum, InCompressedNum,
			EZAttrib_InNotCompressedNum, InNotCompressedNum,
			EZAttrib_InCompressedWithOverheadLengthTotal, InCompressedWithOverheadLengthTotal,
			EZAttrib_InCompressedLengthTotal, InCompressedLengthTotal,
			EZAttrib_InDecompressedLengthTotal, InDecompressedLengthTotal,
			EZAttrib_OutCompressedNum, OutCompressedNum,
			EZAttrib_OutNotCompressedFailedNum, OutNotCompressedFailedNum,
			EZAttrib_OutNotCompressedBoundedNum, OutNotCompressedBoundedNum,
			EZAttrib_OutNotCompressedFlaggedNum, OutNotCompressedFlaggedNum,
			EZAttrib_OutNotCompressedFailedAckOnlyNum, OutNotCompressedFailedAckOnlyNum,
			EZAttrib_OutNotCompressedFailedKeepAliveNum, OutNotCompressedFailedKeepAliveNum,
			EZAttrib_OutCompressedWithOverheadLengthTotal, OutCompressedWithOverheadLengthTotal,
			EZAttrib_OutCompressedLengthTotal, OutCompressedLengthTotal,
			EZAttrib_OutBeforeCompressedLengthTotal, OutBeforeCompressedLengthTotal,
			EZAttrib_OutNotCompressedNumTotal, OutNotCompressedNumTotal,
			EZAttrib_InSavingsPercentTotal, InSavingsPercentTotal,
			EZAttrib_OutSavingsPercentTotal, OutSavingsPercentTotal,
			EZAttrib_InSavingsBytesTotal, InSavingsBytesTotal,
			EZAttrib_OutSavingsBytesTotal, OutSavingsBytesTotal,
			EZAttrib_InSavingsWithOverheadPercentTotal, InSavingsWithOverheadPercentTotal,
			EZAttrib_OutSavingsWithOverheadPercentTotal, OutSavingsWithOverheadPercentTotal,
			EZAttrib_InSavingsWithOverheadBytesTotal, InSavingsWithOverheadBytesTotal,
			EZAttrib_OutSavingsWithOverheadBytesTotal, OutSavingsWithOverheadBytesTotal
		));
	}
}
