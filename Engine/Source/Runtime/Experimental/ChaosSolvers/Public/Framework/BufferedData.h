// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if INCLUDE_CHAOS

namespace Chaos
{
	/**
	 * Container type for double buffered physics data. Wrap whatever results object
	 * in this to have well definied semantics for accessing each side of a buffer and
	 * flipping it
	 */
	template<typename DataType>
	class TBufferedData
	{
	public:

		TBufferedData()
			: SyncCounter(0)
			, BufferIndex(0)
		{
			DataSyncCounts[0] = 0;
			DataSyncCounts[1] = 0;
		}

		/**
		 * Flips the double buffer, no locks here - if synchronizing multiple threads make sure there's a lock somewhere
		 */
		void Flip()
		{
			BufferIndex.Store(GetGameDataIndex());
		}

		/**
		 * Get a readable reference for the game thread side of the double buffer
		 */
		const DataType& GetGameDataForRead() const
		{
			return Data[GetGameDataIndex()];
		}

		/**
		 * Get a readable reference for the physics side of the double buffer
		 */
		const DataType& GetPhysicsDataForRead() const
		{
			return Data[GetPhysicsDataIndex()];
		}

		/**
		 * Get the counter for the last written state on the game side
		 */
		int32 GetGameDataSyncCount() const 
		{
			return DataSyncCounts[GetGameDataIndex()];
		}

		/**
		 * Get the counter for the last written state on the physics side
		 */
		int32 GetPhysicsDataSyncCount() const
		{
			return DataSyncCounts[GetPhysicsDataIndex()];
		}

		/**
		 * Only for the game side to call, gets a writable reference to the game side data.
		 * Mainly useful for exchanging ptrs in the data type. For copying just call GetGameDataForRead
		 */
		DataType& GetGameDataForWrite()
		{
			return Data[GetGameDataIndex()];
		}

		/**
		 * Only for the physics side to call, gets a writable reference to the physics side and
		 * increments the current sync counter to uniquely identify this write
		 */
		DataType& GetPhysicsDataForWrite()
		{
			int32 DataIndex = GetPhysicsDataIndex();
			DataSyncCounts[DataIndex] = ++SyncCounter;
			return Data[DataIndex];
		}

		/**
		 * Direct access to buffered data, useful to initialise members before
		 * beginning simulation. Never use once the data is being managed over
		 * multiple threads
		 */
		DataType& Get(int32 InIndex)
		{
			checkSlow(InIndex == 0 || InIndex == 1);
			return Data[InIndex];
		}

	private:

		int32 GetPhysicsDataIndex() const
		{
			return BufferIndex.Load();
		}

		int32 GetGameDataIndex() const
		{
			return GetPhysicsDataIndex() == 1 ? 0 : 1;
		}

		// Counter used to identify writes
		uint32 SyncCounter;
		// Counter values for each side of the buffer
		uint32 DataSyncCounts[2];

		// Atomic index for accessing the buffer sides
		TAtomic<int32> BufferIndex;

		// The actual data type stored
		DataType Data[2];
	};
}

#endif