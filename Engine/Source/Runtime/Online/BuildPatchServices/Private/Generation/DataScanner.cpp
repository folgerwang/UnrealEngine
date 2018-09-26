// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Generation/DataScanner.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Data/ChunkData.h"
#include "BuildPatchHash.h"

namespace BuildPatchServices
{
	class FDataScanner
		: public IDataScanner
	{
	public:
		FDataScanner(const TArray<uint32>& ChunkWindowSizes, const TArray<uint8>& Data, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector);
		virtual ~FDataScanner();

		virtual bool IsComplete() override;
		virtual TArray<FChunkMatch> GetResultWhenComplete() override;

	private:
		uint32 ConsumeData(FRollingHash& RollingHash, const uint8* Data, uint32 DataLen);
		bool FindChunkDataMatch(const TMap<uint64, TSet<FGuid>>& ChunkInventory, const TMap<FGuid, FSHAHash>& ChunkShaHashes, FRollingHash& RollingHash, FGuid& ChunkMatch, FSHAHash& ChunkSha);
		int32 InsertMatch(TArray<FChunkMatch>& CurrentMatches, int32 SearchIdx, const uint64& InDataOffset, const FGuid& InChunkGuid, const uint32& InWindowSize);
		TArray<FChunkMatch> ScanData();

	private:
		const bool bAllowSkipMatches;
		const TArray<uint32>& ChunkWindowSizes;
		const TArray<uint8>& Data;
		ICloudEnumerationRef CloudEnumeration;
		FStatsCollectorRef StatsCollector;
		FThreadSafeBool bIsComplete;
		FThreadSafeBool bShouldAbort;
		TFuture<TArray<FChunkMatch>> FutureResult;
		volatile FStatsCollector::FAtomicValue* StatCreatedScanners;
		volatile FStatsCollector::FAtomicValue* StatRunningScanners;
		volatile FStatsCollector::FAtomicValue* StatCompleteScanners;
		volatile FStatsCollector::FAtomicValue* StatCpuTime;
		volatile FStatsCollector::FAtomicValue* StatRealTime;
		volatile FStatsCollector::FAtomicValue* StatHashCollisions;
		volatile FStatsCollector::FAtomicValue* StatTotalData;
		volatile FStatsCollector::FAtomicValue* StatSkippedData;
		volatile FStatsCollector::FAtomicValue* StatProcessingSpeed;

	public:
		static FThreadSafeCounter NumIncompleteScanners;
		static FThreadSafeCounter NumRunningScanners;
	};

	FDataScanner::FDataScanner(const TArray<uint32>& InChunkWindowSizes, const TArray<uint8>& InData, const ICloudEnumerationRef& InCloudEnumeration, const FStatsCollectorRef& InStatsCollector)
		: bAllowSkipMatches(true)
		, ChunkWindowSizes(InChunkWindowSizes)
		, Data(InData)
		, CloudEnumeration(InCloudEnumeration)
		, StatsCollector(InStatsCollector)
		, bIsComplete(false)
		, bShouldAbort(false)
	{
		// Create statistics.
		StatCreatedScanners = StatsCollector->CreateStat(TEXT("Scanner: Created Scanners"), EStatFormat::Value);
		StatRunningScanners = StatsCollector->CreateStat(TEXT("Scanner: Running Scanners"), EStatFormat::Value);
		StatCompleteScanners = StatsCollector->CreateStat(TEXT("Scanner: Complete Scanners"), EStatFormat::Value);
		StatCpuTime = StatsCollector->CreateStat(TEXT("Scanner: CPU Time"), EStatFormat::Timer);
		StatRealTime = StatsCollector->CreateStat(TEXT("Scanner: Real Time"), EStatFormat::Timer);
		StatHashCollisions = StatsCollector->CreateStat(TEXT("Scanner: Hash Collisions"), EStatFormat::Value);
		StatTotalData = StatsCollector->CreateStat(TEXT("Scanner: Total Data"), EStatFormat::DataSize);
		StatSkippedData = StatsCollector->CreateStat(TEXT("Scanner: Skipped Data"), EStatFormat::DataSize);
		StatProcessingSpeed = StatsCollector->CreateStat(TEXT("Scanner: Processing Speed"), EStatFormat::DataSpeed);
		FStatsCollector::Accumulate(StatCreatedScanners, 1);

		// Queue thread.
		NumIncompleteScanners.Increment();
		TFunction<TArray<FChunkMatch>()> Task = [this]()
		{
			TArray<FChunkMatch> Result = ScanData();
			NumIncompleteScanners.Decrement();
			FStatsCollector::Accumulate(StatCompleteScanners, 1);
			return MoveTemp(Result);
		};
		FutureResult = Async(EAsyncExecution::ThreadPool, MoveTemp(Task));
	}

	FDataScanner::~FDataScanner()
	{
		// Make sure the task is complete.
		bShouldAbort = true;
		FutureResult.Wait();
	}

	bool FDataScanner::IsComplete()
	{
		return bIsComplete;
	}

	TArray<FChunkMatch> FDataScanner::GetResultWhenComplete()
	{
		return FutureResult.Get();
	}

	uint32 FDataScanner::ConsumeData(FRollingHash& RollingHash, const uint8* DataPtr, uint32 DataLen)
	{
		uint32 NumDataNeeded = RollingHash.GetNumDataNeeded();
		if (NumDataNeeded > 0 && NumDataNeeded <= DataLen)
		{
			RollingHash.ConsumeBytes(DataPtr, NumDataNeeded);
			checkSlow(RollingHash.GetNumDataNeeded() == 0);
			return NumDataNeeded;
		}
		return 0;
	}

	bool FDataScanner::FindChunkDataMatch(const TMap<uint64, TSet<FGuid>>& ChunkInventory, const TMap<FGuid, FSHAHash>& ChunkShaHashes, FRollingHash& RollingHash, FGuid& ChunkMatch, FSHAHash& ChunkSha)
	{
		const TSet<FGuid>* PotentialMatches = ChunkInventory.Find(RollingHash.GetWindowHash());
		bool bFoundMatch = false;
		if (PotentialMatches != nullptr)
		{
			RollingHash.GetWindowData().GetShaHash(ChunkSha);
			// Always return first match in list however count all collisions.
			for (const FGuid& PotentialMatch : *PotentialMatches)
			{
				const FSHAHash* PotentialMatchSha = ChunkShaHashes.Find(PotentialMatch);
				if (PotentialMatchSha != nullptr && *PotentialMatchSha == ChunkSha)
				{
					if (!bFoundMatch)
					{
						ChunkMatch = PotentialMatch;
						bFoundMatch = true;
					}
				}
				else
				{
					FStatsCollector::Accumulate(StatHashCollisions, 1);
				}
			}
		}
		return bFoundMatch;
	}

	int32 FDataScanner::InsertMatch(TArray<FChunkMatch>& CurrentMatches, int32 SearchIdx, const uint64& DataFirst, const FGuid& ChunkGuid, const uint32& DataSize)
	{
		const uint64 DataLast = (DataFirst + DataSize) - 1;

		// The rule is it can overlap anything before it, but the next item in the list must not be overlapped if it is bigger
		// This is assuming a lot about the code calling it, but that is ok for now
		// There are several places that need behavior like this, FBlockStructure should be extended to support merge-able meta, or no-merge, ignore/replace type behavior.

		// Find where start sits between
		for(int32 Idx = 0/*SearchIdx*/; Idx < CurrentMatches.Num(); ++Idx)
		{
			const uint64 ThisMatchFirst = CurrentMatches[Idx].DataOffset;
			const uint64 ThisMatchLast = (ThisMatchFirst + CurrentMatches[Idx].WindowSize) - 1;
			const uint64 ThisMatchSize = CurrentMatches[Idx].WindowSize;

			// Can be inserted before?
			if(DataFirst < ThisMatchFirst)
			{
				// Obv insert if we fit entirely before ThisMatch..
				const bool bFitsInGap = DataLast < ThisMatchFirst;
				if (bFitsInGap)
				{
					check(DataSize < ThisMatchSize);
					CurrentMatches.EmplaceAt(Idx, DataFirst, ChunkGuid, DataSize);
					return Idx;
				}
				return SearchIdx;
			}
			// No shits given based on assumptions...
			else if (DataFirst == ThisMatchFirst)
			{
				return Idx;
			}
			// No shits given based on assumptions...
			else if (DataLast <= ThisMatchLast)
			{
				return Idx;
			}
			// Otherwise may go after..
		}

		// If we did nothing in the loop, we add to end!
		return CurrentMatches.Emplace(DataFirst, ChunkGuid, DataSize);
	}

	TArray<FChunkMatch> FDataScanner::ScanData()
	{
		static volatile FStatsCollector::FAtomicValue TempTimerValue;

		// Count running scanners.
		NumRunningScanners.Increment();
		
		// The return data.
		TArray<FChunkMatch> DataScanResult;

		// Get refs for the chunk inventory.
		const TMap<uint64, TSet<FGuid>>& ChunkInventory = CloudEnumeration->GetChunkInventory();
		const TMap<FGuid, FSHAHash>& ChunkShaHashes = CloudEnumeration->GetChunkShaHashes();

		for (const uint32 WindowSize : ChunkWindowSizes)
		{
			FRollingHash RollingHash(WindowSize);

			// Temp values.
			int32 TempMatchIdx = 0;
			FGuid ChunkMatch;
			FSHAHash ChunkSha;
			uint64 CpuTimer;

			// Track last match so we know if we can start skipping data. This will also cover us for the overlap with previous scanner.
			uint64 LastMatch = 0;

			// Loop over and process all data.
			uint32 NextByte = ConsumeData(RollingHash, &Data[0], Data.Num());
			bool bScanningData = true;
			{
				FStatsCollector::AccumulateTimeBegin(CpuTimer);
				FStatsParallelScopeTimer ParallelScopeTimer(&TempTimerValue, StatRealTime, StatRunningScanners);
				while (bScanningData && !bShouldAbort)
				{
					const uint32 DataStart = NextByte - WindowSize;
					const bool bChunkOverlap = DataStart < (LastMatch + WindowSize);
					// Check for a chunk match at this offset.
					const bool bFoundChunkMatch = FindChunkDataMatch(ChunkInventory, ChunkShaHashes, RollingHash, ChunkMatch, ChunkSha);
					if (bFoundChunkMatch)
					{
						LastMatch = DataStart;
						TempMatchIdx = InsertMatch(DataScanResult, TempMatchIdx, DataStart, ChunkMatch, WindowSize);
					}
					// We can start skipping over the chunk that we matched if we have no overlap potential, i.e. we know this match will not be rejected.
					if (bAllowSkipMatches && bFoundChunkMatch && !bChunkOverlap)
					{
						RollingHash.Clear();
						const bool bHasEnoughData = (NextByte + WindowSize - 1) < static_cast<uint32>(Data.Num());
						if (bHasEnoughData)
						{
							const uint32 Consumed = ConsumeData(RollingHash, &Data[NextByte], Data.Num() - NextByte);
							FStatsCollector::Accumulate(StatSkippedData, Consumed);
							NextByte += Consumed;
						}
						else
						{
							bScanningData = false;
						}
					}
					// Otherwise we only move forwards by one byte.
					else
					{
						const bool bHasMoreData = NextByte < static_cast<uint32>(Data.Num());
						if (bHasMoreData)
						{
							// Roll over next byte.
							RollingHash.RollForward(Data[NextByte++]);
						}
						else
						{
							bScanningData = false;
						}
					}
				}
				FStatsCollector::AccumulateTimeEnd(StatCpuTime, CpuTimer);
				FStatsCollector::Accumulate(StatTotalData, Data.Num());
				FStatsCollector::Set(StatProcessingSpeed, *StatTotalData / FStatsCollector::CyclesToSeconds(ParallelScopeTimer.GetCurrentTime()));
			}
		}

		// Count running scanners.
		NumRunningScanners.Decrement();

		bIsComplete = true;
		return DataScanResult;
	}

	FThreadSafeCounter FDataScanner::NumIncompleteScanners;
	FThreadSafeCounter FDataScanner::NumRunningScanners;

	int32 FDataScannerCounter::GetNumIncompleteScanners()
	{
		return FDataScanner::NumIncompleteScanners.GetValue();
	}

	int32 FDataScannerCounter::GetNumRunningScanners()
	{
		return FDataScanner::NumRunningScanners.GetValue();
	}

	IDataScannerRef FDataScannerFactory::Create(const TArray<uint32>& ChunkWindowSizes, const TArray<uint8>& Data, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector)
	{
		return MakeShareable(new FDataScanner(ChunkWindowSizes, Data, CloudEnumeration, StatsCollector));
	}
}
