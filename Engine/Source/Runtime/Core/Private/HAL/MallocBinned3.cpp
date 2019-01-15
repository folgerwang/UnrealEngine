// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocBinned.cpp: Binned memory allocator
=============================================================================*/

#include "HAL/MallocBinned3.h"

#if PLATFORM_64BITS
#include "Logging/LogMacros.h"
#include "Misc/ScopeLock.h"
#include "Templates/Function.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "HAL/MemoryMisc.h"
#include "HAL/PlatformMisc.h"




#if BINNED3_ALLOW_RUNTIME_TWEAKING

int32 GMallocBinned3PerThreadCaches = DEFAULT_GMallocBinned3PerThreadCaches;
static FAutoConsoleVariableRef GMallocBinned3PerThreadCachesCVar(
	TEXT("MallocBinned3.PerThreadCaches"),
	GMallocBinned3PerThreadCaches,
	TEXT("Enables per-thread caches of small (<= 32768 byte) allocations from FMallocBinned3")
	);

int32 GMallocBinned3BundleSize = DEFAULT_GMallocBinned3BundleSize;
static FAutoConsoleVariableRef GMallocBinned3BundleSizeCVar(
	TEXT("MallocBinned3.BundleSize"),
	GMallocBinned3BundleSize,
	TEXT("Max size in bytes of per-block bundles used in the recycling process")
	);

int32 GMallocBinned3BundleCount = DEFAULT_GMallocBinned3BundleCount;
static FAutoConsoleVariableRef GMallocBinned3BundleCountCVar(
	TEXT("MallocBinned3.BundleCount"),
	GMallocBinned3BundleCount,
	TEXT("Max count in blocks per-block bundles used in the recycling process")
	);

int32 GMallocBinned3MaxBundlesBeforeRecycle = BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle;
static FAutoConsoleVariableRef GMallocBinned3MaxBundlesBeforeRecycleCVar(
	TEXT("MallocBinned3.BundleRecycleCount"),
	GMallocBinned3MaxBundlesBeforeRecycle,
	TEXT("Number of freed bundles in the global recycler before it returns them to the system, per-block size. Limited by BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle (currently 4)")
	);

int32 GMallocBinned3AllocExtra = DEFAULT_GMallocBinned3AllocExtra;
static FAutoConsoleVariableRef GMallocBinned3AllocExtraCVar(
	TEXT("MallocBinned3.AllocExtra"),
	GMallocBinned3AllocExtra,
	TEXT("When we do acquire the lock, how many blocks cached in TLS caches. In no case will we grab more than a page.")
	);

#endif

#if BINNED3_ALLOCATOR_STATS
int64 Binned3AllocatedSmallPoolMemory = 0; // memory that's requested to be allocated by the game
int64 Binned3AllocatedOSSmallPoolMemory = 0;

int64 Binned3AllocatedLargePoolMemory = 0; // memory requests to the OS which don't fit in the small pool
int64 Binned3AllocatedLargePoolMemoryWAlignment = 0; // when we allocate at OS level we need to align to a size

int64 Binned3PoolInfoMemory = 0;
int64 Binned3HashMemory = 0;
int64 Binned3FreeBitsMemory = 0;
int64 Binned3TLSMemory = 0;
TAtomic<int64> Binned3TotalPoolSearches;
TAtomic<int64> Binned3TotalPointerTests;


#endif

#define BINNED3_TIME_LARGE_BLOCKS (0)

#if BINNED3_TIME_LARGE_BLOCKS
TAtomic<double> MemoryRangeReserveTotalTime(0.0);
TAtomic<int32> MemoryRangeReserveTotalCount(0);

TAtomic<double> MemoryRangeFreeTotalTime(0.0);
TAtomic<int32> MemoryRangeFreeTotalCount(0);
#endif

#define BINNED3_LARGE_POOL_CANARIES 0 // need to repad the data structure so that the pagesize divides by this (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))

// Block sizes are based around getting the maximum amount of allocations per pool, with as little alignment waste as possible.
// Block sizes should be close to even divisors of the system page size, and well distributed.
// They must be 16-byte aligned as well.
static uint32 Binned3SmallBlockSizes4k[] = 
{
	16, 32, 48, 64, 80, 96, 112, 128, 160, // +16
	192, 224, 256, 288, 320, // +32
	368,  // /11 ish
	400,  // /10 ish
	448,  // /9 ish
	512,  // /8
	576,  // /7 ish
	672,  // /6 ish
	816,  // /5 ish 
	1024, // /4 
	1360, // /3 ish
	2048, // /2
	4096 // /1
};

static uint32 Binned3SmallBlockSizes8k[] =
{
	736,  // /11 ish
	1168, // /7 ish
	1632, // /5 ish
	2720, // /3 ish
	8192  // /1
};

static uint32 Binned3SmallBlockSizes12k[] =
{
	//1104, // /11 ish
	//1216, // /10 ish
	1536, // /8
	1744, // /7 ish
	2448, // /5 ish
	3072, // /4
	6144, // /2
	12288 // /1
};

static uint32 Binned3SmallBlockSizes16k[] =
{
	//1488, // /11 ish
	//1808, // /9 ish
	//2336, // /7 ish
	3264, // /5 ish
	5456, // /3 ish
	16384 // /1
};

static uint32 Binned3SmallBlockSizes20k[] =
{
	// 2912, // /7 ish
	//3408, // /6 ish
	5120, // /4
	//6186, // /3 ish
	10240, // /2
	20480  // /1
};

static uint32 Binned3SmallBlockSizes24k[] = // 1 total
{
	24576  // /1
};

static uint32 Binned3SmallBlockSizes28k[] = // 6 total
{
	4768,  // /6 ish
	5728,  // /5 ish
	7168,  // /4
	9552,  // /3
	14336, // /2
	28672  // /1
};



struct FSizeTableEntry
{
	uint32 BlockSize;
	uint16 BlocksPerBlockOfBlocks;
	uint8 PagesPlatformForBlockOfBlocks;

	FSizeTableEntry()
	{
	}

	FSizeTableEntry(uint32 InBlockSize, uint64 PlatformPageSize, uint8 Pages4k)
		: BlockSize(InBlockSize)
	{
		check((PlatformPageSize & (BINNED3_BASE_PAGE_SIZE - 1)) == 0 && PlatformPageSize >= BINNED3_BASE_PAGE_SIZE && InBlockSize % BINNED3_MINIMUM_ALIGNMENT == 0);

		uint64 Page4kPerPlatformPage = PlatformPageSize / BINNED3_BASE_PAGE_SIZE;

		PagesPlatformForBlockOfBlocks = 0;
		while (true)
		{
			check(PagesPlatformForBlockOfBlocks < MAX_uint8);
			PagesPlatformForBlockOfBlocks++;
			if (PagesPlatformForBlockOfBlocks * Page4kPerPlatformPage < Pages4k)
			{
				continue;
			}
			if (PagesPlatformForBlockOfBlocks * Page4kPerPlatformPage % Pages4k != 0)
			{
				continue;
			}
			break;
		}
		check((PlatformPageSize * PagesPlatformForBlockOfBlocks) / BlockSize <= MAX_uint16);
		BlocksPerBlockOfBlocks = (PlatformPageSize * PagesPlatformForBlockOfBlocks) / BlockSize;
	}

	bool operator<(const FSizeTableEntry& Other) const
	{
		return BlockSize < Other.BlockSize;
	}
};

static FSizeTableEntry SizeTable[BINNED3_SMALL_POOL_COUNT];

static void FillSizeTable(uint64 PlatformPageSize)
{
	int32 Index = 0;

	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes4k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes4k[Sub], PlatformPageSize, 1);
	}
	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes8k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes8k[Sub], PlatformPageSize, 2);
	}
	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes12k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes12k[Sub], PlatformPageSize, 3);
	}
	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes16k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes16k[Sub], PlatformPageSize, 4);
	}
	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes20k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes20k[Sub], PlatformPageSize, 5);
	}
	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes24k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes24k[Sub], PlatformPageSize, 6);
	}
	for (int32 Sub = 0; Sub < ARRAY_COUNT(Binned3SmallBlockSizes28k); Sub++)
	{
		SizeTable[Index++] = FSizeTableEntry(Binned3SmallBlockSizes28k[Sub], PlatformPageSize, 7);
	}
	Sort(&SizeTable[0], Index);
	check(SizeTable[Index - 1].BlockSize == BINNED3_MAX_LISTED_SMALL_POOL_SIZE);
	check(IsAligned(BINNED3_MAX_LISTED_SMALL_POOL_SIZE, BINNED3_BASE_PAGE_SIZE));
	for (uint32 Size = BINNED3_MAX_LISTED_SMALL_POOL_SIZE + BINNED3_BASE_PAGE_SIZE; Size <= BINNED3_MAX_SMALL_POOL_SIZE; Size += BINNED3_BASE_PAGE_SIZE)
	{
		SizeTable[Index++] = FSizeTableEntry(Size, PlatformPageSize, Size / BINNED3_BASE_PAGE_SIZE);
	}
	check(Index == ARRAY_COUNT(SizeTable));
	check(Index == BINNED3_SMALL_POOL_COUNT);

}

MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) static uint8 Binned3UnusedAlignPadding[PLATFORM_CACHE_LINE_SIZE] GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE) = { 0 };
uint16 FMallocBinned3::SmallBlockSizesReversedShifted[BINNED3_SMALL_POOL_COUNT] = { 0 };
uint32 FMallocBinned3::Binned3TlsSlot = 0;
uint32 FMallocBinned3::OsAllocationGranularity = 0;
uint32 FMallocBinned3::MaxAlignmentForMemoryRangeReserve = 0;

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	uint8* FMallocBinned3::Binned3BaseVMPtr = nullptr;
#else
	uint64 FMallocBinned3::PoolSearchDiv = 0;
	uint8* FMallocBinned3::HighestPoolBaseVMPtr = nullptr;
	uint8* FMallocBinned3::PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT] = { nullptr };
#endif

FMallocBinned3* FMallocBinned3::MallocBinned3 = nullptr;
// Mapping of sizes to small table indices
uint8 FMallocBinned3::MemSizeToIndex[1 + (BINNED3_MAX_SMALL_POOL_SIZE >> BINNED3_MINIMUM_ALIGNMENT_SHIFT)] = { 0 };

struct FMallocBinned3::FPoolInfoSmall
{
	enum ECanary
	{
		SmallUnassigned = 0x39,
		SmallAssigned = 0x71
	};

	uint32 Canary : 7;
	uint32 Taken : 12;
	uint32 NoFirstFreeIndex : 1;
	uint32 FirstFreeIndex : 12;

	FPoolInfoSmall()
		: Canary(ECanary::SmallUnassigned)
		, Taken(0)
		, NoFirstFreeIndex(1)
		, FirstFreeIndex(0)
	{
		static_assert(sizeof(FPoolInfoSmall) == 4, "Padding fail");
	}
	void CheckCanary(ECanary ShouldBe) const
	{
		if (Canary != ShouldBe)
		{
			UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x"), int32(Canary), int32(ShouldBe));
		}
	}
	void SetCanary(ECanary ShouldBe, bool bPreexisting, bool bGuarnteedToBeNew)
	{
		if (bPreexisting)
		{
			if (bGuarnteedToBeNew)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x. This block is both preexisting and guaranteed to be new; which makes no sense."), int32(Canary), int32(ShouldBe));
			}
			if (ShouldBe == ECanary::SmallUnassigned)
			{
				if (Canary != ECanary::SmallAssigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x because this block should be preexisting and in use."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x because this block should be preexisting."), int32(Canary), int32(ShouldBe));
			}
		}
		else
		{
			if (bGuarnteedToBeNew)
			{
				if (Canary != ECanary::SmallUnassigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x. This block is guaranteed to be new yet is it already assigned."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe && Canary != ECanary::SmallUnassigned)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x does not have an expected value."), int32(Canary), int32(ShouldBe));
			}
		}
		Canary = ShouldBe;
	}
	bool HasFreeRegularBlock() const
	{
		CheckCanary(ECanary::SmallAssigned);
		return !NoFirstFreeIndex;
	}

	void* AllocateRegularBlock(uint8* BlockOfBlocksPtr, uint32 BlockSize)
	{
		check(HasFreeRegularBlock());
		++Taken;
		FFreeBlock* Free = (FFreeBlock*)(BlockOfBlocksPtr + BlockSize * FirstFreeIndex);
		void* Result = Free->AllocateRegularBlock();
		if (Free->GetNumFreeRegularBlocks() == 0)
		{
			if (Free->NextFreeIndex == MAX_uint32)
			{
				FirstFreeIndex = 0;
				NoFirstFreeIndex = 1;
			}
			else
			{
				FirstFreeIndex = Free->NextFreeIndex;
				check(uint32(FirstFreeIndex) == Free->NextFreeIndex);
				check(((FFreeBlock*)(BlockOfBlocksPtr + BlockSize * FirstFreeIndex))->GetNumFreeRegularBlocks());
			}
		}

		return Result;
	}
};

struct FMallocBinned3::FPoolInfoLarge
{
	enum class ECanary : uint16
	{
		LargeUnassigned = 0x3943,
		LargeAssigned = 0x17ea,
	};

public:
#if BINNED3_LARGE_POOL_CANARIES
	ECanary	Canary;	// See ECanary
#endif
private:
	uint32 AllocSize;      // Number of bytes allocated
	uint32 OSAllocSize;      // Number of bytes allocated aligned for OS

public:
	FPoolInfoLarge() :
#if BINNED3_LARGE_POOL_CANARIES
		Canary(ECanary::LargeUnassigned),
#endif
		AllocSize(0),
		OSAllocSize(0)
	{
	}
	void CheckCanary(ECanary ShouldBe) const
	{
#if BINNED3_LARGE_POOL_CANARIES
		if (Canary != ShouldBe)
		{
			UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x"), int32(Canary), int32(ShouldBe));
		}
#endif
	}
	void SetCanary(ECanary ShouldBe, bool bPreexisting, bool bGuarnteedToBeNew)
	{
#if BINNED3_LARGE_POOL_CANARIES
		if (bPreexisting)
		{
			if (bGuarnteedToBeNew)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x. This block is both preexisting and guaranteed to be new; which makes no sense."), int32(Canary), int32(ShouldBe));
			}
			if (ShouldBe == ECanary::LargeUnassigned)
			{
				if (Canary != ECanary::LargeAssigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x because this block should be preexisting and in use."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x because this block should be preexisting."), int32(Canary), int32(ShouldBe));
			}
		}
		else
		{
			if (bGuarnteedToBeNew)
			{
				if (Canary != ECanary::LargeUnassigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x. This block is guaranteed to be new yet is it already assigned."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe && Canary != ECanary::LargeUnassigned)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x does not have an expected value."), int32(Canary), int32(ShouldBe));
			}
		}
		Canary = ShouldBe;
#endif
	}
	uint32 GetOSRequestedBytes() const
	{
		return AllocSize;
	}

	UPTRINT GetOsAllocatedBytes() const
	{
		CheckCanary(ECanary::LargeAssigned);
		return (UPTRINT)OSAllocSize;
	}

	void SetOSAllocationSizes(uint32 InRequestedBytes, UPTRINT InAllocatedBytes)
	{
		CheckCanary(ECanary::LargeAssigned);
		check(InRequestedBytes != 0);                // Shouldn't be pooling zero byte allocations
		check(InAllocatedBytes >= InRequestedBytes); // We must be allocating at least as much as we requested

		AllocSize = InRequestedBytes;
		OSAllocSize = InAllocatedBytes;
	}
};


/** Hash table struct for retrieving allocation book keeping information */
struct FMallocBinned3::PoolHashBucket
{
	UPTRINT BucketIndex;
	FPoolInfoLarge* FirstPool;
	PoolHashBucket* Prev;
	PoolHashBucket* Next;

	PoolHashBucket()
	{
		BucketIndex = 0;
		FirstPool   = nullptr;
		Prev        = this;
		Next        = this;
	}

	void Link(PoolHashBucket* After)
	{
		After->Prev = Prev;
		After->Next = this;
		Prev ->Next = After;
		this ->Prev = After;
	}

	void Unlink()
	{
		Next->Prev = Prev;
		Prev->Next = Next;
		Prev       = this;
		Next       = this;
	}
};



struct FMallocBinned3::Private
{
	// Implementation. 
	static CA_NO_RETURN void OutOfMemory(uint64 Size, uint32 Alignment=0)
	{
		// this is expected not to return
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}

	/**
	* Gets the FPoolInfoSmall for a small block memory address. If no valid info exists one is created.
	*/
	static FPoolInfoSmall* GetOrCreatePoolInfoSmall(FMallocBinned3& Allocator, uint32 InPoolIndex, uint32 BlockOfBlocksIndex)
	{
		FPoolInfoSmall*& InfoBlock = Allocator.SmallPoolTables[InPoolIndex].PoolInfos[BlockOfBlocksIndex / Allocator.SmallPoolInfosPerPlatformPage];
		if (!InfoBlock)
		{
			InfoBlock = (FPoolInfoSmall*)FPlatformMemory::MemoryRangeReserve(Allocator.OsAllocationGranularity, true);
			if (!InfoBlock)
			{
				Private::OutOfMemory(Allocator.OsAllocationGranularity);
			}
#if BINNED3_ALLOCATOR_STATS
			Binned3PoolInfoMemory += Allocator.OsAllocationGranularity;
#endif
			DefaultConstructItems<FPoolInfoSmall>((void*)InfoBlock, Allocator.SmallPoolInfosPerPlatformPage);
		}

		FPoolInfoSmall* Result = &InfoBlock[BlockOfBlocksIndex % Allocator.SmallPoolInfosPerPlatformPage];

		bool bGuaranteedToBeNew = false;
		if (BlockOfBlocksIndex >= Allocator.SmallPoolTables[InPoolIndex].NumEverUsedBlockOfBlocks)
		{
			bGuaranteedToBeNew = true;
			Allocator.SmallPoolTables[InPoolIndex].NumEverUsedBlockOfBlocks = BlockOfBlocksIndex + 1;
		}
		Result->SetCanary(FPoolInfoSmall::ECanary::SmallAssigned, false, bGuaranteedToBeNew);
		return Result;
	}
	/**
	 * Gets the FPoolInfoLarge for a large block memory address. If no valid info exists one is created.
	 */
	static FPoolInfoLarge* GetOrCreatePoolInfoLarge(FMallocBinned3& Allocator, void* InPtr)
	{
		/** 
		 * Creates an array of FPoolInfo structures for tracking allocations.
		 */
		auto CreatePoolArray = [](uint64 NumPools)
		{
			uint64 PoolArraySize = NumPools * sizeof(FPoolInfoLarge);

			void* Result;
			{
				LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
				Result = FPlatformMemory::MemoryRangeReserve(PoolArraySize, true);
#if BINNED3_ALLOCATOR_STATS
				Binned3PoolInfoMemory += PoolArraySize;
#endif
			}

			if (!Result)
			{
				OutOfMemory(PoolArraySize);
			}

			DefaultConstructItems<FPoolInfoLarge>(Result, NumPools);
			return (FPoolInfoLarge*)Result;
		};

		uint32 BucketIndex;
		UPTRINT BucketIndexCollision;
		uint32  PoolIndex;
		Allocator.PtrToPoolMapping.GetHashBucketAndPoolIndices(InPtr, BucketIndex, BucketIndexCollision, PoolIndex);

		PoolHashBucket* FirstBucket = &Allocator.HashBuckets[BucketIndex];
		PoolHashBucket* Collision   = FirstBucket;
		do
		{
			if (!Collision->FirstPool)
			{
				Collision->BucketIndex = BucketIndexCollision;
				Collision->FirstPool = CreatePoolArray(Allocator.NumLargePoolsPerPage);
				Collision->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, true);
				return &Collision->FirstPool[PoolIndex];
			}

			if (Collision->BucketIndex == BucketIndexCollision)
			{
				Collision->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, false);
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		}
		while (Collision != FirstBucket);

		// Create a new hash bucket entry
		if (!Allocator.HashBucketFreeList)
		{
			{
				LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
				Allocator.HashBucketFreeList = (PoolHashBucket*)FPlatformMemory::MemoryRangeReserve(FMallocBinned3::OsAllocationGranularity, true);
#if BINNED3_ALLOCATOR_STATS
				Binned3HashMemory += FMallocBinned3::OsAllocationGranularity;
#endif
				if (!Allocator.HashBucketFreeList)
				{
					OutOfMemory(FMallocBinned3::OsAllocationGranularity);
				}
			}

			for (UPTRINT i = 0, n = FMallocBinned3::OsAllocationGranularity / sizeof(PoolHashBucket); i < n; ++i)
			{
				Allocator.HashBucketFreeList->Link(new (Allocator.HashBucketFreeList + i) PoolHashBucket());
			}
		}

		PoolHashBucket* NextFree  = Allocator.HashBucketFreeList->Next;
		PoolHashBucket* NewBucket = Allocator.HashBucketFreeList;

		NewBucket->Unlink();

		if (NextFree == NewBucket)
		{
			NextFree = nullptr;
		}
		Allocator.HashBucketFreeList = NextFree;

		if (!NewBucket->FirstPool)
		{
			NewBucket->FirstPool = CreatePoolArray(Allocator.NumLargePoolsPerPage);
			NewBucket->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, true);
		}
		else
		{
			NewBucket->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, false);
		}

		NewBucket->BucketIndex = BucketIndexCollision;

		FirstBucket->Link(NewBucket);

		return &NewBucket->FirstPool[PoolIndex];
	}

	static FPoolInfoLarge* FindPoolInfo(FMallocBinned3& Allocator, void* InPtr)
	{
		uint32 BucketIndex;
		UPTRINT BucketIndexCollision;
		uint32  PoolIndex;
		Allocator.PtrToPoolMapping.GetHashBucketAndPoolIndices(InPtr, BucketIndex, BucketIndexCollision, PoolIndex);


		PoolHashBucket* FirstBucket = &Allocator.HashBuckets[BucketIndex];
		PoolHashBucket* Collision   = FirstBucket;
		do
		{
			if (Collision->BucketIndex == BucketIndexCollision)
			{
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		}
		while (Collision != FirstBucket);

		return nullptr;
	}

	struct FGlobalRecycler
	{
		bool PushBundle(uint32 InPoolIndex, FBundleNode* InBundle)
		{
			uint32 NumCachedBundles = FMath::Min<uint32>(GMallocBinned3MaxBundlesBeforeRecycle, BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle);
			for (uint32 Slot = 0; Slot < NumCachedBundles; Slot++)
			{
				if (!Bundles[InPoolIndex].FreeBundles[Slot])
				{
					if (!FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Bundles[InPoolIndex].FreeBundles[Slot], InBundle, nullptr))
					{
						return true;
					}
				}
			}
			return false;
		}

		FBundleNode* PopBundle(uint32 InPoolIndex)
		{
			uint32 NumCachedBundles = FMath::Min<uint32>(GMallocBinned3MaxBundlesBeforeRecycle, BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle);
			for (uint32 Slot = 0; Slot < NumCachedBundles; Slot++)
			{
				FBundleNode* Result = Bundles[InPoolIndex].FreeBundles[Slot];
				if (Result)
				{
					if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Bundles[InPoolIndex].FreeBundles[Slot], nullptr, Result) == Result)
					{
						return Result;
					}
				}
			}
			return nullptr;
		}

	private:
		struct FPaddedBundlePointer
		{
			FBundleNode* FreeBundles[BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle];
#define BINNED3_BUNDLE_PADDING (PLATFORM_CACHE_LINE_SIZE - sizeof(FBundleNode*) * BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle)
#if (4 + (4 * PLATFORM_64BITS)) * BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle < PLATFORM_CACHE_LINE_SIZE
			uint8 Padding[BINNED3_BUNDLE_PADDING];
#endif
			FPaddedBundlePointer()
			{
				DefaultConstructItems<FBundleNode*>(FreeBundles, BINNED3_MAX_GMallocBinned3MaxBundlesBeforeRecycle);
			}
		};
		static_assert(sizeof(FPaddedBundlePointer) == PLATFORM_CACHE_LINE_SIZE, "FPaddedBundlePointer should be the same size as a cache line");
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FPaddedBundlePointer Bundles[BINNED3_SMALL_POOL_COUNT] GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);
	};

	static FGlobalRecycler GGlobalRecycler;

	static void FreeBundles(FMallocBinned3& Allocator, FBundleNode* BundlesToRecycle, uint32 InBlockSize, uint32 InPoolIndex)
	{
		FPoolTable& Table = Allocator.SmallPoolTables[InPoolIndex];

		FBundleNode* Bundle = BundlesToRecycle;
		while (Bundle)
		{
			FBundleNode* NextBundle = Bundle->NextBundle;

			FBundleNode* Node = Bundle;
			do
			{
				FBundleNode* NextNode = Node->NextNodeInCurrentBundle;

				uint32 OutBlockOfBlocksIndex;
				void* BasePtrOfNode = Allocator.BlockOfBlocksPointerFromContainedPtr(Node, Allocator.SmallPoolTables[InPoolIndex].PagesPlatformForBlockOfBlocks, OutBlockOfBlocksIndex);
				uint32 BlockWithinIndex = (((uint8*)Node) - ((uint8*)BasePtrOfNode)) / Allocator.SmallPoolTables[InPoolIndex].BlockSize;

				FPoolInfoSmall* NodePoolBlock = Allocator.SmallPoolTables[InPoolIndex].PoolInfos[OutBlockOfBlocksIndex / Allocator.SmallPoolInfosPerPlatformPage];
				if (!NodePoolBlock)
				{
					UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to free an unrecognized small block %p"), Node);
				}
				FPoolInfoSmall* NodePool = &NodePoolBlock[OutBlockOfBlocksIndex % Allocator.SmallPoolInfosPerPlatformPage];

				NodePool->CheckCanary(FPoolInfoSmall::ECanary::SmallAssigned);

				bool bWasExhaused = NodePool->NoFirstFreeIndex;

				// Free a pooled allocation.
				FFreeBlock* Free = (FFreeBlock*)Node;
				Free->NumFreeBlocks = 1;
				Free->NextFreeIndex = NodePool->NoFirstFreeIndex ? MAX_uint32 : NodePool->FirstFreeIndex;
				Free->BlockSizeShifted = (InBlockSize >> BINNED3_MINIMUM_ALIGNMENT_SHIFT);
				Free->Canary = FFreeBlock::CANARY_VALUE;
				Free->PoolIndex = InPoolIndex;
				NodePool->FirstFreeIndex = BlockWithinIndex;
				NodePool->NoFirstFreeIndex = 0;
				check(uint32(NodePool->FirstFreeIndex) == BlockWithinIndex);

				// Free this pool.
				check(NodePool->Taken >= 1);
				if (--NodePool->Taken == 0)
				{
					NodePool->SetCanary(FPoolInfoSmall::ECanary::SmallUnassigned, true, false);
					Table.BlockOfBlockAllocationBits.FreeBit(OutBlockOfBlocksIndex);

					uint64 AllocSize = Allocator.SmallPoolTables[InPoolIndex].PagesPlatformForBlockOfBlocks * Allocator.OsAllocationGranularity;

					if (!bWasExhaused)
					{
						Table.BlockOfBlockIsExhausted.AllocBit(OutBlockOfBlocksIndex);
					}

					verify(FPlatformMemory::MemoryRangeDecommit(BasePtrOfNode, AllocSize));
#if BINNED3_ALLOCATOR_STATS
					Binned3AllocatedOSSmallPoolMemory -= AllocSize;
#endif
				}
				else if (bWasExhaused)
				{
					Table.BlockOfBlockIsExhausted.FreeBit(OutBlockOfBlocksIndex);
				}

				Node = NextNode;
			} while (Node);

			Bundle = NextBundle;
		}
	}


	static FCriticalSection& GetFreeBlockListsRegistrationMutex()
	{
		static FCriticalSection FreeBlockListsRegistrationMutex;
		return FreeBlockListsRegistrationMutex;
	}
	static TArray<FPerThreadFreeBlockLists*>& GetRegisteredFreeBlockLists()
	{
		static TArray<FPerThreadFreeBlockLists*> RegisteredFreeBlockLists;
		return RegisteredFreeBlockLists;
	}
	static void RegisterThreadFreeBlockLists( FPerThreadFreeBlockLists* FreeBlockLists )
	{
		FScopeLock Lock(&GetFreeBlockListsRegistrationMutex());
		GetRegisteredFreeBlockLists().Add(FreeBlockLists);
	}
	static void UnregisterThreadFreeBlockLists( FPerThreadFreeBlockLists* FreeBlockLists )
	{
		FScopeLock Lock(&GetFreeBlockListsRegistrationMutex());
		GetRegisteredFreeBlockLists().Remove(FreeBlockLists);
#if BINNED3_ALLOCATOR_STATS
		FMallocBinned3::FPerThreadFreeBlockLists::ConsolidatedMemory += FreeBlockLists->AllocatedMemory;
#endif
	}
};

FMallocBinned3::Private::FGlobalRecycler FMallocBinned3::Private::GGlobalRecycler;

#if BINNED3_ALLOCATOR_STATS
int64 FMallocBinned3::FPerThreadFreeBlockLists::ConsolidatedMemory = 0;
#endif

void FBitTree::FBitTreeInit(uint32 InDesiredCapacity, uint32 OsAllocationGranularity, bool InitialValue)
{
	DesiredCapacity = InDesiredCapacity;
	AllocationSize = 8;
	Rows = 1;
	uint32 RowsUint64s = 1;
	Capacity = 64;
	OffsetOfLastRow = 0;

	uint32 RowOffsets[10]; // 10 is way more than enough
	RowOffsets[0] = 0;
	uint32 RowNum[10]; // 10 is way more than enough
	RowNum[0] = 1;

	while (Capacity < DesiredCapacity)
	{
		Capacity *= 64;
		RowsUint64s *= 64;
		OffsetOfLastRow = AllocationSize / 8;
		check(Rows < 10);
		RowOffsets[Rows] = OffsetOfLastRow;
		RowNum[Rows] = RowsUint64s;
		AllocationSize += 8 * RowsUint64s;
		Rows++;
	}

	uint32 LastRowTotal = (AllocationSize - OffsetOfLastRow * 8) * 8;
	uint32 ExtraBits = LastRowTotal - DesiredCapacity;
	AllocationSize -= (ExtraBits / 64) * 8;

	int64 AlignedAllocationSize = Align(AllocationSize, OsAllocationGranularity);
	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	Bits = (uint64*)FPlatformMemory::MemoryRangeReserve(AlignedAllocationSize, true);
#if BINNED3_ALLOCATOR_STATS
	Binned3FreeBitsMemory += AlignedAllocationSize;
#endif
	verify(Bits);

	FMemory::Memset(Bits, InitialValue ? 0xff : 0, AllocationSize);

	if (!InitialValue)
	{
		// we fill everything beyond the desired size with occupied
		uint32 ItemsPerBit = 64;
		for (int32 FillRow = Rows - 2; FillRow >= 0; FillRow--)
		{
			uint32 NeededOneBits = RowNum[FillRow] * 64 - (DesiredCapacity + ItemsPerBit - 1) / ItemsPerBit;
			uint32 NeededOne64s = NeededOneBits / 64;
			NeededOneBits %= 64;
			for (uint32 Fill = RowNum[FillRow] - NeededOne64s; Fill < RowNum[FillRow]; Fill++)
			{
				Bits[RowOffsets[FillRow] + Fill] = MAX_uint64;
			}
			if (NeededOneBits)
			{
				Bits[RowOffsets[FillRow] + RowNum[FillRow] - NeededOne64s - 1] = (MAX_uint64 << (64 - NeededOneBits));
			}
			ItemsPerBit *= 64;
		}

		if (DesiredCapacity % 64)
		{
			Bits[AllocationSize / 8 - 1] = (MAX_uint64 << (DesiredCapacity % 64));
		}
	}
}

uint32 FBitTree::AllocBit()
{
	uint32 Result = MAX_uint32;
	if (*Bits != MAX_uint64) // else we are full
	{
		Result = 0;
		uint32 Offset = 0;
		uint32 Row = 0;
		while (true)
		{
			uint64* At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			uint32 LowestZeroBit = FMath::CountTrailingZeros64(~*At);
			check(LowestZeroBit < 64);
			Result = Result * 64 + LowestZeroBit;
			if (Row == Rows - 1)
			{
				check(!((*At) & (1ull << LowestZeroBit))); // this was already allocated?
				*At |= (1ull << LowestZeroBit);
				if (Row > 0)
				{
					if (*At == MAX_uint64)
					{
						do
						{
							uint32 Rem = (Offset - 1) % 64;
							Offset = (Offset - 1) / 64;
							At = Bits + Offset;
							check(At >= Bits && At < Bits + AllocationSize / 8);
							check(*At != MAX_uint64); // this should not already be marked full
							*At |= (1ull << Rem);
							if (*At != MAX_uint64)
							{
								break;
							}
							Row--;
						} while (Row);
					}
				}
				break;
			}
			Offset = Offset * 64 + 1 + LowestZeroBit;
			Row++;
		}
	}

	return Result;
}

void FBitTree::AllocBit(uint32 Index)
{
	check(Index < DesiredCapacity);
	uint32 Row = Rows - 1;
	uint32 Rem = Index % 64;
	uint32 Offset = OffsetOfLastRow + Index / 64;
	uint64* At = Bits + Offset;
	check(At >= Bits && At < Bits + AllocationSize / 8);
	check(!((*At) & (1ull << Rem))); // this was already allocated?
	*At |= (1ull << Rem);
	if (*At == MAX_uint64 && Row > 0)
	{
		do
		{
			Rem = (Offset - 1) % 64;
			Offset = (Offset - 1) / 64;
			At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			check(!((*At) & (1ull << Rem))); // this was already allocated?
			*At |= (1ull << Rem);
			if (*At != MAX_uint64)
			{
				break;
			}
			Row--;
		} while (Row);
	}

}
uint32 FBitTree::NextAllocBit()
{
	uint32 Result = MAX_uint32;
	if (*Bits != MAX_uint64) // else we are full
	{
		Result = 0;
		uint32 Offset = 0;
		uint32 Row = 0;
		while (true)
		{
			uint64* At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			uint32 LowestZeroBit = FMath::CountTrailingZeros64(~*At);
			check(LowestZeroBit < 64);
			Result = Result * 64 + LowestZeroBit;
			if (Row == Rows - 1)
			{
				check(!((*At) & (1ull << LowestZeroBit))); // this was already allocated?
				break;
			}
			Offset = Offset * 64 + 1 + LowestZeroBit;
			Row++;
		}
	}

	return Result;
}


void FBitTree::FreeBit(uint32 Index)
{
	check(Index < DesiredCapacity);
	uint32 Row = Rows - 1;
	uint32 Rem = Index % 64;
	uint32 Offset = OffsetOfLastRow + Index / 64;
	uint64* At = Bits + Offset;
	check(At >= Bits && At < Bits + AllocationSize / 8);
	bool bWasFull = *At == MAX_uint64;
	check((*At) & (1ull << Rem)); // this was not already allocated?
	*At &= ~(1ull << Rem);
	if (bWasFull && Row > 0)
	{
		do
		{
			Rem = (Offset - 1) % 64;
			Offset = (Offset - 1) / 64;
			At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			bWasFull = *At == MAX_uint64;
			*At &= ~(1ull << Rem);
			if (!bWasFull)
			{
				break;
			}
			Row--;
		} while (Row);
	}
}

uint32 FBitTree::CountOnes(uint32 UpTo)
{
	uint32 Result = 0;
	uint64* At = Bits + OffsetOfLastRow;
	while (UpTo >= 64)
	{
		Result += FMath::CountBits(*At);
		At++;
		UpTo -= 64;
	}
	if (UpTo)
	{
		Result += FMath::CountBits((*At) << (64 - UpTo));
	}
	return Result;
}

void FBitTree::Test()
{
#if 0
	int32 Sizes[] =
	{
		1,
		63,
		64,
		65,
		64 * 64 - 1,
		64 * 64,
		64 * 64 + 1,
		64 * 64 * 64 - 99,
		64 * 64 * 64 + 99,

		1024 * 1024 * 1024 / (1 * 4096),
		1024 * 1024 * 1024 / (2 * 4096),
		1024 * 1024 * 1024 / (3 * 4096),
		1024 * 1024 * 1024 / (4 * 4096),
		1024 * 1024 * 1024 / (5 * 4096),
		1024 * 1024 * 1024 / (6 * 4096),
		1024 * 1024 * 1024 / (7 * 4096),

		1024 * 1024 * 1024 / (1 * 16384),
		1024 * 1024 * 1024 / (2 * 16384),
		1024 * 1024 * 1024 / (3 * 16384),
		1024 * 1024 * 1024 / (4 * 16384),
		1024 * 1024 * 1024 / (5 * 16384),
		1024 * 1024 * 1024 / (6 * 16384),
		1024 * 1024 * 1024 / (7 * 16384)

	};

	{
		for (int i = 0; i < ARRAY_COUNT(Sizes); i++)
		{
			FBitTree Tree;
			Tree.FBitTreeInit(Sizes[i], (i & 1) ? 4096 : 16384, false);

			for (int k = 0; k < Sizes[i]; k++)
			{
				uint32 TestI = Tree.NextAllocBit();
				check(TestI == k);
				check(Tree.AllocBit() == k);
			}
			check(Tree.NextAllocBit() == MAX_uint32);
			check(Tree.AllocBit() == MAX_uint32);
			Tree.FreeBit(0);
			check(Tree.NextAllocBit() == 0);
			check(Tree.AllocBit() == 0);

			Tree.FreeBit(Sizes[i] - 1);
			check(Tree.NextAllocBit() == Sizes[i] - 1);
			check(Tree.AllocBit() == Sizes[i] - 1);

			for (int k = 0; k < Sizes[i] - 1; k++)
			{
				Tree.FreeBit(k);
			}
			Tree.FreeBit(Sizes[i] - 1);

			TArray<TPair<float, int32>> Pairs;
			for (int k = 0; k < Sizes[i]; k++)
			{
				check(Tree.NextAllocBit() == k);
				check(Tree.AllocBit() == k);
				Pairs.Add(TPair<float, int32>(FMath::FRand(), k));
			}
			check(Tree.AllocBit() == MAX_uint32);
			Pairs.Sort();
			for (auto& p : Pairs)
			{
				Tree.FreeBit(p.Value);
			}
			Pairs.Empty();
			for (int k = 0; k < Sizes[i]; k++)
			{
				check(Tree.NextAllocBit() == k);
				check(Tree.AllocBit() == k);
				Pairs.Add(TPair<float, int32>(FMath::FRand(), k));
			}
			check(Tree.NextAllocBit() == MAX_uint32);
			check(Tree.AllocBit() == MAX_uint32);
			Pairs.Sort();
			for (auto& p : Pairs)
			{
				Tree.FreeBit(p.Value);
				uint32 Temp = Tree.AllocBit();
				Tree.FreeBit(Temp);
			}
		}
	}
	{
		for (int i = 0; i < ARRAY_COUNT(Sizes); i++)
		{
			FBitTree Tree;
			Tree.FBitTreeInit(Sizes[i], (i & 1) ? 4096 : 16384, false);

			for (int k = 0; k < Sizes[i]; k++)
			{
				uint32 TestI = Tree.NextAllocBit();
				Tree.AllocBit(TestI);
			}
			check(Tree.NextAllocBit() == MAX_uint32);
			check(Tree.AllocBit() == MAX_uint32);
			Tree.FreeBit(0);
			check(Tree.NextAllocBit() == 0);
			Tree.AllocBit(0);

			Tree.FreeBit(Sizes[i] - 1);
			check(Tree.NextAllocBit() == Sizes[i] - 1);
			Tree.AllocBit(Sizes[i] - 1);

			for (int k = 0; k < Sizes[i] - 1; k++)
			{
				Tree.FreeBit(k);
			}
			Tree.FreeBit(Sizes[i] - 1);

			TArray<TPair<float, int32>> Pairs;
			for (int k = 0; k < Sizes[i]; k++)
			{
				check(Tree.NextAllocBit() == k);
				Tree.AllocBit(k);
				Pairs.Add(TPair<float, int32>(FMath::FRand(), k));
			}
			check(Tree.AllocBit() == MAX_uint32);
			Pairs.Sort();
			for (auto& p : Pairs)
			{
				Tree.FreeBit(p.Value);
			}
			Pairs.Empty();
			for (int k = 0; k < Sizes[i]; k++)
			{
				check(Tree.NextAllocBit() == k);
				Tree.AllocBit(k);
				Pairs.Add(TPair<float, int32>(FMath::FRand(), k));
			}
			check(Tree.NextAllocBit() == MAX_uint32);
			check(Tree.AllocBit() == MAX_uint32);
			Pairs.Sort();
			for (auto& p : Pairs)
			{
				Tree.FreeBit(p.Value);
				uint32 Temp = Tree.AllocBit();
				Tree.FreeBit(Temp);
			}
		}
	}
#endif
}

FMallocBinned3::FPoolInfoSmall* FMallocBinned3::PushNewPoolToFront(FMallocBinned3::FPoolTable& Table, uint32 InBlockSize, uint32 InPoolIndex, uint32& OutBlockOfBlocksIndex)
{
	const uint32 BlockOfBlocksSize = OsAllocationGranularity * Table.PagesPlatformForBlockOfBlocks;

	// Allocate memory.

	uint32 BlockOfBlocksIndex = Table.BlockOfBlockAllocationBits.AllocBit();
	if (BlockOfBlocksIndex == MAX_uint32)
	{
		Private::OutOfMemory(InBlockSize + 1); // The + 1 will hopefully be a hint that we actually ran out of our 1GB space.
	}
	uint8* FreePtr = BlockPointerFromIndecies(InPoolIndex, BlockOfBlocksIndex, BlockOfBlocksSize);

	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	if (!FPlatformMemory::MemoryRangeCommit(FreePtr, BlockOfBlocksSize))
	{
		Private::OutOfMemory(BlockOfBlocksSize);
	}
	uint64 EndOffset = UPTRINT(FreePtr + BlockOfBlocksSize) - UPTRINT(PoolBasePtr(InPoolIndex));
	if (EndOffset > Table.UnusedAreaOffsetLow)
	{
		Table.UnusedAreaOffsetLow = EndOffset;
	}
	FFreeBlock* Free = new ((void*)FreePtr) FFreeBlock(BlockOfBlocksSize, InBlockSize, InPoolIndex);
#if BINNED3_ALLOCATOR_STATS
	Binned3AllocatedOSSmallPoolMemory += (int64)BlockOfBlocksSize;
#endif
	check(IsAligned(Free, OsAllocationGranularity));
	// Create pool
	FPoolInfoSmall* Result = Private::GetOrCreatePoolInfoSmall(*this, InPoolIndex, BlockOfBlocksIndex);
	Result->CheckCanary(FPoolInfoSmall::ECanary::SmallAssigned);
	Result->Taken = 0;
	Result->FirstFreeIndex = 0;
	Result->NoFirstFreeIndex = 0;
	Table.BlockOfBlockIsExhausted.FreeBit(BlockOfBlocksIndex);

	OutBlockOfBlocksIndex = BlockOfBlocksIndex;

	return Result;
}

FMallocBinned3::FPoolInfoSmall* FMallocBinned3::GetFrontPool(FPoolTable& Table, uint32 InPoolIndex, uint32& OutBlockOfBlocksIndex)
{
	OutBlockOfBlocksIndex = Table.BlockOfBlockIsExhausted.NextAllocBit();
	if (OutBlockOfBlocksIndex == MAX_uint32)
	{
		return nullptr;
	}
	return Private::GetOrCreatePoolInfoSmall(*this, InPoolIndex, OutBlockOfBlocksIndex);
}


FMallocBinned3::FMallocBinned3()
	: HashBucketFreeList(nullptr)
{
	static bool bOnce = false;
	check(!bOnce); // this is now a singleton-like thing and you cannot make multiple copies
	bOnce = true;

	check(!PLATFORM_32BITS);

	FGenericPlatformMemoryConstants Constants = FPlatformMemory::GetConstants();
	OsAllocationGranularity = Constants.BinnedAllocationGranularity ? Constants.BinnedAllocationGranularity : Constants.PageSize;
	MaxAlignmentForMemoryRangeReserve = Constants.OsAllocationGranularity;
	NumLargePoolsPerPage = OsAllocationGranularity / sizeof(FPoolInfoLarge);
	check(OsAllocationGranularity % sizeof(FPoolInfoLarge) == 0);  // these need to divide evenly!
	PtrToPoolMapping.Init(OsAllocationGranularity, NumLargePoolsPerPage, Constants.AddressLimit);

	checkf(FMath::IsPowerOfTwo(OsAllocationGranularity), TEXT("OS page size must be a power of two"));
	checkf(FMath::IsPowerOfTwo(Constants.AddressLimit), TEXT("OS address limit must be a power of two"));
	checkf(Constants.AddressLimit > OsAllocationGranularity, TEXT("OS address limit must be greater than the page size")); // Check to catch 32 bit overflow in AddressLimit
	checkf(sizeof(FMallocBinned3::FFreeBlock) <= Binned3SmallBlockSizes4k[0], TEXT("Pool header must be able to fit into the smallest block"));
	static_assert(BINNED3_SMALL_POOL_COUNT <= 256, "Small block size array size must fit in a byte");
	static_assert(sizeof(FFreeBlock) <= BINNED3_MINIMUM_ALIGNMENT, "Free block struct must be small enough to fit into a block.");

	// Init pool tables.

	FillSizeTable(OsAllocationGranularity);
	checkf(SizeTable[BINNED3_SMALL_POOL_COUNT - 1].BlockSize == BINNED3_MAX_SMALL_POOL_SIZE, TEXT("BINNED3_MAX_SMALL_POOL_SIZE must equal the largest block size"));

	SmallPoolInfosPerPlatformPage = OsAllocationGranularity / sizeof(FPoolInfoSmall);

	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		checkf(Index == 0 || SizeTable[Index - 1].BlockSize < SizeTable[Index].BlockSize, TEXT("Small block sizes must be strictly increasing"));
		checkf(SizeTable[Index].BlockSize % BINNED3_MINIMUM_ALIGNMENT == 0, TEXT("Small block size must be a multiple of BINNED3_MINIMUM_ALIGNMENT"));

		SmallPoolTables[Index].BlockSize = SizeTable[Index].BlockSize;
		SmallPoolTables[Index].BlocksPerBlockOfBlocks = SizeTable[Index].BlocksPerBlockOfBlocks;
		SmallPoolTables[Index].PagesPlatformForBlockOfBlocks = SizeTable[Index].PagesPlatformForBlockOfBlocks;

		SmallPoolTables[Index].UnusedAreaOffsetLow = 0;
		SmallPoolTables[Index].NumEverUsedBlockOfBlocks = 0;
#if BINNED3_ALLOCATOR_PER_BIN_STATS
		SmallPoolTables[Index].TotalRequestedAllocSize.Store(0);
		SmallPoolTables[Index].TotalAllocCount.Store(0);
		SmallPoolTables[Index].TotalFreeCount.Store(0);
#endif

		int64 TotalNumberOfBlocksOfBlocks = MAX_MEMORY_PER_BLOCK_SIZE / (SizeTable[Index].PagesPlatformForBlockOfBlocks * OsAllocationGranularity);

		int64 MaxPoolInfoMemory = Align(sizeof(FPoolInfoSmall**) * (TotalNumberOfBlocksOfBlocks + SmallPoolInfosPerPlatformPage - 1) / SmallPoolInfosPerPlatformPage, OsAllocationGranularity);
		SmallPoolTables[Index].PoolInfos = (FPoolInfoSmall**)FPlatformMemory::MemoryRangeReserve(MaxPoolInfoMemory, true);
		FMemory::Memzero(SmallPoolTables[Index].PoolInfos, MaxPoolInfoMemory);
#if BINNED3_ALLOCATOR_STATS
		Binned3PoolInfoMemory += MaxPoolInfoMemory;
#endif

		SmallPoolTables[Index].BlockOfBlockAllocationBits.FBitTreeInit(TotalNumberOfBlocksOfBlocks, OsAllocationGranularity, false);
		SmallPoolTables[Index].BlockOfBlockIsExhausted.FBitTreeInit(TotalNumberOfBlocksOfBlocks, OsAllocationGranularity, true);
	}


	// Set up pool mappings
	uint8* IndexEntry = MemSizeToIndex;
	uint32  PoolIndex  = 0;
	for (uint32 Index = 0; Index != 1 + (BINNED3_MAX_SMALL_POOL_SIZE >> BINNED3_MINIMUM_ALIGNMENT_SHIFT); ++Index)
	{
		
		uint32 BlockSize = Index << BINNED3_MINIMUM_ALIGNMENT_SHIFT; // inverse of int32 Index = int32((Size >> BINNED3_MINIMUM_ALIGNMENT_SHIFT));
		while (SizeTable[PoolIndex].BlockSize < BlockSize)
		{
			++PoolIndex;
			check(PoolIndex != BINNED3_SMALL_POOL_COUNT);
		}
		check(PoolIndex < 256);
		*IndexEntry++ = uint8(PoolIndex);
	}
	// now reverse the pool sizes for cache coherency

	for (uint32 Index = 0; Index != BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		uint32 Partner = BINNED3_SMALL_POOL_COUNT - Index - 1;
		SmallBlockSizesReversedShifted[Index] = (SizeTable[Partner].BlockSize >> BINNED3_MINIMUM_ALIGNMENT_SHIFT);
	}
	uint64 MaxHashBuckets = PtrToPoolMapping.GetMaxHashBuckets();

	{
		LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
		int64 HashAllocSize = Align(MaxHashBuckets * sizeof(PoolHashBucket), OsAllocationGranularity);
		HashBuckets = (PoolHashBucket*)FPlatformMemory::MemoryRangeReserve(HashAllocSize, true);
#if BINNED3_ALLOCATOR_STATS
		Binned3HashMemory += HashAllocSize;
#endif
		verify(HashBuckets);
	}

	DefaultConstructItems<PoolHashBucket>(HashBuckets, MaxHashBuckets);
	MallocBinned3 = this;
	GFixedMallocLocationPtr = (FMalloc**)(&MallocBinned3);

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMPtr = (uint8*)FPlatformMemory::MemoryRangeReserve(BINNED3_SMALL_POOL_COUNT * MAX_MEMORY_PER_BLOCK_SIZE);
	verify(Binned3BaseVMPtr);
#else

	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT; ++Index)
	{
		uint8* NewVM = (uint8*)FPlatformMemory::MemoryRangeReserve(MAX_MEMORY_PER_BLOCK_SIZE);
		// insertion sort
		if (Index && NewVM < PoolBaseVMPtr[Index - 1])
		{
			uint32 InsertIndex = 0;
			for (; InsertIndex < Index; ++InsertIndex)
			{
				if (NewVM < PoolBaseVMPtr[InsertIndex])
				{
					break;
				}
			}
			check(InsertIndex < Index);
			for (uint32 MoveIndex = Index; MoveIndex > InsertIndex; --MoveIndex)
			{
				PoolBaseVMPtr[MoveIndex] = PoolBaseVMPtr[MoveIndex - 1];
			}
			PoolBaseVMPtr[InsertIndex] = NewVM;
		}
		else
		{
			PoolBaseVMPtr[Index] = NewVM;
		}
	}
	HighestPoolBaseVMPtr = PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT - 1];
	uint64 TotalGaps = 0;
	for (uint32 Index = 0; Index < BINNED3_SMALL_POOL_COUNT - 1; ++Index)
	{
		check(PoolBaseVMPtr[Index + 1] > PoolBaseVMPtr[Index]); // we sorted it
		check(PoolBaseVMPtr[Index + 1] >= PoolBaseVMPtr[Index] + MAX_MEMORY_PER_BLOCK_SIZE); // and blocks are non-overlapping
		TotalGaps += PoolBaseVMPtr[Index + 1] - (PoolBaseVMPtr[Index] + MAX_MEMORY_PER_BLOCK_SIZE);
	}
	if (TotalGaps == 0)
	{
		PoolSearchDiv = 0;
	}
	else if (TotalGaps < MAX_MEMORY_PER_BLOCK_SIZE)
	{
		PoolSearchDiv = MAX_MEMORY_PER_BLOCK_SIZE; // the gaps are not significant, ignoring them should give accurate searches
	}
	else
	{
		PoolSearchDiv = MAX_MEMORY_PER_BLOCK_SIZE + ((TotalGaps + BINNED3_SMALL_POOL_COUNT - 2) / (BINNED3_SMALL_POOL_COUNT - 1));
	}
#endif
}

FMallocBinned3::~FMallocBinned3()
{
}

bool FMallocBinned3::IsInternallyThreadSafe() const
{ 
	return true;
}

void* FMallocBinned3::MallocExternal(SIZE_T Size, uint32 Alignment)
{
	static_assert(DEFAULT_ALIGNMENT <= BINNED3_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below

	// Only allocate from the small pools if the size is small enough and the alignment isn't crazy large.
	// With large alignments, we'll waste a lot of memory allocating an entire page, but such alignments are highly unlikely in practice.
	if ((Size <= BINNED3_MAX_SMALL_POOL_SIZE) & (Alignment <= BINNED3_MINIMUM_ALIGNMENT)) // one branch, not two
	{
		uint32 PoolIndex = BoundSizeToPoolIndex(Size);
		FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
		if (Lists)
		{
			if (Lists->ObtainRecycledPartial(PoolIndex))
			{
				if (void* Result = Lists->Malloc(PoolIndex))
				{
#if BINNED3_ALLOCATOR_STATS
					SmallPoolTables[PoolIndex].HeadEndAlloc(Size);
					uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);
					Lists->AllocatedMemory += BlockSize;
#endif
					return Result;
				}
			}
		}

		FScopeLock Lock(&Mutex);

		// Allocate from small object pool.
		FPoolTable& Table = SmallPoolTables[PoolIndex];

		uint32 BlockOfBlocksIndex = MAX_uint32;
		FPoolInfoSmall* Pool = GetFrontPool(Table, PoolIndex, BlockOfBlocksIndex);
		if (!Pool)
		{
			Pool = PushNewPoolToFront(Table, Table.BlockSize, PoolIndex, BlockOfBlocksIndex);
		}

		const uint32 BlockOfBlocksSize = OsAllocationGranularity * Table.PagesPlatformForBlockOfBlocks;
		uint8* BlockOfBlocksPtr = BlockPointerFromIndecies(PoolIndex, BlockOfBlocksIndex, BlockOfBlocksSize);

		void* Result = Pool->AllocateRegularBlock(BlockOfBlocksPtr, Table.BlockSize);
#if BINNED3_ALLOCATOR_STATS
		Table.HeadEndAlloc(Size);
		Binned3AllocatedSmallPoolMemory += PoolIndexToBlockSize(PoolIndex);
#endif // BINNED3_ALLOCATOR_STATS
		if (GMallocBinned3AllocExtra)
		{
			if (Lists)
			{
				// prefill the free list with some allocations so we are less likely to hit this slow path with the mutex 
				for (int32 Index = 0; Index < GMallocBinned3AllocExtra && Pool->HasFreeRegularBlock(); Index++)
				{
					if (!Lists->Free(Result, PoolIndex, Table.BlockSize))
					{
						break;
					}
					Result = Pool->AllocateRegularBlock(BlockOfBlocksPtr, Table.BlockSize);
				}
			}
		}
		if (!Pool->HasFreeRegularBlock())
		{
			Table.BlockOfBlockIsExhausted.AllocBit(BlockOfBlocksIndex);
		}

		return Result;
	}
	Alignment = FMath::Max<uint32>(Alignment, BINNED3_MINIMUM_ALIGNMENT);
	Size = Align(FMath::Max((SIZE_T)1, Size), Alignment);

	check(FMath::IsPowerOfTwo(Alignment));
	UE_CLOG(Alignment > MaxAlignmentForMemoryRangeReserve ,LogMemory, Fatal, TEXT("Requested alignment was too large for OS. Alignment=%dKB MaxAlignmentForMemoryRangeReserve=%dKB"), Alignment/1024, MaxAlignmentForMemoryRangeReserve/1024);

	FScopeLock Lock(&Mutex); 

	// Use OS for non-pooled allocations.
	UPTRINT AlignedSize = Align(Size, FMath::Max(OsAllocationGranularity,Alignment));

#if BINNED3_TIME_LARGE_BLOCKS
	double StartTime = FPlatformTime::Seconds();
#endif

	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	void* Result = FPlatformMemory::MemoryRangeReserve(AlignedSize, true);

#if BINNED3_TIME_LARGE_BLOCKS
	double Add = FPlatformTime::Seconds() - StartTime;
	double Old;
	do
	{
		Old = MemoryRangeReserveTotalTime.Load();
	} while (!MemoryRangeReserveTotalTime.CompareExchange(Old, Old + Add));
	MemoryRangeReserveTotalCount++;
#endif

	UE_CLOG(!IsAligned(Result, Alignment) ,LogMemory, Fatal, TEXT("FMallocBinned3 alignment was too large for OS. Alignment=%d   Ptr=%p"), Alignment, Result);

	if (!Result)
	{
		Private::OutOfMemory(AlignedSize);
	}
	check(IsOSAllocation(Result));

#if BINNED3_ALLOCATOR_STATS
	Binned3AllocatedLargePoolMemory += (int64)Size;
	Binned3AllocatedLargePoolMemoryWAlignment += AlignedSize;
#endif

	// Create pool.
	FPoolInfoLarge* Pool = Private::GetOrCreatePoolInfoLarge(*this, Result);
	check(Size > 0 && Size <= AlignedSize && AlignedSize >= OsAllocationGranularity);
	Pool->SetOSAllocationSizes(Size, AlignedSize);

	return Result;
}


void* FMallocBinned3::ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment)
{
	if (NewSize == 0)
	{
		FMallocBinned3::FreeExternal(Ptr);
		return nullptr;
	}
	static_assert(DEFAULT_ALIGNMENT <= BINNED3_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below
	check(FMath::IsPowerOfTwo(Alignment));
	check(Alignment <= OsAllocationGranularity);

	uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);
		if (
			((NewSize <= BlockSize) & (Alignment <= BINNED3_MINIMUM_ALIGNMENT)) && // one branch, not two
			(PoolIndex == 0 || NewSize > PoolIndexToBlockSize(PoolIndex - 1)))
		{
#if BINNED3_ALLOCATOR_STATS
			SmallPoolTables[PoolIndex].HeadEndAlloc(NewSize);
			SmallPoolTables[PoolIndex].HeadEndFree();
#endif
			return Ptr;
		}

		// Reallocate and copy the data across
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		FMemory::Memcpy(Result, Ptr, FMath::Min<SIZE_T>(NewSize, BlockSize));
		FMallocBinned3::FreeExternal(Ptr);
		return Result;
	}
	if (!Ptr)
	{
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		return Result;
	}

	Mutex.Lock();

	// Allocated from OS.
	FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
	if (!Pool)
	{
		UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to realloc an unrecognized block %p"), Ptr);
	}
	UPTRINT PoolOsBytes = Pool->GetOsAllocatedBytes();
	uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
	checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::ReallocExternal %d %d"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
	if (NewSize > PoolOsBytes || // can't fit in the old block
		(NewSize <= BINNED3_MAX_SMALL_POOL_SIZE && Alignment <= BINNED3_MINIMUM_ALIGNMENT) || // can switch to the small block allocator
		Align(NewSize, OsAllocationGranularity) < PoolOsBytes) // we can get some pages back
	{
		// Grow or shrink.
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		SIZE_T CopySize = FMath::Min<SIZE_T>(NewSize, PoolOSRequestedBytes);
		if (CopySize > 4096) // don't release the lock for small stuff
		{
			Mutex.Unlock();
		}
		FMemory::Memcpy(Result, Ptr, CopySize);
		FMallocBinned3::FreeExternal(Ptr);
		if (CopySize <= 4096) // Release here for small stuff
		{
			Mutex.Unlock();
		}
		return Result;
	}

#if BINNED3_ALLOCATOR_STATS
	Binned3AllocatedLargePoolMemory += ((int64)NewSize) - ((int64)Pool->GetOSRequestedBytes());
	// don't need to change the Binned3AllocatedLargePoolMemoryWAlignment because we didn't reallocate so it's the same size
#endif
	
	Pool->SetOSAllocationSizes(NewSize, PoolOsBytes);
	Mutex.Unlock();
	return Ptr;
}

void FMallocBinned3::FreeExternal(void* Ptr)
{
	uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		uint32 BlockSize = PoolIndexToBlockSize(PoolIndex);

		FBundleNode* BundlesToRecycle = nullptr;
		FPerThreadFreeBlockLists* Lists = GMallocBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
		if (Lists)
		{
			BundlesToRecycle = Lists->RecycleFullBundle(PoolIndex);
			bool bPushed = Lists->Free(Ptr, PoolIndex, BlockSize);
			check(bPushed);
#if BINNED3_ALLOCATOR_STATS
			SmallPoolTables[PoolIndex].HeadEndFree();
			Lists->AllocatedMemory -= BlockSize;
#endif
		}
		else
		{
			BundlesToRecycle = (FBundleNode*)Ptr;
			BundlesToRecycle->NextNodeInCurrentBundle = nullptr;
		}
		if (BundlesToRecycle)
		{
			BundlesToRecycle->NextBundle = nullptr;
			FScopeLock Lock(&Mutex);
			Private::FreeBundles(*this, BundlesToRecycle, BlockSize, PoolIndex);
#if BINNED3_ALLOCATOR_STATS
			if (!Lists)
			{
				SmallPoolTables[PoolIndex].HeadEndFree();
				// lists track their own stat track them instead in the global stat if we don't have lists
				Binned3AllocatedSmallPoolMemory -= ((int64)(BlockSize));
			}
#endif
		}
	}
	else if (Ptr)
	{
		FScopeLock Lock(&Mutex);
		FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
		if (!Pool)
		{
			UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to free an unrecognized block %p"), Ptr);
		}
		UPTRINT PoolOsBytes = Pool->GetOsAllocatedBytes();
		uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();

#if BINNED3_ALLOCATOR_STATS
		Binned3AllocatedLargePoolMemory -= ((int64)PoolOSRequestedBytes);
		Binned3AllocatedLargePoolMemoryWAlignment -= ((int64)PoolOsBytes);
#endif

		checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::FreeExternal %d %d"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
		Pool->SetCanary(FPoolInfoLarge::ECanary::LargeUnassigned, true, false);
		// Free an OS allocation.
#if BINNED3_TIME_LARGE_BLOCKS
		double StartTime = FPlatformTime::Seconds();
#endif
		FPlatformMemory::MemoryRangeFree(Ptr, PoolOsBytes);
#if BINNED3_TIME_LARGE_BLOCKS
		double Add = FPlatformTime::Seconds() - StartTime;
		double Old;
		do
		{
			Old = MemoryRangeFreeTotalTime.Load();
		} while (!MemoryRangeFreeTotalTime.CompareExchange(Old, Old + Add));
		MemoryRangeFreeTotalCount++;
#endif
	}
}

bool FMallocBinned3::GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut)
{
	uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < BINNED3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		SizeOut = PoolIndexToBlockSize(PoolIndex);
		return true;
	}
	if (!Ptr)
	{
		return false;
	}
	FScopeLock Lock(&Mutex);
	FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
	if (!Pool)
	{
		UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to GetAllocationSizeExternal an unrecognized block %p"), Ptr);
	}
	UPTRINT PoolOsBytes = Pool->GetOsAllocatedBytes();
	uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
	checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::GetAllocationSizeExternal %d %d"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
	SizeOut = PoolOsBytes;
	return true;
}

bool FMallocBinned3::ValidateHeap()
{
	// Not implemented
	// NumEverUsedBlockOfBlocks gives us all of the information we need to examine each pool, so it is doable.
	return true;
}

const TCHAR* FMallocBinned3::GetDescriptiveName()
{
	return TEXT("Binned3");
}

void FMallocBinned3::FlushCurrentThreadCache()
{
	double StartTimeInner = FPlatformTime::Seconds();
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FMallocBinned3_FlushCurrentThreadCache);
	FPerThreadFreeBlockLists* Lists = FPerThreadFreeBlockLists::Get();

	float WaitForMutexTime = 0.0f;
	float WaitForMutexAndTrimTime = 0.0f;

	if (Lists)
	{
		FScopeLock Lock(&Mutex);
		WaitForMutexTime = FPlatformTime::Seconds() - StartTimeInner;
		for (int32 PoolIndex = 0; PoolIndex != BINNED3_SMALL_POOL_COUNT; ++PoolIndex)
		{
			FBundleNode* Bundles = Lists->PopBundles(PoolIndex);
			if (Bundles)
			{
				Private::FreeBundles(*this, Bundles, PoolIndexToBlockSize(PoolIndex), PoolIndex);
			}
		}
		WaitForMutexAndTrimTime = FPlatformTime::Seconds() - StartTimeInner;
	}

	// These logs must happen outside the above mutex to avoid deadlocks
	if (WaitForMutexTime > 0.02f)
	{
		UE_LOG(LogMemory, Warning, TEXT("FMallocBinned3 took %6.2fms to wait for mutex for trim."), WaitForMutexTime * 1000.0f);
	}
	if (WaitForMutexAndTrimTime > 0.02f)
	{
		UE_LOG(LogMemory, Warning, TEXT("FMallocBinned3 took %6.2fms to wait for mutex AND trim."), WaitForMutexAndTrimTime * 1000.0f);
	}
}

#include "Async/TaskGraphInterfaces.h"

void FMallocBinned3::Trim(bool bTrimThreadCaches)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FMallocBinned3_Trim);

	if (GMallocBinned3PerThreadCaches  &&  bTrimThreadCaches)
	{
		//double StartTime = FPlatformTime::Seconds();
		TFunction<void(ENamedThreads::Type CurrentThread)> Broadcast =
			[this](ENamedThreads::Type MyThread)
		{
			FlushCurrentThreadCache();
		};
		// Skip task threads on desktop platforms as it is too slow and they don't have much memory
		FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(!PLATFORM_DESKTOP, false, Broadcast);
		//UE_LOG(LogTemp, Display, TEXT("Trim Broadcast = %6.2fms"), 1000.0f * float(FPlatformTime::Seconds() - StartTime));
	}
}

void FMallocBinned3::SetupTLSCachesOnCurrentThread()
{
	if (!BINNED3_ALLOW_RUNTIME_TWEAKING && !GMallocBinned3PerThreadCaches)
	{
		return;
	}
	if (!FMallocBinned3::Binned3TlsSlot)
	{
		FMallocBinned3::Binned3TlsSlot = FPlatformTLS::AllocTlsSlot();
	}
	check(FMallocBinned3::Binned3TlsSlot);
	FPerThreadFreeBlockLists::SetTLS();
}

void FMallocBinned3::ClearAndDisableTLSCachesOnCurrentThread()
{
	FlushCurrentThreadCache();
	FPerThreadFreeBlockLists::ClearTLS();
}


bool FMallocBinned3::FFreeBlockList::ObtainPartial(uint32 InPoolIndex)
{
	if (!PartialBundle.Head)
	{
		PartialBundle.Count = 0;
		PartialBundle.Head = FMallocBinned3::Private::GGlobalRecycler.PopBundle(InPoolIndex);
		if (PartialBundle.Head)
		{
			PartialBundle.Count = PartialBundle.Head->Count;
			PartialBundle.Head->NextBundle = nullptr;
			return true;
		}
		return false;
	}
	return true;
}

FMallocBinned3::FBundleNode* FMallocBinned3::FFreeBlockList::RecyleFull(uint32 InPoolIndex)
{
	FMallocBinned3::FBundleNode* Result = nullptr;
	if (FullBundle.Head)
	{
		FullBundle.Head->Count = FullBundle.Count;
		if (!FMallocBinned3::Private::GGlobalRecycler.PushBundle(InPoolIndex, FullBundle.Head))
		{
			Result = FullBundle.Head;
			Result->NextBundle = nullptr;
		}
		FullBundle.Reset();
	}
	return Result;
}

FMallocBinned3::FBundleNode* FMallocBinned3::FFreeBlockList::PopBundles(uint32 InPoolIndex)
{
	FBundleNode* Partial = PartialBundle.Head;
	if (Partial)
	{
		PartialBundle.Reset();
		Partial->NextBundle = nullptr;
	}

	FBundleNode* Full = FullBundle.Head;
	if (Full)
	{
		FullBundle.Reset();
		Full->NextBundle = nullptr;
	}

	FBundleNode* Result = Partial;
	if (Result)
	{
		Result->NextBundle = Full;
	}
	else
	{
		Result = Full;
	}

	return Result;
}

void FMallocBinned3::FPerThreadFreeBlockLists::SetTLS()
{
	check(FMallocBinned3::Binned3TlsSlot);
	FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(FMallocBinned3::Binned3TlsSlot);
	if (!ThreadSingleton)
	{
		LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
		int64 TLSSize = Align(sizeof(FPerThreadFreeBlockLists), FMallocBinned3::OsAllocationGranularity);
		ThreadSingleton = new (FPlatformMemory::MemoryRangeReserve(TLSSize, true)) FPerThreadFreeBlockLists();
#if BINNED3_ALLOCATOR_STATS
		Binned3TLSMemory += TLSSize;
#endif
		verify(ThreadSingleton);
		FPlatformTLS::SetTlsValue(FMallocBinned3::Binned3TlsSlot, ThreadSingleton);
		FMallocBinned3::Private::RegisterThreadFreeBlockLists(ThreadSingleton);
	}
}

void FMallocBinned3::FPerThreadFreeBlockLists::ClearTLS()
{
	check(FMallocBinned3::Binned3TlsSlot);
	FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(FMallocBinned3::Binned3TlsSlot);
	if ( ThreadSingleton )
	{
		FMallocBinned3::Private::UnregisterThreadFreeBlockLists(ThreadSingleton);
	}
	FPlatformTLS::SetTlsValue(FMallocBinned3::Binned3TlsSlot, nullptr);
}

void FMallocBinned3::FFreeBlock::CanaryFail() const
{
	UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to realloc an unrecognized block %p   canary == 0x%x != 0x%x"), (void*)this, (int32)Canary, (int32)FMallocBinned3::FFreeBlock::CANARY_VALUE);
}

#if BINNED3_ALLOCATOR_STATS
int64 FMallocBinned3::GetTotalAllocatedSmallPoolMemory() const
{
	int64 FreeBlockAllocatedMemory = 0;
	{
		FScopeLock Lock(&Private::GetFreeBlockListsRegistrationMutex());
		for (const FPerThreadFreeBlockLists* FreeBlockLists : Private::GetRegisteredFreeBlockLists())
		{
			FreeBlockAllocatedMemory += FreeBlockLists->AllocatedMemory;
		}
		FreeBlockAllocatedMemory += FPerThreadFreeBlockLists::ConsolidatedMemory;
	}

	return Binned3AllocatedSmallPoolMemory + FreeBlockAllocatedMemory;
}
#endif

void FMallocBinned3::GetAllocatorStats( FGenericMemoryStats& OutStats )
{
#if BINNED3_ALLOCATOR_STATS

	int64 TotalAllocatedSmallPoolMemory = GetTotalAllocatedSmallPoolMemory();

	OutStats.Add(TEXT("Binned3AllocatedSmallPoolMemory"), TotalAllocatedSmallPoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedOSSmallPoolMemory"), Binned3AllocatedOSSmallPoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedLargePoolMemory"), Binned3AllocatedLargePoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedLargePoolMemoryWAlignment"), Binned3AllocatedLargePoolMemoryWAlignment);

	uint64 TotalAllocated = TotalAllocatedSmallPoolMemory + Binned3AllocatedLargePoolMemory;
	uint64 TotalOSAllocated = Binned3AllocatedOSSmallPoolMemory + Binned3AllocatedLargePoolMemoryWAlignment;

	OutStats.Add(TEXT("TotalAllocated"), TotalAllocated);
	OutStats.Add(TEXT("TotalOSAllocated"), TotalOSAllocated);
#endif
	FMalloc::GetAllocatorStats(OutStats);
}

#if BINNED3_ALLOCATOR_STATS && BINNED3_USE_SEPARATE_VM_PER_POOL
void FMallocBinned3::RecordPoolSearch(uint32 Tests)
{
	Binned3TotalPoolSearches++;
	Binned3TotalPointerTests += Tests;
}
#endif


void FMallocBinned3::DumpAllocatorStats(class FOutputDevice& Ar)
{
#if BINNED3_ALLOCATOR_STATS

	int64 TotalAllocatedSmallPoolMemory = GetTotalAllocatedSmallPoolMemory();

	Ar.Logf(TEXT("FMallocBinned3 Mem report"));
	Ar.Logf(TEXT("Constants.BinnedAllocationGranularity = %d"), int32(OsAllocationGranularity));
	Ar.Logf(TEXT("BINNED3_MAX_SMALL_POOL_SIZE = %d"), int32(BINNED3_MAX_SMALL_POOL_SIZE));
	Ar.Logf(TEXT("MAX_MEMORY_PER_BLOCK_SIZE = %llu"), uint64(MAX_MEMORY_PER_BLOCK_SIZE));
	Ar.Logf(TEXT("Small Pool Allocations: %fmb  (including block size padding)"), ((double)TotalAllocatedSmallPoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Small Pool OS Allocated: %fmb"), ((double)Binned3AllocatedOSSmallPoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Large Pool Requested Allocations: %fmb"), ((double)Binned3AllocatedLargePoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Large Pool OS Allocated: %fmb"), ((double)Binned3AllocatedLargePoolMemoryWAlignment) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("PoolInfo: %fmb"), ((double)Binned3PoolInfoMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Hash: %fmb"), ((double)Binned3HashMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Free Bits: %fmb"), ((double)Binned3FreeBitsMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("TLS: %fmb"), ((double)Binned3TLSMemory) / (1024.0f * 1024.0f));
#if BINNED3_USE_SEPARATE_VM_PER_POOL
	Ar.Logf(TEXT("BINNED3_USE_SEPARATE_VM_PER_POOL is true - VM is Contiguous = %d"), PoolSearchDiv == 0);
	if (PoolSearchDiv)
	{
		Ar.Logf(TEXT("%llu Pointer Searches   %llu Pointer Compares    %llu Compares/Search"), Binned3TotalPoolSearches.Load(), Binned3TotalPointerTests.Load(), Binned3TotalPointerTests.Load() / Binned3TotalPoolSearches.Load());
		uint64 TotalMem = PoolBaseVMPtr[BINNED3_SMALL_POOL_COUNT - 1] + MAX_MEMORY_PER_BLOCK_SIZE - PoolBaseVMPtr[0];
		uint64 MinimumMem = uint64(BINNED3_SMALL_POOL_COUNT) * MAX_MEMORY_PER_BLOCK_SIZE;
		Ar.Logf(TEXT("Percent of gaps in the address range %6.4f  (hopefully < 1, or the searches above will suffer)"), 100.0f * (1.0f - float(MinimumMem) / float(TotalMem)));
	}
#else
	Ar.Logf(TEXT("BINNED3_USE_SEPARATE_VM_PER_POOL is false"));
#endif
	Ar.Logf(TEXT("Total allocated from OS: %fmb"), 
		((double)
			Binned3AllocatedOSSmallPoolMemory + Binned3AllocatedLargePoolMemoryWAlignment + Binned3PoolInfoMemory + Binned3HashMemory + Binned3FreeBitsMemory + Binned3TLSMemory
			) / (1024.0f * 1024.0f));


#if BINNED3_TIME_LARGE_BLOCKS
	Ar.Logf(TEXT("MemoryRangeReserve %d calls %6.3fs    %6.3fus / call"), MemoryRangeReserveTotalCount.Load(), float(MemoryRangeReserveTotalTime.Load()), float(MemoryRangeReserveTotalTime.Load()) * 1000000.0f / float(MemoryRangeReserveTotalCount.Load()));
	Ar.Logf(TEXT("MemoryRangeFree    %d calls %6.3fs    %6.3fus / call"), MemoryRangeFreeTotalCount.Load(), float(MemoryRangeFreeTotalTime.Load()), float(MemoryRangeFreeTotalTime.Load()) * 1000000.0f / float(MemoryRangeFreeTotalCount.Load()));
#endif

#if BINNED3_ALLOCATOR_PER_BIN_STATS
	for (int32 PoolIndex = 0; PoolIndex < BINNED3_SMALL_POOL_COUNT; PoolIndex++)
	{
		
		int64 VM = SmallPoolTables[PoolIndex].UnusedAreaOffsetLow;
		uint32 CommittedBlocks = SmallPoolTables[PoolIndex].BlockOfBlockAllocationBits.CountOnes(SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks);
		uint32 PartialBlocks = SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks - SmallPoolTables[PoolIndex].BlockOfBlockIsExhausted.CountOnes(SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks);
		uint32 FullBlocks = CommittedBlocks - PartialBlocks;
		int64 ComittedVM = VM - (SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks - CommittedBlocks) * SmallPoolTables[PoolIndex].PagesPlatformForBlockOfBlocks * OsAllocationGranularity;

		int64 AveSize = SmallPoolTables[PoolIndex].TotalAllocCount.Load() ? SmallPoolTables[PoolIndex].TotalRequestedAllocSize.Load() / SmallPoolTables[PoolIndex].TotalAllocCount.Load() : 0;
		int64 EstPadWaste = ((SmallPoolTables[PoolIndex].TotalAllocCount.Load() - SmallPoolTables[PoolIndex].TotalFreeCount.Load()) * (PoolIndexToBlockSize(PoolIndex) - AveSize));

		Ar.Logf(TEXT("Pool %2d   Size %6d   Allocs %8lld  Frees %8lld  AveAllocSize %6d  EstPadWaste %4dKB  UsedVM %3dMB  CommittedVM %3dMB  HighSlabs %6d  CommittedSlabs %6d  FullSlabs %6d  PartialSlabs  %6d"), 
			PoolIndex,
			PoolIndexToBlockSize(PoolIndex),
			SmallPoolTables[PoolIndex].TotalAllocCount.Load(),
			SmallPoolTables[PoolIndex].TotalFreeCount.Load(),
			AveSize,
			EstPadWaste / 1024,
			VM / (1024 * 1024),
			ComittedVM / (1024 * 1024),
			SmallPoolTables[PoolIndex].NumEverUsedBlockOfBlocks,
			CommittedBlocks,
			FullBlocks,
			PartialBlocks
			);
	}
#else
#endif

#else
	Ar.Logf(TEXT("Allocator Stats for Binned3 are not in this build set BINNED3_ALLOCATOR_STATS 1 in MallocBinned3.cpp"));
#endif
}
#if !BINNED3_INLINE
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED3
		//#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned3::MallocBinned3)
		#include "FMemory.inl"
	#endif
#endif
#endif