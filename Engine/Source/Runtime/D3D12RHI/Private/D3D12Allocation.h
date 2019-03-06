// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Allocation.h: A Collection of allocators
=============================================================================*/

#pragma once
#include "D3D12Resources.h"

// Segregated free list texture allocator
// Description:
// - Binned read-only texture allocation based on sizes
// Suggestions:
// - You can check memory wastage using "stat d3d12rhi" in a dev build
// - Tune d3d12.ReadOnlyTextureAllocator.MinPoolSize/MinNumToPool/MaxPoolSize
//   according to video memory budget
// - Memory overhead is slightly over 200 MB in internal tests but consider
//   adjusting above cvars or disabling if it fails your use case
// - The purpose of this allocator is pooling texture allocations because
//   creating committed resources is slow on PC. But if committed resource
//   creation ever becomes fast, there is no need for this allocator
// Internal test statistics (11/6/2018):
// - Average read-only texture alloc time reduced from ~420 us to ~72 us
// - Number of allocations over 1 ms reduced from 8145 to 504 (from 14.76% to
//   0.92%) over a 17 minutes 11 seconds game replay
// - Peak memory overhead was ~207 MB (from 2666.58 MB to 2874.08 MB)
// TODO:
// - Defragmentation support
#define D3D12RHI_SEGREGATED_TEXTURE_ALLOC (PLATFORM_WINDOWS)
#define D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE (!(UE_BUILD_TEST || UE_BUILD_SHIPPING))

class FD3D12SegList;

class FD3D12ResourceAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:

	FD3D12ResourceAllocator(FD3D12Device* ParentDevice,
		FRHIGPUMask VisibleNodes,
		const FString& Name,
		D3D12_HEAP_TYPE HeapType,
		D3D12_RESOURCE_FLAGS Flags,
		uint32 MaxSizeForPooling);

	~FD3D12ResourceAllocator();

	// Any allocation larger than this just gets straight up allocated (i.e. not pooled).
	// These large allocations should be infrequent so the CPU overhead should be minimal
	const uint32 MaximumAllocationSizeForPooling;
	D3D12_RESOURCE_FLAGS ResourceFlags;

protected:

	const FString DebugName;

	bool Initialized;

	const D3D12_HEAP_TYPE HeapType;

	FCriticalSection CS;

#if defined(UE_BUILD_DEBUG)
	uint32 SpaceUsed;
	uint32 InternalFragmentation;
	uint32 NumBlocksInDeferredDeletionQueue;
	uint32 PeakUsage;
	uint32 FailedAllocationSpace;
#endif
};

//-----------------------------------------------------------------------------
//	Buddy Allocator
//-----------------------------------------------------------------------------
// Allocates blocks from a fixed range using buddy allocation method.
// Buddy allocation allows reasonably fast allocation of arbitrary size blocks
// with minimal fragmentation and provides efficient reuse of freed ranges.
// When a block is de-allocated an attempt is made to merge it with it's 
// neighbour (buddy) if it is contiguous and free.
// Based on reference implementation by MSFT: billkris

// Unfortunately the api restricts the minimum size of a placed buffer resource to 64k
#define MIN_PLACED_BUFFER_SIZE (64 * 1024)
#define D3D_BUFFER_ALIGNMENT (64 * 1024)

#if defined(UE_BUILD_DEBUG)
#define INCREASE_ALLOC_COUNTER(A, B) (A = A + B);
#define DECREASE_ALLOC_COUNTER(A, B) (A = A - B);
#else
#define INCREASE_ALLOC_COUNTER(A, B)
#define DECREASE_ALLOC_COUNTER(A, B)
#endif

enum eBuddyAllocationStrategy
{
	// This strategy uses Placed Resources to sub-allocate a buffer out of an underlying ID3D12Heap.
	// The benefit of this is that each buffer can have it's own resource state and can be treated
	// as any other buffer. The downside of this strategy is the API limitation which enforces
	// the minimum buffer size to 64k leading to large internal fragmentation in the allocator
	kPlacedResourceStrategy,
	// The alternative is to manually sub-allocate out of a single large buffer which allows block
	// allocation granularity down to 1 byte. However, this strategy is only really valid for buffers which
	// will be treated as read-only after their creation (i.e. most Index and Vertex buffers). This 
	// is because the underlying resource can only have one state at a time.
	kManualSubAllocationStrategy
};

class FD3D12BuddyAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12BuddyAllocator(FD3D12Device* ParentDevice,
		FRHIGPUMask VisibleNodes,
		const FString& Name,
		eBuddyAllocationStrategy InAllocationStrategy,
		D3D12_HEAP_TYPE HeapType,
		D3D12_HEAP_FLAGS HeapFlags,
		D3D12_RESOURCE_FLAGS Flags,
		uint32 MaxSizeForPooling,
		uint32 InAllocatorID,
		uint32 InMaxBlockSize,
		uint32 InMinBlockSize = MIN_PLACED_BUFFER_SIZE);

	bool TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);

	void Deallocate(FD3D12ResourceLocation& ResourceLocation);

	void Initialize();

	void Destroy();

	void CleanUpAllocations();

	void DumpAllocatorStats(class FOutputDevice& Ar);

	void ReleaseAllResources();

	void Reset();

	inline bool IsEmpty()
	{
		return FreeBlocks[MaxOrder].Num() == 1;
	}

	inline uint32 GetTotalSizeUsed() const { return TotalSizeUsed; }
	inline uint64 GetAllocationOffsetInBytes(const FD3D12BuddyAllocatorPrivateData& AllocatorPrivateData) const { return uint64(AllocatorPrivateData.Offset) * MinBlockSize; }

	inline FD3D12Heap* GetBackingHeap() { check(AllocationStrategy == kPlacedResourceStrategy); return BackingHeap.GetReference(); }

	inline bool IsOwner(FD3D12ResourceLocation& ResourceLocation)
	{
		return ResourceLocation.GetAllocator() == (FD3D12BaseAllocatorType*)this;
	}

protected:
	const uint32 MaxBlockSize;
	const uint32 MinBlockSize;
	const D3D12_HEAP_FLAGS HeapFlags;
	const eBuddyAllocationStrategy AllocationStrategy;
	const uint32 AllocatorID;

	TRefCountPtr<FD3D12Resource> BackingResource;
	TRefCountPtr<FD3D12Heap> BackingHeap;

private:
	struct RetiredBlock
	{
		FD3D12Resource* PlacedResource;
		uint64 FrameFence;
		FD3D12BuddyAllocatorPrivateData Data;
#if defined(UE_BUILD_DEBUG)
		// Padding is only need in debug builds to keep track of internal fragmentation for stats.
		uint32 Padding;
#endif
	};

	TArray<RetiredBlock> DeferredDeletionQueue;
	TArray<TSet<uint32>> FreeBlocks;
	uint32 MaxOrder;
	uint32 TotalSizeUsed;

	bool HeapFullMessageDisplayed;

	inline uint32 SizeToUnitSize(uint32 size) const
	{
		return (size + (MinBlockSize - 1)) / MinBlockSize;
	}

	inline uint32 UnitSizeToOrder(uint32 size) const
	{
		unsigned long Result;
		_BitScanReverse(&Result, size + size - 1); // ceil(log2(size))
		return Result;
	}

	inline uint32 GetBuddyOffset(const uint32 &offset, const uint32 &size)
	{
		return offset ^ size;
	}

	uint32 OrderToUnitSize(uint32 order) const { return ((uint32)1) << order; }
	uint32 AllocateBlock(uint32 order);
	void DeallocateBlock(uint32 offset, uint32 order);

	bool CanAllocate(uint32 size, uint32 alignment);

	void DeallocateInternal(RetiredBlock& Block);

	void Allocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);
};

//-----------------------------------------------------------------------------
//	Multi-Buddy Allocator
//-----------------------------------------------------------------------------
// Builds on top of the Buddy Allocator but covers some of it's deficiencies by
// managing multiple buddy allocator instances to better match memory usage over
// time.

class FD3D12MultiBuddyAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12MultiBuddyAllocator(FD3D12Device* ParentDevice,
		FRHIGPUMask VisibleNodes,
		const FString& Name,
		eBuddyAllocationStrategy InAllocationStrategy,
		D3D12_HEAP_TYPE HeapType,
		D3D12_HEAP_FLAGS InHeapFlags,
		D3D12_RESOURCE_FLAGS Flags,
		uint32 MaxSizeForPooling,
		uint32 InAllocatorID,
		uint32 InMaxBlockSize,
		uint32 InMinBlockSize = MIN_PLACED_BUFFER_SIZE);
	~FD3D12MultiBuddyAllocator();

	bool TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);

	void Deallocate(FD3D12ResourceLocation& ResourceLocation);

	void Initialize();

	void Destroy();

	void CleanUpAllocations();

	void DumpAllocatorStats(class FOutputDevice& Ar);

	void ReleaseAllResources();

	void Reset();

	const eBuddyAllocationStrategy GetAllocationStrategy() const { return AllocationStrategy; }

protected:
	const eBuddyAllocationStrategy AllocationStrategy;
	const D3D12_HEAP_FLAGS HeapFlags;
	const uint32 MaxBlockSize;
	const uint32 MinBlockSize;
	const uint32 AllocatorID;

	FD3D12BuddyAllocator* CreateNewAllocator();

	TArray<FD3D12BuddyAllocator*> Allocators;
};

//-----------------------------------------------------------------------------
//	Bucket Allocator
//-----------------------------------------------------------------------------
// Resources are allocated from buckets, which are just a collection of resources of a particular size.
// Blocks can be an entire resource or a sub allocation from a resource.
class FD3D12BucketAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12BucketAllocator(FD3D12Device* ParentDevice,
		FRHIGPUMask VisibleNodes,
		const FString& Name,
		D3D12_HEAP_TYPE HeapType,
		D3D12_RESOURCE_FLAGS Flags,
		uint64 InBlockRetentionFrameCount);

	bool TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);

	void Deallocate(FD3D12ResourceLocation& ResourceLocation);

	void Initialize();

	void Destroy();

	void CleanUpAllocations();

	void DumpAllocatorStats(class FOutputDevice& Ar);

	void ReleaseAllResources();

	void Reset();

private:

	static uint32 FORCEINLINE BucketFromSize(uint32 size, uint32 bucketShift)
	{
		uint32 bucket = FMath::CeilLogTwo(size);
		bucket = bucket < bucketShift ? 0 : bucket - bucketShift;
		return bucket;
	}

	static uint32 FORCEINLINE BlockSizeFromBufferSize(uint32 bufferSize, uint32 bucketShift)
	{
		const uint32 minSize = 1 << bucketShift;
		return bufferSize > minSize ? FMath::RoundUpToPowerOfTwo(bufferSize) : minSize;
	}

#if SUB_ALLOCATED_DEFAULT_ALLOCATIONS
	static const uint32 MIN_HEAP_SIZE = 256 * 1024;
#else
	static const uint32 MIN_HEAP_SIZE = 64 * 1024;
#endif

	static const uint32 BucketShift = 6;
	static const uint32 NumBuckets = 22; // bucket resource sizes range from 64 to 2^28 

	FThreadsafeQueue<FD3D12BlockAllocatorPrivateData> AvailableBlocks[NumBuckets];
	FThreadsafeQueue<FD3D12BlockAllocatorPrivateData> ExpiredBlocks;
	TArray<FD3D12Resource*> SubAllocatedResources;// keep a list of the sub-allocated resources so that they may be cleaned up

	// This frame count value helps makes sure that we don't delete resources too soon. If resources are deleted too soon,
	// we can get in a loop the heap allocator will be constantly deleting and creating resources every frame which
	// results in CPU stutters. DynamicRetentionFrameCount was tested and set to a value that appears to be adequate for
	// creating a stable state on the Infiltrator demo.
	const uint64 BlockRetentionFrameCount;
};

#ifdef USE_BUCKET_ALLOCATOR
typedef FD3D12BucketAllocator FD3D12AllocatorType;
#else
typedef FD3D12MultiBuddyAllocator FD3D12AllocatorType;
#endif

//-----------------------------------------------------------------------------
//	FD3D12DynamicHeapAllocator
//-----------------------------------------------------------------------------
// This is designed for allocation of scratch memory such as temporary staging buffers
// or shadow buffers for dynamic resources.
class FD3D12DynamicHeapAllocator : public FD3D12AdapterChild, public FD3D12MultiNodeGPUObject
{
public:

	FD3D12DynamicHeapAllocator(FD3D12Adapter* InParent, FD3D12Device* InParentDevice, const FString& InName, eBuddyAllocationStrategy InAllocationStrategy,
		uint32 InMaxSizeForPooling,
		uint32 InMaxBlockSize,
		uint32 InMinBlockSize);

	void Init();

	// Allocates <size> bytes from the end of an available resource heap.
	void* AllocUploadResource(uint32 size, uint32 alignment, FD3D12ResourceLocation& ResourceLocation);

	void CleanUpAllocations();

	void Destroy();

private:

	FD3D12AllocatorType Allocator;
};

//-----------------------------------------------------------------------------
//	FD3D12DefaultBufferPool
//-----------------------------------------------------------------------------
class FD3D12DefaultBufferPool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12DefaultBufferPool(FD3D12Device* InParent, FD3D12AllocatorType* InAllocator);
	~FD3D12DefaultBufferPool() { delete Allocator; }

	// Grab a buffer from the available buffers or create a new buffer if none are available
	void AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, uint32 InUsage, FD3D12ResourceLocation& ResourceLocation, uint32 Alignment, const TCHAR* Name);

	void CleanUpAllocations();

private:
	FD3D12AllocatorType* Allocator;
};

// FD3D12DefaultBufferAllocator
//
class FD3D12DefaultBufferAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12DefaultBufferAllocator(FD3D12Device* InParent, FRHIGPUMask VisibleNodes);

	// Grab a buffer from the available buffers or create a new buffer if none are available
	void AllocDefaultResource(const D3D12_RESOURCE_DESC& pDesc, uint32 InUsage, FD3D12ResourceLocation& ResourceLocation, uint32 Alignment, const TCHAR* Name);
	void FreeDefaultBufferPools();
	void CleanupFreeBlocks();

private:

	enum class EBufferPool : uint32
	{
		None,
		SRV,
		UAV,

		Count,
	};

	void InitializeAllocator(EBufferPool PoolIndex, D3D12_RESOURCE_FLAGS Flags);
	FD3D12DefaultBufferPool* DefaultBufferPools[(uint32)EBufferPool::Count];

	inline EBufferPool GetBufferPool(D3D12_RESOURCE_FLAGS Flags) const
	{
		if (Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
			return EBufferPool::UAV;
		else if (Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
			return EBufferPool::None;
		else
			return EBufferPool::SRV;
	}

	bool BufferIsWriteable(const D3D12_RESOURCE_DESC& Desc)
	{
		const bool bDSV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
		const bool bRTV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0;
		const bool bUAV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;

		// Buffer Depth Stencils are invalid
		check(bDSV == false);
		const bool bWriteable = bDSV || bRTV || bUAV;
		return bWriteable;
	}
};

//-----------------------------------------------------------------------------
//	Fast Allocation
//-----------------------------------------------------------------------------

struct FD3D12FastAllocatorPage
{
	FD3D12FastAllocatorPage()
		: PageSize(0)
		, NextFastAllocOffset(0)
		, FastAllocData(nullptr)
		, FrameFence(0) {};

	FD3D12FastAllocatorPage(uint32 Size)
		: PageSize(Size)
		, NextFastAllocOffset(0)
		, FastAllocData(nullptr)
		, FrameFence(0) {};

	void Reset()
	{
		NextFastAllocOffset = 0;
	}

	const uint32 PageSize;
	TRefCountPtr<FD3D12Resource> FastAllocBuffer;
	uint32 NextFastAllocOffset;
	void* FastAllocData;
	uint64 FrameFence;
};

class FD3D12FastAllocatorPagePool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12FastAllocatorPagePool(FD3D12Device* Parent, FRHIGPUMask VisibiltyMask, D3D12_HEAP_TYPE InHeapType, uint32 Size);

	FD3D12FastAllocatorPagePool(FD3D12Device* Parent, FRHIGPUMask VisibiltyMask, const D3D12_HEAP_PROPERTIES& InHeapProperties, uint32 Size);

	FD3D12FastAllocatorPage* RequestFastAllocatorPage();
	void ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page);
	void CleanupPages(uint64 FrameLag);

	inline uint32 GetPageSize() const { return PageSize; }

	inline D3D12_HEAP_TYPE GetHeapType() const { return HeapProperties.Type; }
	inline bool IsCPUWritable() const { return ::IsCPUWritable(GetHeapType(), &HeapProperties); }

	void Destroy();

protected:

	const uint32 PageSize;
	const D3D12_HEAP_PROPERTIES HeapProperties;

	TArray<FD3D12FastAllocatorPage*> Pool;
};

class FD3D12FastAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12FastAllocator(FD3D12Device* Parent, FRHIGPUMask VisibiltyMask, D3D12_HEAP_TYPE InHeapType, uint32 PageSize);
	FD3D12FastAllocator(FD3D12Device* Parent, FRHIGPUMask VisibiltyMask, const D3D12_HEAP_PROPERTIES& InHeapProperties, uint32 PageSize);

	template<typename LockType>
	void* Allocate(uint32 Size, uint32 Alignment, class FD3D12ResourceLocation* ResourceLocation);

	template<typename LockType>
	void Destroy();

	template<typename LockType>
	void CleanupPages(uint64 FrameLag);

protected:
	FD3D12FastAllocatorPagePool PagePool;

	FD3D12FastAllocatorPage* CurrentAllocatorPage;

	FCriticalSection CS;
};

class FD3D12AbstractRingBuffer
{
public:
	FD3D12AbstractRingBuffer(uint64 BufferSize)
		: Fence(nullptr)
		, Size(BufferSize)
		, Head(BufferSize)
		, Tail(0)
		, LastFence(0)
	{}

	static const uint64 FailedReturnValue = uint64(-1);

	inline void Reset(uint64 NewSize)
	{
		Size = NewSize;
		Head = Size;
		Tail = 0;
		LastFence = 0;
		OutstandingAllocs.Empty();
	}

	inline void SetFence(FD3D12Fence* InFence)
	{
		Fence = InFence;
		LastFence = 0;
	}

	inline const uint64 GetSpaceLeft() const { return Head - Tail; }

#if 0  // used to detect problems with fencing 
	void GetOverwritableBlocks(uint64& Block1Start, uint64& Block1Size, uint64& Block2Start, uint64& Block2Size) const
	{
		check(Head >= Tail);
		check(Size >= Head - Tail);
		uint64 Used = Size - (Head - Tail);

		if (Used == Size)
		{
			Block1Start = 0;
			Block1Size = 0;

			Block2Start = 0;
			Block2Size = 0;
		}
		else
		{
			uint64 PhysicalTail = Tail % Size;

			if (PhysicalTail <= Used)
			{
				// there is only one block, it starts at PhysicalTail
				Block1Start = 0;
				Block1Size = 0;

				Block2Start = PhysicalTail;
				Block2Size = Size - Used;
			}
			else
			{
				Block1Start = 0;
				Block1Size = PhysicalTail - Used;

				Block2Start = PhysicalTail;
				Block2Size = Size - PhysicalTail;
			}
		}
	}
#endif // used to detect problems with fencing 

	inline uint64 Allocate(uint64 Count)
	{
		{
			const uint64 LastCompletedFence = Fence->GetLastCompletedFenceFast();
			// If progress has been made since we were here last
			if (LastCompletedFence > LastFence)
			{
				LastFence = LastCompletedFence;

				for (auto It = OutstandingAllocs.CreateIterator(); It; ++It)
				{
					if (It.Key() < LastCompletedFence)
					{
						Head += It.Value();
						It.RemoveCurrent();
					}
				}
			}
		}

		uint64 ReturnValue = FailedReturnValue;

		uint64 PhysicalTail = Tail % Size;

		if (PhysicalTail + Count > Size)
		{
			// Force the wrap around by simply allocating the difference
			uint64 Padding = Allocate(Size - PhysicalTail);
			if (Padding == FailedReturnValue)
			{
				return FailedReturnValue;
			}

			PhysicalTail = Tail % Size;
		}

		if (Tail + Count < Head)
		{
			ReturnValue = PhysicalTail;
			Tail += Count;
			OutstandingAllocs.FindOrAdd(Fence->GetCurrentFence()) += Count;
		}

		return ReturnValue;
	}

private:
	FD3D12Fence* Fence;
	uint64 Size;
	uint64 Head;
	uint64 Tail;
	uint64 LastFence;

	TMap<uint64, uint64, TInlineSetAllocator<16> > OutstandingAllocs;
};

class FD3D12FastConstantAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12FastConstantAllocator(FD3D12Device* Parent, FRHIGPUMask VisibiltyMask, uint32 InPageSize);

	void Init();

#if USE_STATIC_ROOT_SIGNATURE
	void* Allocate(uint32 Bytes, class FD3D12ResourceLocation& OutLocation, FD3D12ConstantBufferView* OutCBView);
#else
	void* Allocate(uint32 Bytes, class FD3D12ResourceLocation& OutLocation);
#endif


private:
	void ReallocBuffer();

	FD3D12ResourceLocation UnderlyingResource;

	uint32 PageSize;
	FD3D12AbstractRingBuffer RingBuffer;
};

//-----------------------------------------------------------------------------
//	FD3D12SegListAllocator
//-----------------------------------------------------------------------------

class FD3D12SegHeap : public FD3D12Heap
{
private:
	FD3D12SegHeap(
		FD3D12Device* Parent,
		FRHIGPUMask VisibileNodeMask,
		ID3D12Heap* NewHeap,
		uint64 HeapSize,
		FD3D12SegList* Owner,
		uint32 Idx) :
		FD3D12Heap(Parent, VisibileNodeMask),
		OwnerList(Owner),
		ArrayIdx(Idx),
		FirstFreeOffset(0)
	{
		this->SetHeap(NewHeap);
		BeginTrackingResidency(HeapSize);
	}

	virtual ~FD3D12SegHeap() = default;

	FD3D12SegHeap(const FD3D12SegHeap&) = delete;
	FD3D12SegHeap(FD3D12SegHeap&&) = delete;

	FD3D12SegHeap& operator=(const FD3D12SegHeap&) = delete;
	FD3D12SegHeap& operator=(FD3D12SegHeap&&) = delete;

	bool IsArrayIdxValid() const { return ArrayIdx >= 0; }

	bool IsFull(uint32 HeapSize) const
	{
		check(FirstFreeOffset <= HeapSize);
		return !FreeBlockOffsets.Num() && FirstFreeOffset == HeapSize;
	}

	bool IsEmpty(uint32 BlockSize) const
	{
		return FreeBlockOffsets.Num() * BlockSize == FirstFreeOffset;
	}

	// @return - In-heap offset of the allocated block
	uint32 AllocateBlock(uint32 BlockSize)
	{
		if (!FreeBlockOffsets.Num())
		{
			uint32 Ret = FirstFreeOffset;
			FirstFreeOffset += BlockSize;
			return Ret;
		}
		else
		{
			return FreeBlockOffsets.Pop();
		}
	}

	TArray<uint32> FreeBlockOffsets;
	FD3D12SegList* OwnerList;
	int32 ArrayIdx;
	uint32 FirstFreeOffset;

	friend class FD3D12SegList;
	friend class FD3D12SegListAllocator;
};

class FD3D12SegList
{
private:
	FD3D12SegList(uint32 InBlockSize, uint32 InHeapSize)
		: BlockSize(InBlockSize)
		, HeapSize(InHeapSize)
#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
		, TotalBytesAllocated(0)
#endif
	{
		check(!(HeapSize % BlockSize));
		check(HeapSize / BlockSize > 1);
	}

	~FD3D12SegList()
	{
		FScopeLock Lock(&CS);
		check(!!BlockSize);
		check(!!HeapSize);
		for (const auto& Heap : FreeHeaps)
		{
			check(Heap->GetRefCount() == 1);
		}
	}

	FD3D12SegList(const FD3D12SegList&) = delete;
	FD3D12SegList(FD3D12SegList&&) = delete;

	FD3D12SegList& operator=(const FD3D12SegList&) = delete;
	FD3D12SegList& operator=(FD3D12SegList&&) = delete;

	// @return - In-heap offset of the allocated block
	uint32 AllocateBlock(
		FD3D12Device* Device,
		FRHIGPUMask VisibleNodeMask,
		D3D12_HEAP_TYPE HeapType,
		D3D12_HEAP_FLAGS HeapFlags,
		TRefCountPtr<FD3D12SegHeap>& OutHeap)
	{
		FScopeLock Lock(&CS);
		uint32 Offset;

		if (!!FreeHeaps.Num())
		{
			const int32 LastHeapIdx = FreeHeaps.Num() - 1;
			OutHeap = FreeHeaps[LastHeapIdx];
			Offset = OutHeap->AllocateBlock(BlockSize);
			check(Offset <= HeapSize - BlockSize);
			if (OutHeap->IsFull(HeapSize))
			{
				// Heap is full
				OutHeap->ArrayIdx = INDEX_NONE;
				FreeHeaps.RemoveAt(LastHeapIdx);
			}
		}
		else
		{
			OutHeap = CreateBackingHeap(Device, VisibleNodeMask, HeapType, HeapFlags);
			Offset = OutHeap->AllocateBlock(BlockSize);
#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
			TotalBytesAllocated += HeapSize;
#endif
		}
		return Offset;
	}

	// Deferred deletion is handled by FD3D12SegListAllocator
	void FreeBlock(FD3D12SegHeap* RESTRICT Heap, uint32 Offset)
	{
		FScopeLock Lock(&CS);

		check(!(Offset % BlockSize));
		check(Offset <= HeapSize - BlockSize);
		check(this == Heap->OwnerList);

		const bool bFull = Heap->IsFull(HeapSize);
		Heap->FreeBlockOffsets.Add(Offset);

		if (bFull)
		{
			// Heap was full
			check(!Heap->IsArrayIdxValid());
			Heap->ArrayIdx = FreeHeaps.Add(Heap);
		}
		else if (Heap->IsEmpty(BlockSize))
		{
			// Heap is empty
			check(Heap->GetRefCount() == 1);
			check(Heap->IsArrayIdxValid());
			check(FreeHeaps.Num() > Heap->ArrayIdx);
			const int32 Idx = Heap->ArrayIdx;
			const int32 LastIdx = FreeHeaps.Num() - 1;
			FreeHeaps.RemoveAtSwap(Idx);
			if (Idx != LastIdx)
			{
				FreeHeaps[Idx]->ArrayIdx = Idx;
			}
#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
			TotalBytesAllocated -= HeapSize;
#endif
		}
	}

	FD3D12SegHeap* CreateBackingHeap(
		FD3D12Device* Parent,
		FRHIGPUMask VisibleNodeMask,
		D3D12_HEAP_TYPE HeapType,
		D3D12_HEAP_FLAGS HeapFlags);

	TArray<TRefCountPtr<FD3D12SegHeap>> FreeHeaps;
	FCriticalSection CS;
	uint32 BlockSize;
	uint32 HeapSize;

#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
	uint64 TotalBytesAllocated;
#endif

	friend class FD3D12SegListAllocator;
};

#if !D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
static_assert(sizeof(FD3D12SegList) <= 64, "Try to make it fit in a single cacheline");
#endif

class FD3D12SegListAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	static constexpr uint32 InvalidOffset = 0xffffffff;

	FD3D12SegListAllocator(
		FD3D12Device* Parent,
		FRHIGPUMask VisibilityMask,
		D3D12_HEAP_TYPE InHeapType,
		D3D12_HEAP_FLAGS InHeapFlags,
		uint32 InMinPoolSize,
		uint32 InMinNumToPool,
		uint32 InMaxPoolSize);

	~FD3D12SegListAllocator()
	{
		check(!SegLists.Num());
		check(!FenceValues.Num());
		check(!DeferredDeletionQueue.Num());
#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
		check(!TotalBytesRequested);
#endif
	}

	FD3D12SegListAllocator(const FD3D12SegListAllocator&) = delete;
	FD3D12SegListAllocator(FD3D12SegListAllocator&&) = delete;

	FD3D12SegListAllocator& operator=(const FD3D12SegListAllocator&) = delete;
	FD3D12SegListAllocator& operator=(FD3D12SegListAllocator&&) = delete;

	uint32 Allocate(uint32 SizeInBytes, uint32 Alignment, TRefCountPtr<FD3D12SegHeap>& OutHeap)
	{
		check(!(Alignment & Alignment - 1));

		const uint32 BlockSize = CalculateBlockSize(SizeInBytes, Alignment);
		if (ShouldPool(BlockSize))
		{
			FD3D12SegList* SegList;
			{
				FRWScopeLock Lock(SegListsRWLock, SLT_ReadOnly);
				FD3D12SegList** SegListPtr = SegLists.Find(BlockSize);
				SegList = !!SegListPtr ? *SegListPtr : nullptr;
			}
			if (!SegList)
			{
				const uint32 HeapSize = CalculateHeapSize(BlockSize);
				{
					FRWScopeLock Lock(SegListsRWLock, SLT_Write);
					FD3D12SegList** SegListPtr = SegLists.Find(BlockSize);
					SegList = !!SegListPtr ?
						*SegListPtr :
						SegLists.Add(BlockSize, new FD3D12SegList(BlockSize, HeapSize));
				}
			}
			check(!!SegList);
			uint32 Ret = SegList->AllocateBlock(
				this->GetParentDevice(),
				this->GetVisibilityMask(),
				HeapType,
				HeapFlags,
				OutHeap);
			check(Ret != InvalidOffset);
#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
			TotalBytesRequested += SizeInBytes;
#endif
			return Ret;
		}
		OutHeap = nullptr;
		return InvalidOffset;
	}

	void Deallocate(FD3D12Resource* PlacedResource, uint32 Offset, uint32 SizeInBytes);

	void CleanUpAllocations();

	void Destroy();

	bool GetMemoryStats(uint64& OutTotalAllocated, uint64& OutTotalUnused) const
	{
#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
		FScopeLock LockCS(&DeferredDeletionCS);
		FRWScopeLock LockRW(SegListsRWLock, SLT_Write);

		OutTotalAllocated = 0;
		for (const auto& Pair : SegLists)
		{
			const FD3D12SegList* SegList = Pair.Value;
			OutTotalAllocated += SegList->TotalBytesAllocated;
		}
		OutTotalUnused = OutTotalAllocated - TotalBytesRequested.Load(EMemoryOrder::Relaxed);
		return true;
#else
		return false;
#endif
	}

private:
	struct FRetiredBlock
	{
		// FD3D12Resource knows which heap it is from
		FD3D12Resource* PlacedResource;
		uint32 Offset;
		uint32 ResourceSize;

		FRetiredBlock(
			FD3D12Resource* InResource,
			uint32 InOffset,
			uint32 InResourceSize) :
			PlacedResource(InResource),
			Offset(InOffset),
			ResourceSize(InResourceSize)
		{}
	};

	static constexpr uint32 CalculateBlockSize(uint32 SizeInBytes, uint32 Alignment)
	{
		return (SizeInBytes + Alignment - 1) & ~(Alignment - 1);
	}

	bool ShouldPool(uint32 BlockSize) const
	{
		return BlockSize * 2 <= MaxPoolSize;
	}

	uint32 CalculateHeapSize(uint32 BlockSize) const
	{
		check(MinPoolSize + BlockSize - 1 > MinPoolSize);
		uint32 NumPooled = (MinPoolSize + BlockSize - 1) / BlockSize;
		if (NumPooled < MinNumToPool)
		{
			NumPooled = MinNumToPool;
		}
		const uint32 MaxNumPooled = MaxPoolSize / BlockSize;
		if (NumPooled > MaxNumPooled)
		{
			NumPooled = MaxNumPooled;
		}
		check(NumPooled > 1);
		check(NumPooled * BlockSize >= MinPoolSize);
		check(NumPooled * BlockSize <= MaxPoolSize);
		return NumPooled * BlockSize;
	}

	template <typename AllocX, typename AllocY>
	void FreeRetiredBlocks(TArray<TArray<FRetiredBlock, AllocX>, AllocY>& PendingDeletes);

	TMap<uint32, FD3D12SegList*> SegLists;
	TArray<uint64> FenceValues;
	TArray<TArray<FRetiredBlock>> DeferredDeletionQueue;
	mutable FRWLock SegListsRWLock;
	mutable FCriticalSection DeferredDeletionCS;
	const D3D12_HEAP_TYPE HeapType;
	const D3D12_HEAP_FLAGS HeapFlags;
	const uint32 MinPoolSize;
	const uint32 MinNumToPool;
	const uint32 MaxPoolSize;

#if D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
	TAtomic<uint64> TotalBytesRequested;
#endif
};

//-----------------------------------------------------------------------------
//	FD3D12TextureAllocator
//-----------------------------------------------------------------------------

#if D3D12RHI_SEGREGATED_TEXTURE_ALLOC
class FD3D12TextureAllocatorPool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12TextureAllocatorPool(FD3D12Device* Device, FRHIGPUMask VisibilityNode);

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, uint8 UEFormat, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, const TCHAR* Name);

	void CleanUpAllocations()
	{
		ReadOnlyTexturePool.CleanUpAllocations();
	}

	void Destroy()
	{
		ReadOnlyTexturePool.Destroy();
	}

	bool GetMemoryStats(uint64& OutTotalAllocated, uint64& OutTotalUnused) const
	{
		return ReadOnlyTexturePool.GetMemoryStats(OutTotalAllocated, OutTotalUnused);
	}

private:
	FD3D12SegListAllocator ReadOnlyTexturePool;
};
#else
class FD3D12TextureAllocator : public FD3D12MultiBuddyAllocator
{
public:
	FD3D12TextureAllocator(FD3D12Device* Device, FRHIGPUMask VisibleNodes, const FString& Name, uint32 HeapSize, D3D12_HEAP_FLAGS Flags);

	~FD3D12TextureAllocator();

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, const TCHAR* Name);
};

class FD3D12TextureAllocatorPool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12TextureAllocatorPool(FD3D12Device* Device, FRHIGPUMask VisibilityNode);

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, uint8 UEFormat, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, const TCHAR* Name);

	void CleanUpAllocations() { ReadOnlyTexturePool.CleanUpAllocations(); }

	void Destroy() { ReadOnlyTexturePool.Destroy(); }

private:
	FD3D12TextureAllocator ReadOnlyTexturePool;
};
#endif
