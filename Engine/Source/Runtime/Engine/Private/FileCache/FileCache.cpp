#include "FileCache.h"
#include "Containers/BinaryHeap.h"
#include "Containers/Queue.h"
#include "Containers/LockFreeList.h"
#include "Templates/TypeHash.h"
#include "Misc/ScopeLock.h"
#include "Async/AsyncFileHandle.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"

DECLARE_STATS_GROUP(TEXT("Streaming File Cache"), STATGROUP_SFC, STATCAT_Advanced);

//Use: SCOPE_CYCLE_COUNTER(STAT_SFC_CopyIntoCacheMemcpy);
DECLARE_CYCLE_STAT(TEXT("Process Residency"), STAT_SFC_ProcessResidency, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("Process Completed Requests"), STAT_SFC_ProcessCompletedRequests, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("Read Data"), STAT_SFC_ReadData, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("Request Cache Lines"), STAT_SFC_RequestLines, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("EvictAll"), STAT_SFC_EvictAll, STATGROUP_SFC);

// These below are pretty high throughput and probably should be removed once the system gets more mature
DECLARE_CYCLE_STAT(TEXT("Find Eviction Candidate"), STAT_SFC_FindEvictionCandidate, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("Map Cache"), STAT_SFC_MapCache, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("Read Data Memcpy"), STAT_SFC_ReadDataMemcpy, STATGROUP_SFC);
DECLARE_CYCLE_STAT(TEXT("Copy Into Cache Memcpy"), STAT_SFC_CopyIntoCacheMemcpy, STATGROUP_SFC);

DEFINE_LOG_CATEGORY_STATIC(LogStreamingFileCache, Log, All);

static const int CacheLineSize = 64 * 1024;
static const int UnusedThreshold = 4;
static const int IoBlockSize = 512 * 1024;
static const int CacheLinesPerIOBlock = IoBlockSize / CacheLineSize;

#define NUM_CACHE_BLOCKS 512

// 
// Strongly typed ids to avoid confusion in the code
// 

template <int SetBlockSize, typename Parameter> class StrongBlockIdentifier
{

	static const int InvalidHandle = 0xFFFFFFFF;

public:

	StrongBlockIdentifier() : Id(InvalidHandle)
	{

	}

	explicit StrongBlockIdentifier(int32 SetId) : Id(SetId) {}

	bool IsValid() const
	{
		return Id != InvalidHandle;
	}

	int32 Get() const {
		check(IsValid());
		return Id;
	}

	StrongBlockIdentifier& operator++()
	{
		Id = Id + 1;
		return *this;
	}

	StrongBlockIdentifier& operator--()
	{
		Id = Id - 1;
		return *this;
	}

	StrongBlockIdentifier operator++(int)
	{
		StrongBlockIdentifier Temp(*this);
		operator++();
		return Temp;
	}

	StrongBlockIdentifier operator--(int)
	{
		StrongBlockIdentifier Temp(*this);
		operator--();
		return Temp;
	}

	// Get the offset in the file to read this block
	int64 GetOffset()
	{
		check(IsValid());
		return (int64_t)Id * (int64_t)BlockSize;
	}

	// Get the number of bytes that need to be read for this block
	// takes into account incomplete blocks at the end of the file
	int64 GetSize(int64 FileSize)
	{
		check(IsValid());
		return FMath::Min((int64)BlockSize, FileSize - GetOffset());
	}

	friend uint32 GetTypeHash(const StrongBlockIdentifier<SetBlockSize, Parameter>& Info)
	{
		return GetTypeHash(Info.Id);
	}


	bool operator== (const StrongBlockIdentifier<SetBlockSize, Parameter>&Other) const
	{
		return Id == Other.Id;
	}

	bool operator!= (const StrongBlockIdentifier<SetBlockSize, Parameter>&Other) const
	{
		return !(*this == Other);
	}

	static const int32 BlockSize = SetBlockSize;

private:
	int32 Id;
};

using CacheLineID = StrongBlockIdentifier<CacheLineSize, struct CacheLineStrongType>; // Unique per file handle
using CacheSlotID = StrongBlockIdentifier<CacheLineSize, struct CacheSlotStrongType>; // Unique per cache
using IoBlockID = StrongBlockIdentifier<IoBlockSize, struct IoBlockStrongType>;

class FFileCacheHandle;

// Some terminology:
// A line: A fixed size block of a file on disc that can be brought into the cache
// Slot: A fixed size piece of memory that can contain the data for a certain line in memory

#define MIN_STATUS 0xFFFF			// A status value below this means the cache line is mapped to the cache slot corresponding to the value
#define LOCKED MIN_STATUS + 1		// This status value means the line is currently locked by other code (there is no difference between read/write locking for now)
#define UNAVAILABLE MIN_STATUS + 2	// This status value means the line is currently not available in memory (it was never loaded or has been evicted)

/**
 * Per open file handle this manages the residency of the cache lines pertaining to that file.
 * This class is lock less and can be used from any thread.
 */
class SharedResidency
{
public:

	SharedResidency() {}
	~SharedResidency() {
		// Just do a sanity check that nothing is locked or resident anymore which would mean this instance can't be destroyed now
		// If it's locked -> Something is still using this data and cleary it has to be finised before we can destroy
		// IF it's resident -> If we destroy this class how can the EvictionPolicyManager ever correctly notify us of eviction
		for (int i = 0; i < Lines.Num(); i++)
		{
			checkf(Lines[i] == UNAVAILABLE, TEXT("A cache line was still locked or resident"));
		}
	}

	/**
	 * Specify the number of cache lines to manage.
	 */
	void Initialize(int32 NumLines)
	{
		Lines.SetNumUninitialized(NumLines);
		for (int i = 0; i < NumLines; i++)
		{
			Lines[i] = UNAVAILABLE;
		}
	}

	/**
	 * Return true if the data is resident. Note this is only exactly correct the moment this function runs
	 * If Lock is called immediately after it may still fail because another thread caused it to be eviced in the meantime
	 * If you want to test and hold guaranteed residency you simply have to call Lock and check the result.
	 */
	bool IsResident(CacheLineID Line)
	{
		return Lines[Line.Get()] < MIN_STATUS;
	}

	/**
	 * Try to lock the cache line. If the data is available and can be locked
	 * OutSlotId will contain the cache slot where the data can be found.	
	 * returns true in OutResidencyStatus if the data is resident but currently not available for locking
	 */
	bool Lock(CacheLineID Line, CacheSlotID &OutSlotId, bool &OutResidencyStatus)
	{
		// Just lock the line whatever it's current status is right now
		int32 SlotOrStatus = FPlatformAtomics::InterlockedExchange(&Lines[Line.Get()], LOCKED);
		if (SlotOrStatus < MIN_STATUS)
		{
			OutSlotId = CacheSlotID(SlotOrStatus);
			return true;
		}
		else if (SlotOrStatus == LOCKED)
		{
			// It was already locked not much to do about this...
			OutResidencyStatus = true;
			return false;
		}
		else if (SlotOrStatus == UNAVAILABLE)
		{
			// It was unavailable but now we changed the status to locked it so unlock it again
			int32 OldStatus = FPlatformAtomics::InterlockedExchange(&Lines[Line.Get()], UNAVAILABLE);
			check(OldStatus == LOCKED);
			OutResidencyStatus = false;
			return false;
		}
		else
		{
			checkf(false, TEXT("Invalid status value"));
			return false;
		}
	}

	/**
	 * Unlock a previously locked page. Obviously only valid if the page was previously 
	 * successfully locked
	 */
	void Unlock(CacheLineID Line, CacheSlotID Slot)
	{
		// We successfully locked this slot so put is back in for someone else
		int OldSlotOrStatus = FPlatformAtomics::InterlockedExchange(&Lines[Line.Get()], Slot.Get());
		check(OldSlotOrStatus == LOCKED); // We left it in the locked state so should still be there
	}


	// The memory at slotId now contains valid data for this line so now map it so other threads can start using it....
	void Map(CacheLineID Line, CacheSlotID Slot)
	{
		SCOPE_CYCLE_COUNTER(STAT_SFC_MapCache);

		// We spin here as another thread may temporary lock even UNAVAILABLE lines. This can only be very short and
		// only happens in code this class has control over (in SharedResidency::Lock) as UNAVAILABLE lines can not be locked by user
		// code.
		int OldSlotOrStatus;
		while (true)
		{
			OldSlotOrStatus = FPlatformAtomics::InterlockedCompareExchange(&Lines[Line.Get()], Slot.Get(), UNAVAILABLE);
			if (OldSlotOrStatus < MIN_STATUS)
			{
				// It's already mapped to something this is a coding error
				check(false);
				return;
			}
			else if (OldSlotOrStatus == UNAVAILABLE)
			{
				// all went fine
				return;
			}
			else if (OldSlotOrStatus == LOCKED)
			{
				// Try again until no longer locked...
			}
		}
	}

	// Try to evict the cache line
	bool TryEvict(CacheLineID Line/*, CacheSlotID Slot*/)
	{
		/*// If it's available try to setting it to unmap, if it's not we can't really unmap it now as it's in use
		if (FPlatformAtomics::InterlockedCompareExchange(&pages[pageID], UNAVAILABLE, slotID) != slotID)
		{
			... try evicting something else
				return UNAVAILABLE;
		}

		return slotId->Available right now*/

		CacheSlotID Slot;
		bool Status;

		if (Lock(Line, Slot, Status))
		{
			// Instead of unlocking it we just set it's status to unavailable
			int OldSlotOrStatus = FPlatformAtomics::InterlockedExchange(&Lines[Line.Get()], UNAVAILABLE);
			check(OldSlotOrStatus == LOCKED); // We left it in the locked state so should still be there
			return true;
		}
		else
		{
			return false;
		}
	}

protected:

	TArray<int32> Lines;
};


/////////

// Uniquely identifies a cache line in a file
// Should this be called LineInfo instead...?!?
struct SlotInfo
{
	SlotInfo() : Handle(nullptr), Line(0) {}
	SlotInfo(FFileCacheHandle *SetHandle, CacheLineID SetLine) : Handle(SetHandle), Line(SetLine) {}

	FFileCacheHandle *Handle;
	CacheLineID Line;

	bool operator== (const SlotInfo &Other) const
	{
		return Handle == Other.Handle && Line == Other.Line;
	}

	friend uint32 GetTypeHash(const SlotInfo &Info)
	{
		return HashCombine(::GetTypeHash(Info.Line.Get()), PointerHash(Info.Handle));
	}

	/**
	 * Check if the is currently empty. I.e doesn't contain data for any cache line
	 */
	inline bool IsEmpty()
	{
		return (Handle == nullptr);
	}
};

////////////////

/**
 * Per cache (currently there's only once cache so this is a singleton) an instance of this class is created
 * to manage eviction of items in the cache.
 * 
 * Thread safety: This class is save to use from any thread.
 * Locking: Performance critical functions are lock less others may take locks.
 */
class EvictionPolicyManager
{
public:

	EvictionPolicyManager(int NumSlots) : OutstandingMessages(0)
	{
		checkf(SlotInfos.Num() == 0, TEXT("EvictionPolicyManager was already initialized"));
		SlotInfos.AddDefaulted(NumSlots);
		for (int i = 0; i < NumSlots; i++)
		{
			LruHeap.Add(0, i);
		}
	}

	/**
	 * Notify the manager that a certain slot in the cache was used.
	 * This is most of the time a non blocking lock less operation. 
	 * If to many outstanding touches are queued the list will be flushed nonetheless
	 */
	void SendPageTouched(const SlotInfo &Info)
	{
		// FIXME: save the current time here too? In case it was queued a long time ago
		Messages.Enqueue(Info);
		FPlatformAtomics::InterlockedIncrement(&OutstandingMessages);

		if (OutstandingMessages > 1024)
		{
			FScopeLock Lock(&CriticalSection);
			ProcessMessages();
		}
	}

	/**
	 * Find a suitable cache slot and assign it to the specified cache line.
	 * The returned cache slot id will not be returned by FindEvictionCandidate again
	 * until it is made available for eviction again by calling MakeAvailableForEviction
	 */
	bool FindEvictionCandidate(const SlotInfo &NewOwner, CacheSlotID &OutCacheSlot);

	/**
	 * Make the cache item available for eviction again.
	 */
	void MakeAvailableForEviction(CacheSlotID &CacheSlot)
	{
		FScopeLock Lock(&CriticalSection);
		LruHeap.Add(GetLruKey(), CacheSlot.Get() );
	}

	/**
	 * Evict all items for the specified file.
	 */
	bool EvictAll(FFileCacheHandle *File);

private:

	void ProcessMessages()
	{
		SCOPE_CYCLE_COUNTER(STAT_SFC_ProcessResidency);
		SlotInfo Message;
		while (Messages.Dequeue(Message))
		{
			FPlatformAtomics::InterlockedDecrement(&OutstandingMessages);
			CacheSlotID Slot = ResidencyMap.FindRef(Message);
			if (Slot.IsValid())
			{
				LruHeap.Update(GetLruKey(), Slot.Get());
			}
		}
	}

	int64 GetLruKey()
	{
		return FPlatformTime::Cycles64();
	}

	inline void SanityCheck()
	{
		check(ResidencyMap.Num() <= SlotInfos.Num());
		check(LruHeap.Num() <= (uint32)SlotInfos.Num());
	}

	FCriticalSection CriticalSection;
	TQueue<SlotInfo, EQueueMode::Mpsc> Messages;	// We only ever consume items in the critical section so there really is only a single consumer even if it runs on searate threads...
	TArray<SlotInfo> SlotInfos;						// Cache slot -> SlotInfo lookup
	TMap<SlotInfo, CacheSlotID> ResidencyMap;		// SlotInfo -> Cache slot lookup
	FBinaryHeap<int64, uint32> LruHeap;
	int32 OutstandingMessages;
};

EvictionPolicyManager &GetEvictionPolicy()
{
	static EvictionPolicyManager Manager(NUM_CACHE_BLOCKS);
	return Manager;
}

/**
 * Simply manages cache memory. This is a rather uninteresting helper class
 * the real magic happens elsewhere.
 */
template <class BlockType> class Cache
{
public:
	Cache(int32 SizeInBytes)
	{
		SizeInBlocks = SizeInBytes / BlockType::BlockSize;
		Memory = (uint8 *)FMemory::Malloc(SizeInBytes);
	}

	~Cache()
	{
		FMemory::Free(Memory);
	}

	uint8 *operator[] (BlockType &Block)
	{
		check(Block.Get() < SizeInBlocks);
		return Memory + Block.Get() * BlockType::BlockSize;
	}

protected:
	int32 SizeInBlocks;
	uint8 *Memory;
};

Cache<CacheSlotID> &GetCache()
{
	static Cache<CacheSlotID> TheCache(CacheLineSize * NUM_CACHE_BLOCKS); // Large enough for now?
	return TheCache;
}


///////////////

class FFileCacheHandle : public IFileCacheHandle
{
public:

	FFileCacheHandle();
	virtual ~FFileCacheHandle() override;
	bool Initialize(const FString &FileName);

	//
	// Block helper functions. These are just convenience around basic math.
	// 
	 
	/*
	 * Get the block id that contains the specified offset
	 */
	template<typename BlockIDType> inline BlockIDType GetBlock(int64 Offset)
	{
		checkf(Offset < FileSize, TEXT("Offset beyond end of file"));
		return BlockIDType(FMath::DivideAndRoundDown(Offset, (int64)BlockIDType::BlockSize));
	}

	template<typename BlockIDType> inline int32 GetNumBlocks(int64 Offset, int64 Size)
	{
		BlockIDType FirstBlock = GetBlock<BlockIDType>(Offset);
		BlockIDType LastBlock = GetBlock<BlockIDType>(Offset + Size - 1);// Block containing the last byte
		return (LastBlock.Get() - FirstBlock.Get()) + 1;
	}

	// Returns the offset within the first block covering the byte range to read from
	template<typename BlockIDType> inline size_t GetBlockOffset(int64 Offset)
	{
		return Offset - FMath::DivideAndRoundDown(Offset, (int64)BlockIDType::BlockSize) *  BlockIDType::BlockSize;
	}

	// Returns the size within the first cache line covering the byte range to read
	template<typename BlockIDType> inline size_t GetBlockSize(int64 Offset, int64 Size)
	{
		int64 OffsetInBlock = GetBlockOffset<BlockIDType>(Offset);
		return FMath::Min((int64)(BlockIDType::BlockSize - OffsetInBlock), Size - Offset);
	}

	IFileCacheReadBuffer* ReadData(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority) override;

	void WaitAll() override;

	SharedResidency &GetSharedResidency()
	{
		return Residency;
	}

	CacheLineID GetFirstLine()
	{
		return CacheLineID(0);
	}

	// Returns a cache line past the end of the list
	CacheLineID GetEndLine()
	{
		return CacheLineID(NumSlots);
	}

private:

	int64 FileSize;
	int64 NumSlots;
	IAsyncReadFileHandle *InnerHandle;
	SharedResidency Residency;

	struct CompletedRequest
	{
		CompletedRequest() : Data(nullptr), Offset(0), Size(0) {};
		CompletedRequest(IAsyncReadRequest *SetData, int64 SetOffset, int64 SetSize) : Data(SetData), Offset(SetOffset), Size(SetSize) {};
		IAsyncReadRequest *Data;
		int64 Offset;
		int64 Size;
	};

	TQueue<CompletedRequest> CompletedRequests; // Requests and relevant related info that have been completed
	TArray<CompletedRequest> LiveRequests; // Request that have been created and need to be freed by us

	void ProcessCompletedRequests();

	void RequestLines(TArray<CacheLineID> &SortedLines);
};

///////////////

/**
* Find a suitable cache slot and assign it to the specified cache line.
* FIXME: This gets the lock and usually we want to evict a whole bunch of lines at once when a big read request completes so it
* may be a bit better perf-wise to buffer this?
*/
bool EvictionPolicyManager::FindEvictionCandidate(const SlotInfo &NewOwner, CacheSlotID &OutCacheSlot)
{
	SCOPE_CYCLE_COUNTER(STAT_SFC_FindEvictionCandidate);

	FScopeLock Lock(&CriticalSection);
	SanityCheck();

	// Update the lru with recent info
	// after this function returns this is not necessarily the absolute latest info
	// but good enough
	ProcessMessages();

	int32 NumLruItems = LruHeap.Num();

	// We could stop trying to find an unlocked item here sooner or later... this is rather arbitrary
	// in the end it should be pretty rare tough to find a lot of items locked
	for (int Tries = 0; Tries < NumLruItems; Tries++) // Once we tried all items it means they;re all locked and we're all out of luck
	{
		int32 CacheSlot = LruHeap.Top();
		SlotInfo &Info = SlotInfos[CacheSlot];
		if (Info.IsEmpty())
		{
			// Never allocated before just return it
			//UE_LOG(LogStreamingFileCache, Log, TEXT("Allocating new slot %i"), CacheSlot);
			LruHeap.Pop();
			Info = NewOwner;
			OutCacheSlot = CacheSlotID(CacheSlot);
			ResidencyMap.FindOrAdd(Info) = OutCacheSlot;
			return true;
		}
		else
		{
			// It's already allocated try to evict it
			if (Info.Handle->GetSharedResidency().TryEvict(Info.Line))
			{
				//UE_LOG(LogStreamingFileCache, Log, TEXT("Evicting slot %i (was %llu,%i)"), CacheSlot, (int64)Info.Handle, Info.Line.Get());

				int32 NumRemoved = ResidencyMap.Remove(Info);
				check(NumRemoved > 0);

				LruHeap.Pop();
				Info = NewOwner;
				OutCacheSlot = CacheSlotID(CacheSlot);
				ResidencyMap.FindOrAdd(Info) = OutCacheSlot;
				return true;
			}
			else
			{
				//UE_LOG(LogStreamingFileCache, Log, TEXT("Could not evict slot %i, in use"), CacheSlot);

				// This case should be pretty rare. If we get here it means we took the least recently used item but found it still
				// locked. This probably means the cache is thrashing as "least recently" must still be "pretty recent" or at least
				// recent enough that some code decided to keep it locked.
				// 
				// We can't really solve this here so we mark it as used just now so we won't try to evict it again for a while
				// The loop in the containing scope will then test new least recently used in the hope we will succeed in evicting that.
				// If this loop fails having tried all slots we'll just give up as this means all pages are locked.
				LruHeap.Update(GetLruKey(), CacheSlot);
			}
		}
	}

	return false;
}

/**
* Evict all items for the specified file.
* Returns false if some items could not be evicted (e.g. because they are still locked)
*/
bool EvictionPolicyManager::EvictAll(FFileCacheHandle *File)
{
	SCOPE_CYCLE_COUNTER(STAT_SFC_EvictAll);

	FScopeLock Lock(&CriticalSection);

	// Update the lru with recent info
	// after this function returns this is not necessarily the absolute latest info
	// but good enough
	ProcessMessages();
	SanityCheck();

	bool AllOk = true;

	for (CacheLineID Line = File->GetFirstLine(); Line != File->GetEndLine(); Line++)
	{
		SlotInfo Info(File, Line);
		CacheSlotID Slot = ResidencyMap.FindRef(Info);
		if (Slot.IsValid())
		{
			// It's already allocated try to evict it
			if (!Info.Handle->GetSharedResidency().TryEvict(Line))
			{
				AllOk = false;
			}
			else
			{
				ResidencyMap.Remove(Info);
				// Make it a prime candiate for reuse
				LruHeap.Update(0, Slot.Get());
				SlotInfos[Slot.Get()] = SlotInfo();
			}
		}
	}

	return AllOk;
}

///////////////

FFileCacheHandle::~FFileCacheHandle()
{
	if (InnerHandle)
	{
		WaitAll();
		check(LiveRequests.Num() == 0);
		bool result = GetEvictionPolicy().EvictAll(this);
		check(result);
		delete InnerHandle;
	}
}

FFileCacheHandle::FFileCacheHandle() : FileSize(0), InnerHandle(nullptr)
{

}

bool FFileCacheHandle::Initialize(const FString &FileName)
{
	InnerHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*FileName);
	if (InnerHandle == nullptr)
	{
		return false;
	}

	// Get the file size
	IAsyncReadRequest *SizeRequest = InnerHandle->SizeRequest();
	if (SizeRequest == nullptr)
	{
		delete InnerHandle;
		InnerHandle = nullptr;
		return false;
	}
	
	SizeRequest->WaitCompletion();
	FileSize = SizeRequest->GetSizeResults();
	delete SizeRequest;

	if (FileSize < 0)
	{
		delete InnerHandle;
		InnerHandle = nullptr;
		return false;
	}

	NumSlots = FMath::DivideAndRoundUp(FileSize, (int64)CacheLineSize);
	Residency.Initialize((int32)NumSlots);

	return true;
}

IFileCacheReadBuffer *FFileCacheHandle::ReadData(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority)
{
	SCOPE_CYCLE_COUNTER(STAT_SFC_ReadData);

	TArray<CacheLineID> ToRequest;
	TMap<CacheLineID, CacheSlotID> ResidentPages;
	TArray<CacheSlotID> LineIndexToMap;

	checkf(Offset < FileSize, TEXT("Read beyond end of file"));
	checkf(Offset+BytesToRead <= FileSize, TEXT("Read beyond end of file"));

	// Lock any pages for reading
	int32 NumCacheLines = GetNumBlocks<CacheLineID>(Offset, BytesToRead);
	LineIndexToMap.AddDefaulted(NumCacheLines); // We could use a CacheLineID->CacheSlotId map here but this is probably lower overhead assuming most blocks are resident
	bool AllLocked = true;

	// This will fill the cache with any completed requests.
	// FIXME: Note this may take some time to copy all the data to the cache
	// a possible future avenue may be filling the cache from a separate job.
	ProcessCompletedRequests();

	CacheLineID Line = GetBlock<CacheLineID>(Offset);
	for (int i=0; i<NumCacheLines; i++, Line++)
	{
		GetEvictionPolicy().SendPageTouched(SlotInfo(this, Line));

		CacheSlotID Slot;
		bool AlreadyResident;

		if (Residency.Lock(Line, Slot, AlreadyResident))
		{
			LineIndexToMap[i] = Slot;
		}
		else
		{
			AllLocked = false;

			if (!AlreadyResident)
			{
				ToRequest.Add(Line);
			}
		}
	}

	if (ToRequest.Num() > 0)
	{
		RequestLines(ToRequest);
	}

	FAllocatedFileCacheReadBuffer *ResultBuffer = nullptr;
	if (AllLocked)
	{
		//UE_LOG(LogStreamingFileCache, Log, TEXT("Read hit cache %llu, %llu"), Offset, BytesToRead);

		ResultBuffer = new FAllocatedFileCacheReadBuffer(BytesToRead);
		int64 CurrentOffset = Offset;
		int32 RelativeOffset = 0;

		// Patch together the individual lines in one continuous block to return
		CacheLineID Id = GetBlock<CacheLineID>(Offset);
		for (int i = 0;  i < NumCacheLines; Id++, i++)
		{
			CacheSlotID Slot = LineIndexToMap[i];
			check(Slot.IsValid());
			uint8 *CacheSlotMemory = GetCache()[Slot];
			size_t LineOffset = GetBlockOffset<CacheLineID>(CurrentOffset);
			size_t LineSize = GetBlockSize<CacheLineID>(CurrentOffset, Offset+BytesToRead);
			{
				// It's a good thing if this is about the same as STAT_SFC_ReadData. This means the real cost is the
				// memcpy not the anything else in the cache system.
				SCOPE_CYCLE_COUNTER(STAT_SFC_ReadDataMemcpy);
				FPlatformMemory::Memcpy((uint8 *)ResultBuffer->GetData() + RelativeOffset, CacheSlotMemory + LineOffset, LineSize);
			}
			CurrentOffset += LineSize;
			RelativeOffset += LineSize;
		}
	}

	// Unlock anything we locked
	CacheLineID Id = GetBlock<CacheLineID>(Offset);
	for (int i = 0; i < NumCacheLines; Id++, i++)
	{
		if (LineIndexToMap[i].IsValid())
		{
			Residency.Unlock(Id, LineIndexToMap[i]);
		}
	}

	return ResultBuffer;
}

void FFileCacheHandle::RequestLines(TArray<CacheLineID> &SortedLines)
{
	SCOPE_CYCLE_COUNTER(STAT_SFC_RequestLines);

	// Figure out the data we should read for this request. 
	// We always read data in larger chunks but the chunks are not aligned to this larger block size
	// so we have a some of freedom on how to choose the chunks to load
	// for now we just align to the first and then read until the last to be fine tuned later...
	
	for (int i = 0; i < SortedLines.Num(); i++)
	{
		int64 Offset = SortedLines[i].GetOffset();
		int64 Limit = FMath::Min(Offset + IoBlockSize, FileSize);
		int64 BytesToRead = Limit - Offset;

		// Skip over cache lines covering the block we will schedule now
		while (i < SortedLines.Num() && SortedLines[i].GetOffset() < Limit)
		{
			i++;
		}

		// Trim the request at the end with cache lines which are already resident
		CacheLineID LastBlock = GetBlock<CacheLineID>(Limit - 1);
		while (Residency.IsResident(LastBlock))
		{
			LastBlock--;
		}

		int64 TrimmedLimit = LastBlock.GetOffset() + LastBlock.GetSize(FileSize);
		BytesToRead = TrimmedLimit - Offset;

		if (TrimmedLimit < Limit)
		{
			//UE_LOG(LogStreamingFileCache, Log, TEXT("Trimmed  %llu kb from read request"), ((Limit-TrimmedLimit)/1024) );
		}
		
		// Check if we already got a load request doing for this exact range
		bool AlreadyLoading = false;
		for (int r = 0; r < LiveRequests.Num(); r++)
		{
			if (LiveRequests[r].Offset == Offset && LiveRequests[r].Size == BytesToRead)
			{
				AlreadyLoading = true;
				break;
			}
		}

		if (!AlreadyLoading && LiveRequests.Num() < 32 )
		{
			FAsyncFileCallBack ReadCallbackFunction = [this, Offset, BytesToRead](bool bWasCancelled, IAsyncReadRequest* Request)
			{
				// Per mail discussion with gil 30/05/17:
				// "You are not supposed to do anything that takes time in the callback and something like acquiring a separate lock could easily serialize what should be parallel operation."
				// "But if locks are bad, what can you do in a callback function? Start a task, trigger an event, change a thread safe counter, push something on a lock free list...stuff like that."
				CompletedRequests.Enqueue(CompletedRequest(Request, Offset, BytesToRead));
			};

			//UE_LOG(LogStreamingFileCache, Log, TEXT("Scheduling read %llu, %llu"), Offset, BytesToRead);

			IAsyncReadRequest *Request = InnerHandle->ReadRequest(Offset, BytesToRead, AIOP_Normal, &ReadCallbackFunction);
			LiveRequests.Add(CompletedRequest(Request, Offset, BytesToRead));
		}
	}
}

void FFileCacheHandle::ProcessCompletedRequests()
{
	SCOPE_CYCLE_COUNTER(STAT_SFC_ProcessCompletedRequests);

	CompletedRequest Completed;
	while (CompletedRequests.Dequeue(Completed))
	{
		//UE_LOG(LogStreamingFileCache, Log, TEXT("Processing completed read %llu, %llu"), Completed.Offset, Completed.Size);

		int NumLines = GetNumBlocks<CacheLineID>(Completed.Offset, Completed.Size);
		CacheLineID Line = GetBlock<CacheLineID>(Completed.Offset);
		uint8* ReadData = Completed.Data->GetReadResults(); // We now own the memory in the ReadData pointer

		for (int i = 0; i < NumLines; i++, Line++)
		{
			// If it's resident we don't have to do anything it was just double read and we discard the data read this time...
			if (!Residency.IsResident(Line))
			{
				CacheSlotID Slot;
				// FindEvictionCandidate and MakeAvailableForEviction both get the lock
				// on ever call this should probably be batched. To avoid keeping the lock
				// too long an approach where we do a lock-find-unlock followed by memcpy followed by lock-makeavail-unlock
				// could be followed.
				if (GetEvictionPolicy().FindEvictionCandidate( SlotInfo(this, Line), Slot))
				{
					size_t RelativeOffset = (size_t)(Line.GetOffset() - Completed.Offset);
					{
						SCOPE_CYCLE_COUNTER(STAT_SFC_CopyIntoCacheMemcpy);
						FPlatformMemory::Memcpy(GetCache()[Slot], ReadData + RelativeOffset, Line.GetSize(FileSize));
					}
					Residency.Map(Line, Slot);

					// Make it available for eviction again hmm is this usuefull to do this so soon
					// this means that some requests may 'in theory' evict tiles that have just been loaded
					// as part of this request (however unlikely as they are at the front of the lru)
					GetEvictionPolicy().MakeAvailableForEviction(Slot);
				}
				else
				{
					// Hmm we throw away the data this is really bad so we should log this...
				}
			}
		}

		// Free the request now we're fully done with it
		int NumRemoved = 0;
		for (int i = 0; i < LiveRequests.Num(); i++)
		{
			if (LiveRequests[i].Data == Completed.Data)
			{
				check(LiveRequests[i].Offset == Completed.Offset);
				check(LiveRequests[i].Size == Completed.Size);
				NumRemoved++;
				LiveRequests.RemoveAt(i);
				break;
			}
		}

		checkf(NumRemoved > 0, TEXT("Completed request was not in LiveRequests list"));
		FMemory::Free(ReadData);
		delete Completed.Data;
	}
}

void FFileCacheHandle::WaitAll()
{
	for (int i = 0; i < LiveRequests.Num(); i++)
	{
		LiveRequests[i].Data->WaitCompletion();
	}
	ProcessCompletedRequests();
	check(LiveRequests.Num() == 0);
}

IFileCacheHandle *IFileCacheHandle::CreateFileCacheHandle(const FString &FileName)
{
	FFileCacheHandle *Handle = new FFileCacheHandle();
	if (!Handle->Initialize(FileName))
	{
		delete Handle;
		return nullptr;
	}
	return Handle;
}
