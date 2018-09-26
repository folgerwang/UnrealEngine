// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Generation/ChunkWriter.h"
#include "Logging/LogMacros.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Async/Async.h"
#include "Core/AsyncHelpers.h"
#include "Common/FileSystem.h"
#include "Data/ChunkData.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_CLASS(LogChunkWriter, Log, All);

/** Here lies stats that should be reimplemented where possible

StatFileCreateTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Create Time"), EStatFormat::Timer);
StatCheckExistsTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Check Exist Time"), EStatFormat::Timer);
StatCompressTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Compress Time"), EStatFormat::Timer);
StatSerlialiseTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Serialize Time"), EStatFormat::Timer);
StatChunksSaved = StatsCollector->CreateStat(TEXT("Chunk Writer: Num Saved"), EStatFormat::Value);
StatDataWritten = StatsCollector->CreateStat(TEXT("Chunk Writer: Data Size Written"), EStatFormat::DataSize);
StatDataWriteSpeed = StatsCollector->CreateStat(TEXT("Chunk Writer: Data Write Speed"), EStatFormat::DataSpeed);
StatCompressionRatio = StatsCollector->CreateStat(TEXT("Chunk Writer: Compression Ratio"), EStatFormat::Percentage);

*/

namespace BuildPatchServices
{
	typedef TTuple<TArray<uint8>, FGuid, uint64, FSHAHash> FChunkDataJob;
	typedef TTuple <FGuid, int64> FChunkOutputSize;
	typedef TTuple <FGuid, uint64> FChunkOutputHash;
	typedef TTuple <FGuid, FSHAHash> FChunkOutputSha;

	class FWriterChunkDataAccess
		: public IChunkDataAccess
	{
	public:
		FWriterChunkDataAccess(TArray<uint8>& InDataRef, FChunkHeader& InHeaderRef)
			: DataRef(InDataRef)
			, HeaderRef(InHeaderRef)
		{
		}
		// IChunkDataAccess interface begin.
		virtual void GetDataLock(const uint8** OutChunkData, const FChunkHeader** OutChunkHeader) const override
		{
			(*OutChunkData) = DataRef.GetData();
			(*OutChunkHeader) = &HeaderRef;
		}
		virtual void GetDataLock(uint8** OutChunkData, FChunkHeader** OutChunkHeader) override
		{
			(*OutChunkData) = DataRef.GetData();
			(*OutChunkHeader) = &HeaderRef;
		}
		virtual void ReleaseDataLock() const
		{
		}
		// IChunkDataAccess interface end.

	private:
		TArray<uint8>& DataRef;
		FChunkHeader& HeaderRef;
	};

	class FParallelChunkWriter
		: public IParallelChunkWriter
	{
	public:
		FParallelChunkWriter(FParallelChunkWriterConfig InConfig, IFileSystem* InFileSystem, IChunkDataSerialization* InChunkDataSerialization, FStatsCollector* InStatsCollector)
			: Config(MoveTemp(InConfig))
			, FileSystem(InFileSystem)
			, ChunkDataSerialization(InChunkDataSerialization)
			, StatsCollector(InStatsCollector)
			, bMoreDataIsExpected(true)
			, bShouldAbort(false)
		{
			FileSystem->MakeDirectory(*Config.ChunkDirectory);
			if (!FileSystem->DirectoryExists(*Config.ChunkDirectory))
			{
				UE_LOG(LogChunkWriter, Fatal, TEXT("Could not create cloud directory (%s)."), *Config.ChunkDirectory);
			}
			for (int32 ThreadIdx = 0; ThreadIdx < Config.NumberOfThreads; ++ThreadIdx)
			{
				WriterThreads.Add(Async<void>(EAsyncExecution::Thread, [this]()
				{
					WriterThread();
				}));
			}
		}

		~FParallelChunkWriter()
		{
			bShouldAbort = true;
			for (const TFuture<void>& Thread : WriterThreads)
			{
				Thread.Wait();
			}
			WriterThreads.Empty();
		}

		// IParallelChunkWriter interface begin.
		virtual void AddChunkData(TArray<uint8> ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash, const FSHAHash& ChunkSha) override
		{
			DebugCheckSingleProducer();
			while (ChunkDataJobQueueCount.GetValue() >= Config.MaxQueueSize)
			{
				FPlatformProcess::Sleep(0);
			}
			ChunkDataJobQueueCount.Increment();
			ChunkDataJobQueue.Enqueue(FChunkDataJob(MoveTemp(ChunkData), ChunkGuid, ChunkHash, ChunkSha));
		}
		virtual FParallelChunkWriterSummaries OnProcessComplete() override
		{
			bMoreDataIsExpected = false;
			for (const TFuture<void>& Thread : WriterThreads)
			{
				Thread.Wait();
			}
			WriterThreads.Empty();
			FChunkOutputSize ChunkOutputSize;
			while (ChunkOutputSizeQueue.Dequeue(ChunkOutputSize))
			{
				ParallelChunkWriterSummaries.ChunkOutputSizes.Add(ChunkOutputSize.Get<0>(), ChunkOutputSize.Get<1>());
			}
			FChunkOutputHash ChunkOutputHash;
			while (ChunkOutputHashQueue.Dequeue(ChunkOutputHash))
			{
				ParallelChunkWriterSummaries.ChunkOutputHashes.Add(ChunkOutputHash.Get<0>(), ChunkOutputHash.Get<1>());
			}
			FChunkOutputSha ChunkOutputSha;
			while (ChunkOutputShaQueue.Dequeue(ChunkOutputSha))
			{
				ParallelChunkWriterSummaries.ChunkOutputShas.Add(ChunkOutputSha.Get<0>(), ChunkOutputSha.Get<1>());
			}
			return ParallelChunkWriterSummaries;
		}
		// IParallelChunkWriter interface end.

	private:
		void WriterThread()
		{
			bool bReceivedJob;
			FChunkDataJob ChunkDataJob;
			while (!bShouldAbort && (bMoreDataIsExpected || ChunkDataJobQueueCount.GetValue() > 0))
			{
				ChunkDataJobQueueConsumerCS.Lock();
				bReceivedJob = ChunkDataJobQueue.Dequeue(ChunkDataJob);
				ChunkDataJobQueueConsumerCS.Unlock();
				if (bReceivedJob)
				{
					ChunkDataJobQueueCount.Decrement();
					TArray<uint8>& ChunkData = ChunkDataJob.Get<0>();
					FGuid& ChunkGuid = ChunkDataJob.Get<1>();
					uint64& ChunkHash = ChunkDataJob.Get<2>();
					FSHAHash& ChunkSha = ChunkDataJob.Get<3>();
					FChunkHeader ChunkHeader;
					ChunkHeader.Guid = ChunkGuid;
					ChunkHeader.DataSizeCompressed = ChunkData.Num();
					ChunkHeader.DataSizeUncompressed = ChunkData.Num();
					ChunkHeader.StoredAs = EChunkStorageFlags::None;
					ChunkHeader.HashType = EChunkHashFlags::RollingPoly64 | EChunkHashFlags::Sha1;
					ChunkHeader.RollingHash = ChunkHash;
					ChunkHeader.SHAHash = ChunkSha;
					TUniquePtr<FWriterChunkDataAccess> ChunkDataAccess(new FWriterChunkDataAccess(ChunkData, ChunkHeader));
					const FString NewChunkFilename = FBuildPatchUtils::GetChunkNewFilename(Config.FeatureLevel, Config.ChunkDirectory, ChunkGuid, ChunkHash);
					if (!FileSystem->FileExists(*NewChunkFilename))
					{
						bool bSaveSuccess = false;
						int32 RetryCount = Config.SaveRetryCount;
						while (!bShouldAbort && !bSaveSuccess && RetryCount-- >= 0)
						{
							FileSystem->MakeDirectory(*FPaths::GetPath(NewChunkFilename));
							TUniquePtr<FArchive> ChunkFileOut = FileSystem->CreateFileWriter(*NewChunkFilename);
							if (!ChunkFileOut.IsValid())
							{
								UE_LOG(LogChunkWriter, Log, TEXT("Could not create chunk (%s)."), *NewChunkFilename);
								FPlatformProcess::Sleep(Config.SaveRetryTime);
								continue;
							}
							EChunkSaveResult ChunkSaveResult = ChunkDataSerialization->SaveToArchive(*ChunkFileOut.Get(), ChunkDataAccess.Get());
							ChunkOutputSizeQueue.Enqueue(FChunkOutputSize(ChunkGuid, ChunkFileOut->TotalSize()));
							ChunkOutputHashQueue.Enqueue(FChunkOutputHash(ChunkGuid, ChunkHash));
							ChunkOutputShaQueue.Enqueue(FChunkOutputSha(ChunkGuid, ChunkSha));
							if (ChunkFileOut->IsError() || ChunkSaveResult != EChunkSaveResult::Success)
							{
								UE_LOG(LogChunkWriter, Log, TEXT("Could not save chunk [%s] (%s)."), *ToString(ChunkSaveResult), *NewChunkFilename);
								FPlatformProcess::Sleep(Config.SaveRetryTime);
								continue;
							}
							bSaveSuccess = true;
						}
						if (!bSaveSuccess)
						{
							UE_LOG(LogChunkWriter, Fatal, TEXT("Chunk save failure (%s)."), *NewChunkFilename);
						}
					}
					else
					{
						UE_LOG(LogChunkWriter, Log, TEXT("Skipping already existing chunk file (%s)."), *NewChunkFilename);
					}
				}
				else
				{
					FPlatformProcess::Sleep(1.0/10.0);
				}
			}
		}

		void DebugCheckSingleProducer()
		{
#if !UE_BUILD_SHIPPING
			const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
			if (ProducerThreadId.IsSet())
			{
				check(CurrentThreadId == ProducerThreadId.GetValue());
			}
			else
			{
				ProducerThreadId = CurrentThreadId;
			}
#endif
		}

	private:
		const FParallelChunkWriterConfig Config;
		IFileSystem* const FileSystem;
		IChunkDataSerialization* const ChunkDataSerialization;
		FStatsCollector* const StatsCollector;

		TArray<TFuture<void>> WriterThreads;
		FThreadSafeBool bMoreDataIsExpected;
		FThreadSafeBool bShouldAbort;

		FCriticalSection ChunkDataJobQueueConsumerCS;
		TQueue<FChunkDataJob, EQueueMode::Spsc> ChunkDataJobQueue;
		FThreadSafeInt32 ChunkDataJobQueueCount;

		TQueue<FChunkOutputSize, EQueueMode::Mpsc> ChunkOutputSizeQueue;
		TQueue<FChunkOutputHash, EQueueMode::Mpsc> ChunkOutputHashQueue;
		TQueue<FChunkOutputSha, EQueueMode::Mpsc> ChunkOutputShaQueue;
		FParallelChunkWriterSummaries ParallelChunkWriterSummaries;

#if !UE_BUILD_SHIPPING
		TOptional<uint32> ProducerThreadId;
#endif
	};

	IParallelChunkWriter* FParallelChunkWriterFactory::Create(FParallelChunkWriterConfig Config, IFileSystem* FileSystem, IChunkDataSerialization* ChunkDataSerialization, FStatsCollector* StatsCollector)
	{
		return new FParallelChunkWriter(MoveTemp(Config), FileSystem, ChunkDataSerialization, StatsCollector);
	}
}
