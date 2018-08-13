// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/MemoryChunkStore.h"

namespace BuildPatchServices
{
	class IMemoryChunkStoreStat;
	class IFileOperationTracker;

	/**
	 * Interface to the statistics class which provides access to tracked values from a memory chunk store stat.
	 */
	class IMemoryChunkStoreStatistics
		: public IMemoryChunkStoreStat
	{
	public:
		/**
		 * @return the number of chunks held.
		 */
		virtual int32 GetStoreUse() const = 0;

		/**
		 * @return the number of chunks held which are retained due to multiple references.
		 */
		virtual int32 GetStoreRetained() const = 0;

		/**
		 * @return the number of chunks which have been booted.
		 */
		virtual int32 GetNumBooted() const = 0;

		/**
		 * @return the maximum number of chunks which can be held.
		 */
		virtual int32 GetStoreSize() const = 0;
	};

	/**
	 * Interface to the statistics class which aggregates tracked values from multiple memory chunk store stats.
	 */
	class IMemoryChunkStoreAggregateStatistics
	{
	public:
		virtual ~IMemoryChunkStoreAggregateStatistics() { }

		/**
		 * Exposes an IMemoryChunkStoreStatistics interface which can be given to a memory chunk store and used for individual stats.
		 * @param Index     The index for the interface to get. Repeated calls with the same value will get the same instance.
		 * @return the interface for the stat instance assigned to the given index.
		 */
		virtual IMemoryChunkStoreStatistics* Expose(int32 Index) = 0;

		/**
		 * @return the average number of chunks held in the memory stores.
		 */
		virtual float GetAverageStoreUse() const = 0;

		/**
		 * @return the peak number of chunks held in the memory stores.
		 */
		virtual int32 GetPeakStoreUse() const = 0;

		/**
		 * @return the average number of chunks held in the memory stores which are retained due to multiple references.
		 */
		virtual float GetAverageStoreRetained() const = 0;

		/**
		 * @return the peak number of chunks held in the memory stores which are retained due to multiple references.
		 */
		virtual int32 GetPeakStoreRetained() const = 0;

		/**
		 * @return the total number of chunks which can be held in memory stores.
		 */
		virtual int32 GetTotalStoreSize() const = 0;

		/**
		 * @return the number of chunks which were booted from memory stores.
		 */
		virtual int32 GetTotalNumBooted() const = 0;
	};

	/**
	 * A factory for creating an IMemoryChunkStoreAggregateStatistics instance.
	 */
	class FMemoryChunkStoreAggregateStatisticsFactory
	{
	public:
		/**
		 * Creates a statistics interface which exposes individual memory chunk stores stat dependencies, and collates
		 * calls received by these into additional aggregated information.
		 * @param MultipleReferencedChunks      For retained stats, the set of chunks which have multiple references.
		 * @param FileOperationTracker          The file operation tracker which will be used to update data states.
		 * @return the new IMemoryChunkStoreAggregateStatistics instance created.
		 */
		static IMemoryChunkStoreAggregateStatistics* Create(const TSet<FGuid>& MultipleReferencedChunks, IFileOperationTracker* FileOperationTracker);
	};
}