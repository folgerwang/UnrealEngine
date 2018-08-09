// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Installer/Statistics/MemoryChunkStoreStatistics.h"
#include "Misc/Guid.h"
#include "Core/AsyncHelpers.h"
#include "Installer/Statistics/FileOperationTracker.h"

namespace BuildPatchServices
{
	class FMeanUseTracker
	{
	public:
		FMeanUseTracker();

		void Increment();
		void Decrement();
		void CountSame();
		float GetMean() const;
		int32 GetPeak() const;
		int32 GetCurrent() const;

	private:
		FThreadSafeInt32 UsesCount;
		FThreadSafeInt64 SumCurrentUses;
		FThreadSafeInt32 CurrentUse;
		volatile int32 PeakUse;
	};

	FMeanUseTracker::FMeanUseTracker()
		: UsesCount(0)
		, SumCurrentUses(0)
		, CurrentUse(0)
		, PeakUse(0)
	{
	}

	void FMeanUseTracker::Increment()
	{
		int32 NewValue = CurrentUse.Increment();
		SumCurrentUses.Add(NewValue);
		UsesCount.Increment();
		AsyncHelpers::LockFreePeak(&PeakUse, NewValue);
	}

	void FMeanUseTracker::Decrement()
	{
		int32 NewValue = CurrentUse.Decrement();
		SumCurrentUses.Add(NewValue);
		UsesCount.Increment();
	}

	void FMeanUseTracker::CountSame()
	{
		SumCurrentUses.Add(CurrentUse.GetValue());
		UsesCount.Increment();
	}

	float FMeanUseTracker::GetMean() const
	{
		const double SumCurrentUsesFloat = SumCurrentUses.GetValue();
		const double UseChangeCountFloat = UsesCount.GetValue();
		return UseChangeCountFloat > 0.f ? SumCurrentUsesFloat / UseChangeCountFloat : 0.f;
	}

	int32 FMeanUseTracker::GetPeak() const
	{
		return PeakUse;
	}

	int32 FMeanUseTracker::GetCurrent() const
	{
		return CurrentUse.GetValue();
	}

	class FMemoryChunkStoreStatistics
		: public IMemoryChunkStoreStatistics
	{
	public:
		FMemoryChunkStoreStatistics(const TSet<FGuid>& MultipleReferencedChunks, IFileOperationTracker* FileOperationTracker, FMeanUseTracker* AggregateStoreUseTracker, FMeanUseTracker* AggregateStoreRetainTracker, FThreadSafeInt32* AggregateNumChunksBooted, FThreadSafeInt32* AggregateStoreSize);
		~FMemoryChunkStoreStatistics();

		// IMemoryChunkStoreStat interface begin.
		virtual void OnChunkStored(const FGuid& ChunkId) override;
		virtual void OnChunkReleased(const FGuid& ChunkId) override;
		virtual void OnChunkBooted(const FGuid& ChunkId) override;
		virtual void OnStoreUseUpdated(int32 ChunkCount) override;
		virtual void OnStoreSizeUpdated(int32 Size) override;
		// IMemoryChunkStoreStat interface end.

		// IMemoryChunkStoreStatistics interface begin.
		virtual int32 GetStoreUse() const override;
		virtual int32 GetStoreRetained() const override;
		virtual int32 GetNumBooted() const override;
		virtual int32 GetStoreSize() const override;
		// IMemoryChunkStoreStatistics interface end.

	private:
		const TSet<FGuid>& MultipleReferencedChunks;
		IFileOperationTracker* const FileOperationTracker;
		FMeanUseTracker StoreUseTracker;
		FMeanUseTracker StoreRetainTracker;
		FThreadSafeInt32 NumChunksBooted;
		FThreadSafeInt32 StoreSize;
		FMeanUseTracker* AggregateStoreUseTracker;
		FMeanUseTracker* AggregateStoreRetainTracker;
		FThreadSafeInt32* AggregateNumChunksBooted;
		FThreadSafeInt32* AggregateStoreSize;
	};

	FMemoryChunkStoreStatistics::FMemoryChunkStoreStatistics(const TSet<FGuid>& InMultipleReferencedChunks, IFileOperationTracker* InFileOperationTracker, FMeanUseTracker* InStoreUseTracker, FMeanUseTracker* InStoreRetainTracker, FThreadSafeInt32* InNumChunksBooted, FThreadSafeInt32* InTotalStoreSize)
		: MultipleReferencedChunks(InMultipleReferencedChunks)
		, FileOperationTracker(InFileOperationTracker)
		, AggregateStoreUseTracker(InStoreUseTracker)
		, AggregateStoreRetainTracker(InStoreRetainTracker)
		, AggregateNumChunksBooted(InNumChunksBooted)
		, AggregateStoreSize(InTotalStoreSize)
	{
	}

	FMemoryChunkStoreStatistics::~FMemoryChunkStoreStatistics()
	{
	}

	void FMemoryChunkStoreStatistics::OnChunkStored(const FGuid& ChunkId)
	{
		StoreUseTracker.Increment();
		AggregateStoreUseTracker->Increment();
		FileOperationTracker->OnDataStateUpdate(ChunkId, EFileOperationState::DataInMemoryStore);
		if (MultipleReferencedChunks.Contains(ChunkId))
		{
			StoreRetainTracker.Increment();
			AggregateStoreRetainTracker->Increment();
		}
		else
		{
			StoreRetainTracker.CountSame();
			AggregateStoreRetainTracker->CountSame();
		}
	}

	void FMemoryChunkStoreStatistics::OnChunkReleased(const FGuid& ChunkId)
	{
		StoreUseTracker.Decrement();
		AggregateStoreUseTracker->Decrement();
		if (MultipleReferencedChunks.Contains(ChunkId))
		{
			StoreRetainTracker.Decrement();
			AggregateStoreRetainTracker->Decrement();
		}
		else
		{
			StoreRetainTracker.CountSame();
			AggregateStoreRetainTracker->CountSame();
		}
	}

	void FMemoryChunkStoreStatistics::OnChunkBooted(const FGuid& ChunkId)
	{
		NumChunksBooted.Increment();
		AggregateNumChunksBooted->Increment();
		// We desire a release count too, as it tracks just numbers in the store.
		OnChunkReleased(ChunkId);
	}

	void FMemoryChunkStoreStatistics::OnStoreUseUpdated(int32 ChunkCount)
	{
	}

	void FMemoryChunkStoreStatistics::OnStoreSizeUpdated(int32 Size)
	{
		int32 Diff = Size - StoreSize.Set(Size);
		if (Diff > 0)
		{
			AggregateStoreSize->Add(Diff);
		}
		else
		{
			AggregateStoreSize->Subtract(FMath::Abs(Diff));
		}
	}

	int32 FMemoryChunkStoreStatistics::GetStoreUse() const
	{
		return StoreUseTracker.GetCurrent();
	}

	int32 FMemoryChunkStoreStatistics::GetStoreRetained() const
	{
		return StoreRetainTracker.GetCurrent();
	}

	int32 FMemoryChunkStoreStatistics::GetNumBooted() const
	{
		return NumChunksBooted.GetValue();
	}

	int32 FMemoryChunkStoreStatistics::GetStoreSize() const
	{
		return StoreSize.GetValue();
	}

	class FMemoryChunkStoreAggregateStatistics
		: public IMemoryChunkStoreAggregateStatistics
	{
	public:
		FMemoryChunkStoreAggregateStatistics(const TSet<FGuid>& MultipleReferencedChunks, IFileOperationTracker* FileOperationTracker);
		~FMemoryChunkStoreAggregateStatistics();

		// IMemoryChunkStoreStatistics interface begin.
		virtual IMemoryChunkStoreStatistics* Expose(int32 Index) override;
		virtual float GetAverageStoreUse() const override;
		virtual int32 GetPeakStoreUse() const override;
		virtual float GetAverageStoreRetained() const override;
		virtual int32 GetPeakStoreRetained() const override;
		virtual int32 GetTotalStoreSize() const override;
		virtual int32 GetTotalNumBooted() const override;
		// IMemoryChunkStoreStatistics interface end.

	private:
		const TSet<FGuid> MultipleReferencedChunks;
		IFileOperationTracker* const FileOperationTracker;
		FMeanUseTracker AggregateStoreUseTracker;
		FMeanUseTracker AggregateStoreRetainTracker;
		FThreadSafeInt32 AggregateNumChunksBooted;
		FThreadSafeInt32 AggregateStoreSize;
		TMap<int32, TUniquePtr<FMemoryChunkStoreStatistics>> MemoryChunkStoreStatistics;
	};

	FMemoryChunkStoreAggregateStatistics::FMemoryChunkStoreAggregateStatistics(const TSet<FGuid>& InMultipleReferencedChunks, IFileOperationTracker* InFileOperationTracker)
		: MultipleReferencedChunks(InMultipleReferencedChunks)
		, FileOperationTracker(InFileOperationTracker)
		, AggregateNumChunksBooted(0)
		, AggregateStoreSize(0)
	{
	}

	FMemoryChunkStoreAggregateStatistics::~FMemoryChunkStoreAggregateStatistics()
	{
	}

	IMemoryChunkStoreStatistics* FMemoryChunkStoreAggregateStatistics::Expose(int32 Index)
	{
		if (MemoryChunkStoreStatistics.Contains(Index) == false)
		{
			MemoryChunkStoreStatistics.Emplace(Index, new FMemoryChunkStoreStatistics(
				MultipleReferencedChunks,
				FileOperationTracker,
				&AggregateStoreUseTracker,
				&AggregateStoreRetainTracker,
				&AggregateNumChunksBooted,
				&AggregateStoreSize));
		}
		return MemoryChunkStoreStatistics[Index].Get();
	}

	float FMemoryChunkStoreAggregateStatistics::GetAverageStoreUse() const
	{
		return AggregateStoreUseTracker.GetMean();
	}

	int32 FMemoryChunkStoreAggregateStatistics::GetPeakStoreUse() const
	{
		return AggregateStoreUseTracker.GetPeak();
	}

	float FMemoryChunkStoreAggregateStatistics::GetAverageStoreRetained() const
	{
		return AggregateStoreRetainTracker.GetMean();
	}

	int32 FMemoryChunkStoreAggregateStatistics::GetPeakStoreRetained() const
	{
		return AggregateStoreRetainTracker.GetPeak();
	}

	int32 FMemoryChunkStoreAggregateStatistics::GetTotalStoreSize() const
	{
		return AggregateStoreSize.GetValue();
	}

	int32 FMemoryChunkStoreAggregateStatistics::GetTotalNumBooted() const
	{
		return AggregateNumChunksBooted.GetValue();
	}

	IMemoryChunkStoreAggregateStatistics* FMemoryChunkStoreAggregateStatisticsFactory::Create(const TSet<FGuid>& MultipleReferencedChunks, IFileOperationTracker* FileOperationTracker)
	{
		check(FileOperationTracker != nullptr);
		return new FMemoryChunkStoreAggregateStatistics(MultipleReferencedChunks, FileOperationTracker);
	}
};